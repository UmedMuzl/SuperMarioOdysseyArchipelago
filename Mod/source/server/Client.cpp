#include "server/Client.hpp"
#include "al/layout/SimpleLayoutAppearWaitEnd.h"
#include "al/util/LiveActorUtil.h"
#include "game/SaveData/SaveDataAccessFunction.h"
#include "heap/seadHeapMgr.h"
#include "logger.hpp"
#include "packets/Packet.h"
#include "server/hns/HideAndSeekMode.hpp"

SEAD_SINGLETON_DISPOSER_IMPL(Client)

typedef void (Client::*ClientThreadFunc)(void);

/**
 * @brief Construct a new Client:: Client object
 * 
 * @param bufferSize defines the maximum amount of puppets the client can handle
 */
Client::Client() {

    mHeap = sead::ExpHeap::create(0x50000, "ClientHeap", sead::HeapMgr::instance()->getCurrentHeap(), 8, sead::Heap::cHeapDirection_Forward, false);

    sead::ScopedCurrentHeapSetter heapSetter(
        mHeap);  // every new call after this will use ClientHeap instead of SequenceHeap

    mReadThread = new al::AsyncFunctorThread("ClientReadThread", al::FunctorV0M<Client*, ClientThreadFunc>(this, &Client::readFunc), 0, 0x1000, {0});

    mKeyboard = new Keyboard(nn::swkbd::GetRequiredStringBufferSize());

    mSocket = new SocketClient("SocketClient", mHeap, this);
    
    mPuppetHolder = new PuppetHolder(maxPuppets);

    for (size_t i = 0; i < MAXPUPINDEX; i++)
    {
        mPuppetInfoArr[i] = new PuppetInfo();

        sprintf(mPuppetInfoArr[i]->puppetName, "Puppet%zu", i);
    }

    strcpy(mDebugPuppetInfo.puppetName, "PuppetDebug");

    mConnectCount = 0;

    curCollectedShines.fill(-1);

    collectedShineCount = 0;

    mShineArray.allocBuffer(100, nullptr); // max of 100 shine actors in buffer

    nn::account::GetLastOpenedUser(&mUserID);

    nn::account::Nickname playerName;
    nn::account::GetNickname(&playerName, mUserID);
    Logger::setLogName(playerName.name);  // set Debug logger name to player name

    mUsername = playerName.name;

    apChatLine1 = "";
    apChatLine2 = "";
    apChatLine3 = "";
    
    worldScenarios.fill(1);
    worldPayCounts.fill(-1);

    collectedShines.fill(0);
    collectedOutfits.fill(0);
    collectedStickers.fill(0);
    collectedSouvenirs.fill(0);
    collectedCaptures.fill(0);

    shopCapTextReplacements.fill({254, 255, 255, 255});
    shopClothTextReplacements.fill({254, 255, 255, 255});
    shopStickerTextReplacements.fill({254, 255, 255, 255});
    shopGiftTextReplacements.fill({254, 255, 255, 255});
    shopMoonTextReplacements.fill({254, 255, 255, 255});


    apGameNames.fill(sead::WFixedSafeString<40>());
    apSlotNames.fill(sead::WFixedSafeString<40>());
    apItemNames.fill(sead::WFixedSafeString<40>());

    mUserID.print();

    Logger::log("Player Name: %s\n", playerName.name);

    Logger::log("%s Build Number: %s\n", playerName.name, TOSTRING(BUILDVERSTR));

}

/**
 * @brief initializes client class using initInfo obtained from StageScene::init
 * 
 * @param initInfo init info used to create layouts used by client
 */
void Client::init(al::LayoutInitInfo const &initInfo, GameDataHolderAccessor holder) {

    mConnectStatus = new (mHeap) al::SimpleLayoutAppearWaitEnd("", "SaveMessage", initInfo, 0, false);
    al::setPaneString(mConnectStatus, "TxtSave", u"Connecting to Server.", 0);
    al::setPaneString(mConnectStatus, "TxtSaveSh", u"Connecting to Server.", 0);

    mUIMessage = new (mHeap) al::WindowConfirmWait("ServerWaitConnect", "WindowConfirmWait", initInfo);
    mUIMessage->setTxtMessage(u"a");
    mUIMessage->setTxtMessageConfirm(u"b");

    mHolder = holder;

    startThread();

    Logger::log("Heap Free Size: %f/%f\n", mHeap->getFreeSize() * 0.001f, mHeap->getSize() * 0.001f);
}

/**
 * @brief starts client read thread
 * 
 * @return true if read thread was succesfully started
 * @return false if read thread was unable to start, or thread was already started.
 */
bool Client::startThread() {
    if(mReadThread->isDone() ) {
        mReadThread->start();
        Logger::log("Read Thread Successfully Started.\n");
        return true;
    }else {
        Logger::log("Read Thread has already started! Or other unknown reason.\n");
        return false;
    }
}

/**
 * @brief starts a connection using client's TCP socket class, pulling up the software keyboard for user inputted IP if save file does not have one saved.
 * 
 * @return true if successful connection to server
 * @return false if connection was unable to establish
 */
bool Client::startConnection() {

    bool isNeedSave = false;

    bool isOverride = al::isPadHoldZL(-1);

    if (mServerIP.isEmpty() || isOverride) {
        mKeyboard->setHeaderText(u"Save File does not contain an IP!");
        mKeyboard->setSubText(u"Please set a Server IP Below.");
        mServerIP = "127.0.0.1";
        Client::openKeyboardIP();
        isNeedSave = true;
    }

    if (!mServerPort || isOverride) {
        mKeyboard->setHeaderText(u"Save File does not contain a port!");
        mKeyboard->setSubText(u"Please set a Server Port Below.");
        mServerPort = 1027;
        Client::openKeyboardPort();
        isNeedSave = true;
    }

    if (isNeedSave) {
        SaveDataAccessFunction::startSaveDataWrite(mHolder.mData);
    }

    // This might need to be changed later
    // Repeat connection attempts until successful
    while (!mIsConnectionActive) {
        mIsConnectionActive = mSocket->init(mServerIP.cstr(), mServerPort).isSuccess();
        nn::os::YieldThread();
        nn::os::SleepThread(nn::TimeSpan::FromNanoSeconds(2500000000));
    }

    if (mIsConnectionActive) {

        Logger::log("Succesful Connection. Waiting to receive init packet.\n");

        bool waitingForInitPacket = true;
        // wait for client init packet

        while (waitingForInitPacket) {

            Packet *curPacket = mSocket->tryGetPacket();

            if (curPacket) {
                
                if (curPacket->mType == PacketType::CLIENTINIT) {
                    InitPacket* initPacket = (InitPacket*)curPacket;

                    Logger::log("Server Max Player Size: %d\n", initPacket->maxPlayers);

                    maxPuppets = initPacket->maxPlayers - 1;

                    waitingForInitPacket = false;
                }

                mHeap->free(curPacket);

            } else {
                Logger::log("Receive failed! Stopping Connection.\n");
                mIsConnectionActive = false;
                waitingForInitPacket = false;
            }
        }


    }

    return mIsConnectionActive;
}

/**
 * @brief Opens up OS's software keyboard in order to change the currently used server IP.
 * @returns whether or not a new IP has been defined and needs to be saved.
 */
bool Client::openKeyboardIP() {

    if (!sInstance) {
        Logger::log("Static Instance is null!\n");
        return false;
    }

    // opens swkbd with the initial text set to the last saved IP
    sInstance->mKeyboard->openKeyboard(
        sInstance->mServerIP.cstr(), [](nn::swkbd::KeyboardConfig& config) {
            config.keyboardMode = nn::swkbd::KeyboardMode::ModeASCII;
            config.textMaxLength = MAX_HOSTNAME_LENGTH;
            config.textMinLength = 1;
            config.isUseUtf8 = true;
            config.inputFormMode = nn::swkbd::InputFormMode::OneLine;
        });

    hostname prevIp = sInstance->mServerIP;

    while (true) {
        if (sInstance->mKeyboard->isThreadDone()) {
            if(!sInstance->mKeyboard->isKeyboardCancelled())
                sInstance->mServerIP = sInstance->mKeyboard->getResult();
            break;
        }
        nn::os::YieldThread(); // allow other threads to run
    }

    bool isFirstConnect = prevIp != sInstance->mServerIP;

    sInstance->mSocket->setIsFirstConn(isFirstConnect);

    return isFirstConnect;
}

/**
 * @brief Opens up OS's software keyboard in order to change the currently used server port.
 * @returns whether or not a new port has been defined and needs to be saved.
 */
bool Client::openKeyboardPort() {

    if (!sInstance) {
        Logger::log("Static Instance is null!\n");
        return false;
    }

    // opens swkbd with the initial text set to the last saved port
    char buf[6];
    nn::util::SNPrintf(buf, 6, "%u", sInstance->mServerPort);

    sInstance->mKeyboard->openKeyboard(buf, [](nn::swkbd::KeyboardConfig& config) {
        config.keyboardMode = nn::swkbd::KeyboardMode::ModeNumeric;
        config.textMaxLength = 5;
        config.textMinLength = 2;
        config.isUseUtf8 = true;
        config.inputFormMode = nn::swkbd::InputFormMode::OneLine;
    });

    int prevPort = sInstance->mServerPort;

    while (true) {
        if (sInstance->mKeyboard->isThreadDone()) {
            if(!sInstance->mKeyboard->isKeyboardCancelled())
                sInstance->mServerPort = ::atoi(sInstance->mKeyboard->getResult());
            break;
        }
        nn::os::YieldThread(); // allow other threads to run
    }

    bool isFirstConnect = prevPort != sInstance->mServerPort;

    sInstance->mSocket->setIsFirstConn(isFirstConnect);

    return isFirstConnect;
}


void Client::showUIMessage(const char16_t* msg) {
    if (!sInstance) {
        return;
    }

    sInstance->mUIMessage->setTxtMessageConfirm(msg);

    al::hidePane(sInstance->mUIMessage, "Page01");  // hide A button prompt

    if (!sInstance->mUIMessage->mIsAlive) {
        sInstance->mUIMessage->appear();

        sInstance->mUIMessage->playLoop();
    }

    al::startAction(sInstance->mUIMessage, "Confirm", "State");
}

void Client::hideUIMessage() {
    if (!sInstance) {
        return;
    }

    sInstance->mUIMessage->tryEnd();
}

/**
 * @brief main thread function for read thread, responsible for processing packets from server
 * 
 */
void Client::readFunc() {

    if (waitForGameInit) {
        nn::os::YieldThread(); // sleep the thread for the first thing we do so that game init can finish
        nn::os::SleepThread(nn::TimeSpan::FromSeconds(2));
        waitForGameInit = false;
    }

    mConnectStatus->appear();

    al::startAction(mConnectStatus, "Loop", "Loop");

    if (!startConnection()) {

        Logger::log("Failed to Connect to Server.\n");

        nn::os::SleepThread(nn::TimeSpan::FromNanoSeconds(250000000)); // sleep active thread for 0.25 seconds

        mConnectStatus->end();
                
        return;
    }

    nn::os::SleepThread(nn::TimeSpan::FromNanoSeconds(500000000)); // sleep for 0.5 seconds to let connection layout fully show (probably should find a better way to do this)

    mConnectStatus->end();

    while(mIsConnectionActive) {

        Packet *curPacket = mSocket->tryGetPacket();  // will block until a packet has been received, or socket disconnected

        if (curPacket) {

            switch (curPacket->mType)
            {
            case PacketType::PLAYERINF:
                updatePlayerInfo((PlayerInf*)curPacket);
                break;
            case PacketType::GAMEINF:
                updateGameInfo((GameInf*)curPacket);
                break;
            case PacketType::HACKCAPINF:
                updateHackCapInfo((HackCapInf *)curPacket);
                break;
            case PacketType::CAPTUREINF:                    
                updateCaptureInfo((CaptureInf*)curPacket);
                break;
            case PacketType::PLAYERCON:
                updatePlayerConnect((PlayerConnect*)curPacket);

                // Send relevant info packets when another client is connected

                if (lastGameInfPacket != emptyGameInfPacket) {
                    // Assume game packets are empty from first connection
                    if (lastGameInfPacket.mUserID != mUserID)
                        lastGameInfPacket.mUserID = mUserID;
                    mSocket->send(&lastGameInfPacket);
                }

                // No need to send player/costume packets if they're empty
                if (lastPlayerInfPacket.mUserID == mUserID)
                    mSocket->send(&lastPlayerInfPacket);
                if (lastCostumeInfPacket.mUserID == mUserID)
                    mSocket->send(&lastCostumeInfPacket);
                if (lastTagInfPacket.mUserID == mUserID)
                    mSocket->send(&lastTagInfPacket);
                if (lastCaptureInfPacket.mUserID == mUserID)
                    mSocket->send(&lastCaptureInfPacket);

                break;
            case PacketType::COSTUMEINF:
                updateCostumeInfo((CostumeInf*)curPacket);
                break;
            case PacketType::CHECK:
                receiveCheck((Check*)curPacket);
                break;
            case PacketType::ITEMCOLL:
                updateItems((ItemCollect*)curPacket);
                break;
            case PacketType::FILLERCOLL:
                updateFiller((FillerCollect*)curPacket);
                break;
            case PacketType::APCHATMESSAGE:
                updateChatMessages((ArchipelagoChatMessage*)curPacket);
                break;
            case PacketType::SLOTDATA:
                updateSlotData((SlotData*)curPacket);
                break;
            case PacketType::APINFO:
                addApInfo((ApInfo*)curPacket);
                break;
            case PacketType::SHOPREPLACE:
                updateShopReplace((ShopReplacePacket*)curPacket);
                break;
            case PacketType::UNLOCKWORLD:
                updateWorlds((UnlockWorld*)curPacket);
                break;
            case PacketType::PROGRESS:
                updateProgress((ProgressWorld*)curPacket);
                break;
            case PacketType::DEATHLINK:
                receiveDeath((Deathlink*)curPacket);
                break;
            case PacketType::PLAYERDC:
                Logger::log("Received Player Disconnect!\n");
                curPacket->mUserID.print();
                disconnectPlayer((PlayerDC*)curPacket);
                break;
            case PacketType::TAGINF:
                updateTagInfo((TagInf*)curPacket);
                break;
            case PacketType::CHANGESTAGE:
                sendToStage((ChangeStagePacket*)curPacket);
                break;
            case PacketType::CLIENTINIT: {
                InitPacket* initPacket = (InitPacket*)curPacket;
                Logger::log("Server Max Player Size: %d\n", initPacket->maxPlayers);
                maxPuppets = initPacket->maxPlayers - 1;
                break;
            }
			case PacketType::UDPINIT: {
				UdpInit* initPacket = (UdpInit*)curPacket;
				Logger::log("Received udp init packet from server\n");
				
				sInstance->mSocket->setPeerUdpPort(initPacket->port);
				sendUdpHolePunch();
				sendUdpInit();
				
				break;
			}
			case PacketType::HOLEPUNCH: 
				sendUdpHolePunch();
				break;
            default:
                Logger::log("Discarding Unknown Packet Type.\n");
                break;
            }

            mHeap->free(curPacket);

        }else { // if false, socket has errored or disconnected, so restart the connection
            Logger::log("Client Socket Encountered an Error, restarting connection! Errno: 0x%x\n", mSocket->socket_errno);
        }

    }

    Logger::log("Client Read Thread ending.\n");
}

/**
 * @brief sends player info packet to current server
 * 
 * @param player pointer to current player class, used to get translation, animation, and capture data
 */
void Client::sendPlayerInfPacket(const PlayerActorBase *playerBase, bool isYukimaru) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    
    if(!playerBase) {
        Logger::log("Error: Null Player Reference\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    PlayerInf *packet = new PlayerInf();
    packet->mUserID = sInstance->mUserID;

    packet->playerPos = al::getTrans(playerBase);

    al::calcQuat(&packet->playerRot,
                 playerBase);  // calculate rotation based off pose instead of using quat rotation

    if (!isYukimaru) { 
        
        PlayerActorHakoniwa* player = (PlayerActorHakoniwa*)playerBase;

        for (size_t i = 0; i < 6; i++)
        {
            packet->animBlendWeights[i] = player->mPlayerAnimator->getBlendWeight(i);
        }

        const char *hackName = player->mHackKeeper->getCurrentHackName();

        if (hackName != nullptr) {

            sInstance->isClientCaptured = true;

            const char* actName = al::getActionName(player->mHackKeeper->currentHackActor);

            if (actName) {
                packet->actName = PlayerAnims::FindType(actName);
                packet->subActName = PlayerAnims::Type::Unknown;
                //strcpy(packet.actName, actName); 
            } else {
                packet->actName = PlayerAnims::Type::Unknown;
                packet->subActName = PlayerAnims::Type::Unknown;
            }
        } else {
            packet->actName = PlayerAnims::FindType(player->mPlayerAnimator->mAnimFrameCtrl->getActionName());
            packet->subActName = PlayerAnims::FindType(player->mPlayerAnimator->curSubAnim.cstr());

            sInstance->isClientCaptured = false;
        }

    } else {

        // TODO: implement YukimaruRacePlayer syncing
        
        for (size_t i = 0; i < 6; i++)
        {
            packet->animBlendWeights[i] = 0;
        }

        sInstance->isClientCaptured = false;

        packet->actName = PlayerAnims::Type::Unknown;
        packet->subActName = PlayerAnims::Type::Unknown;
    }
    
    if(sInstance->lastPlayerInfPacket != *packet) {
        sInstance->lastPlayerInfPacket = *packet; // deref packet and store in client memory
        sInstance->mSocket->queuePacket(packet);
    } else {
        sInstance->mHeap->free(packet); // free packet if we're not using it
    }

}

/**
 * @brief sends info related to player's cap actor to server
 * 
 * @param hackCap pointer to cap actor, used to get translation, animation, and state info
 */
void Client::sendHackCapInfPacket(const HackCap* hackCap) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    bool isFlying = hackCap->isFlying();

    // if cap is in flying state, send packet as often as this function is called
    if (isFlying) {
        HackCapInf *packet = new HackCapInf();
        packet->mUserID = sInstance->mUserID;
        packet->capPos = al::getTrans(hackCap);

        packet->isCapVisible = isFlying;

        packet->capQuat.x = hackCap->mJointKeeper->mJointRot.x;
        packet->capQuat.y = hackCap->mJointKeeper->mJointRot.y;
        packet->capQuat.z = hackCap->mJointKeeper->mJointRot.z;
        packet->capQuat.w = hackCap->mJointKeeper->mSkew;

        strcpy(packet->capAnim, al::getActionName(hackCap));

        sInstance->mSocket->queuePacket(packet);

        sInstance->isSentHackInf = true;

    } else if (sInstance->isSentHackInf) { // if cap is not flying, check to see if previous function call sent a packet, and if so, send one final packet resetting cap data.
        HackCapInf *packet = new HackCapInf();
        packet->mUserID = sInstance->mUserID;
        packet->isCapVisible = false;
        packet->capPos = sead::Vector3f::zero;
        packet->capQuat = sead::Quatf::unit;
        sInstance->mSocket->queuePacket(packet);
        sInstance->isSentHackInf = false;
    }
}

/**
 * @brief 
 * Sends both stage info and player 2D info to the server.
 * @param player 
 * @param holder 
 */
void Client::sendGameInfPacket(const PlayerActorHakoniwa* player, GameDataHolderAccessor holder) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    GameInf *packet = new GameInf();
    packet->mUserID = sInstance->mUserID;

    if (player) {
        packet->is2D = player->mDimKeeper->is2DModel;
    } else {
        packet->is2D = false;
    }

    packet->scenarioNo = holder.mData->mGameDataFile->getScenarioNo();

    strcpy(packet->stageName, GameDataFunction::getCurrentStageName(holder));

    if (*packet != sInstance->lastGameInfPacket && *packet != sInstance->emptyGameInfPacket) {
        sInstance->lastGameInfPacket = *packet;
        sInstance->mSocket->queuePacket(packet);
    } else {
        sInstance->mHeap->free(packet); // free packet if we're not using it
    }
}

/**
 * @brief 
 * Sends only stage info to the server.
 * @param holder 
 */
void Client::sendGameInfPacket(GameDataHolderAccessor holder) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    GameInf *packet = new GameInf();
    packet->mUserID = sInstance->mUserID;

    packet->is2D = false;

    packet->scenarioNo = holder.mData->mGameDataFile->getScenarioNo();

    strcpy(packet->stageName, GameDataFunction::getCurrentStageName(holder));

    if (*packet != sInstance->emptyGameInfPacket) {
        sInstance->lastGameInfPacket = *packet;
        sInstance->mSocket->queuePacket(packet);
    }
}

/**
 * @brief 
 * 
 */
void Client::sendTagInfPacket() {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    HideAndSeekMode* hsMode = GameModeManager::instance()->getMode<HideAndSeekMode>();

    if (!GameModeManager::instance()->isMode(GameMode::HIDEANDSEEK)) {
        Logger::log("State is not Hide and Seek!\n");
        return;
    }

    HideAndSeekInfo* curInfo = GameModeManager::instance()->getInfo<HideAndSeekInfo>();

    TagInf *packet = new TagInf();

    packet->mUserID = sInstance->mUserID;

    packet->isIt = hsMode->isPlayerIt() && hsMode->isModeActive();

    packet->minutes = curInfo->mHidingTime.mMinutes;
    packet->seconds = curInfo->mHidingTime.mSeconds;
    packet->updateType = static_cast<TagUpdateType>(TagUpdateType::STATE | TagUpdateType::TIME);

    sInstance->mSocket->queuePacket(packet);

    sInstance->lastTagInfPacket = *packet;
}

/**
 * @brief 
 * 
 * @param body 
 * @param cap 
 */
void Client::sendCostumeInfPacket(const char* body, const char* cap) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    if (!strcmp(body, "") && !strcmp(cap, "")) { return; }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    CostumeInf *packet = new CostumeInf(body, cap);
    packet->mUserID = sInstance->mUserID;
    sInstance->lastCostumeInfPacket = *packet;
    sInstance->mSocket->queuePacket(packet);
}

/**
 * @brief 
 * 
 * @param player 
 */
void Client::sendCaptureInfPacket(const PlayerActorHakoniwa* player) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    if (sInstance->isClientCaptured && !sInstance->isSentCaptureInf) {
        CaptureInf *packet = new CaptureInf();
        packet->mUserID = sInstance->mUserID;
        strcpy(packet->hackName, tryConvertName(player->mHackKeeper->getCurrentHackName()));
        sInstance->mSocket->queuePacket(packet);
        sInstance->lastCaptureInfPacket = *packet;
        sInstance->isSentCaptureInf = true;
    } else if (!sInstance->isClientCaptured && sInstance->isSentCaptureInf) {
        CaptureInf *packet = new CaptureInf();
        packet->mUserID = sInstance->mUserID;
        strcpy(packet->hackName, "");
        sInstance->mSocket->queuePacket(packet);
        sInstance->lastCaptureInfPacket = *packet;
        sInstance->isSentCaptureInf = false;
    }
}

/**
 * @brief
 */
void Client::resendInitPackets() {
    // CostumeInfPacket
    if (lastCostumeInfPacket.mUserID == mUserID) {
        mSocket->queuePacket(&lastCostumeInfPacket);
    }

    // GameInfPacket
    if (lastGameInfPacket != emptyGameInfPacket) {
        mSocket->queuePacket(&lastGameInfPacket);
    }

    // TagInfPacket
    if (lastTagInfPacket.mUserID == mUserID) {
        mSocket->queuePacket(&lastTagInfPacket);
    }

    // CaptureInfPacket
    if (lastCaptureInfPacket.mUserID == mUserID) {
        mSocket->queuePacket(&lastCaptureInfPacket);
    }
}

/**
 * @brief 
 * 
 * @param shineID 
 */
//void Client::sendShineCollectPacket(int shineID) {
//
//    if (!sInstance) {
//        Logger::log("Static Instance is Null!\n");
//        return;
//    }
//
//    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
//
//    if(sInstance->lastCollectedShine != shineID) {
//        ShineCollect *packet = new ShineCollect();
//        packet->mUserID = sInstance->mUserID;
//        packet->shineId = shineID;
//
//        sInstance->lastCollectedShine = shineID;
//
//        sInstance->mSocket->queuePacket(packet);
//    }
//}

/**
 * @brief
 *
 * @param itemName
 */
void Client::sendItemCollectPacket(char* itemName, int itemType) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

  /*  if (!strcmp(itemName, "")) {
        return;
    }*/

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    ItemCollect* packet = new ItemCollect(itemName, itemType);
    packet->mUserID = sInstance->mUserID;

    sInstance->mSocket->queuePacket(packet);
}

/**
 * @brief
 *
 * @param itemName
 */
void Client::sendRegionalCollectPacket(GameDataHolderAccessor holder, al::PlacementId* placementId) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }


    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    RegionalCollect* packet = new RegionalCollect();
    
    sead::FixedSafeString<0x20> placementString;
    placementId->makeString(&placementString);
    strcpy(packet->objId, placementString.cstr());
    strcpy(packet->worldName, GameDataFunction::getCurrentStageName(holder));
    packet->mUserID = sInstance->mUserID;

    sInstance->mSocket->queuePacket(packet);
}

/**
 * @brief
 *
 * @param itemName
 */
void Client::sendDeathlinkPacket() {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    Deathlink* packet = new Deathlink();
    packet->mUserID = sInstance->mUserID;

    sInstance->mSocket->queuePacket(packet);
}

void Client::receiveDeath(Deathlink* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->apDeath = true;
    sInstance->dying = true;

}

void Client::setDying(bool value)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->dying = value;
}

void Client::setApDeath(bool value)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->apDeath = value;
}

void Client::sendShineChecksPacket() {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    ShineChecks* packet = new ShineChecks();
    packet->mUserID = sInstance->mUserID;

    packet->checks1 = getShineChecks(1);
    packet->checks2 = getShineChecks(2);
    packet->checks3 = getShineChecks(3);
    packet->checks4 = getShineChecks(4);
    packet->checks5 = getShineChecks(5);
    packet->checks6 = getShineChecks(6);
    packet->checks7 = getShineChecks(7);
    packet->checks8 = getShineChecks(8);
    packet->checks9 = getShineChecks(9);
    packet->checks10 = getShineChecks(10);
    packet->checks11 = getShineChecks(11);
    packet->checks12 = getShineChecks(12);
    packet->checks13 = getShineChecks(13);
    packet->checks14 = getShineChecks(14);
    packet->checks15 = getShineChecks(15);
    packet->checks16 = getShineChecks(16);
    packet->checks17 = getShineChecks(17);
    packet->checks18 = getShineChecks(18);
    packet->checks19 = getShineChecks(19);
    packet->checks20 = getShineChecks(20);
    packet->checks21 = getShineChecks(21);
    packet->checks22 = getShineChecks(22);
    packet->checks23 = getShineChecks(23);
    packet->checks24 = getShineChecks(24);
    

    sInstance->mSocket->queuePacket(packet);
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updatePlayerInfo(PlayerInf *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo) {
        return;
    }

    if(!curInfo->isConnected) {
        curInfo->isConnected = true;
    }

    curInfo->playerPos = packet->playerPos;

    // check if rotation is larger than zero and less than or equal to 1
    if(abs(packet->playerRot.x) > 0.f || abs(packet->playerRot.y) > 0.f || abs(packet->playerRot.z) > 0.f || abs(packet->playerRot.w) > 0.f) {
        if(abs(packet->playerRot.x) <= 1.f || abs(packet->playerRot.y) <= 1.f || abs(packet->playerRot.z) <= 1.f || abs(packet->playerRot.w) <= 1.f) {
            curInfo->playerRot = packet->playerRot;
        }
    }

        if (packet->actName != PlayerAnims::Type::Unknown) {
            strcpy(curInfo->curAnimStr, PlayerAnims::FindStr(packet->actName));
            if (curInfo->curAnimStr[0] == '\0')
                Logger::log("[ERROR] %s: actName was out of bounds: %d\n", __func__, packet->actName);
        } else {
            strcpy(curInfo->curAnimStr, "Wait");
        }

        if(packet->subActName != PlayerAnims::Type::Unknown) {
            strcpy(curInfo->curSubAnimStr, PlayerAnims::FindStr(packet->subActName));
            if (curInfo->curSubAnimStr[0] == '\0')
                Logger::log("[ERROR] %s: subActName was out of bounds: %d\n", __func__, packet->subActName);
        } else {
            strcpy(curInfo->curSubAnimStr, "");
        }

    curInfo->curAnim = packet->actName;
    curInfo->curSubAnim = packet->subActName;

    for (size_t i = 0; i < 6; i++)
    {
        // weights can only be between 0 and 1
        if(packet->animBlendWeights[i] >= 0.f && packet->animBlendWeights[i] <= 1.f) {
            curInfo->blendWeights[i] = packet->animBlendWeights[i];
        }
    }

    //TEMP

    if(!curInfo->isCapThrow) {
        curInfo->capPos = packet->playerPos;
    }

}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateHackCapInfo(HackCapInf *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (curInfo) {
        curInfo->capPos = packet->capPos;
        curInfo->capRot = packet->capQuat;

        curInfo->isCapThrow = packet->isCapVisible;

        strcpy(curInfo->capAnim, packet->capAnim);
    }
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateCaptureInfo(CaptureInf* packet) {
    
    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);
        
    if (!curInfo) {
        return;
    }

    curInfo->isCaptured = strlen(packet->hackName) > 0;

    if (curInfo->isCaptured) {
        strcpy(curInfo->curHack, packet->hackName);
    }
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateCostumeInfo(CostumeInf *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo) {
        return;
    }

    strcpy(curInfo->costumeBody, packet->bodyModel);
    strcpy(curInfo->costumeHead, packet->capModel);
}

/**
 * @brief 
 * 
 * @param packet 
 */
//void Client::updateShineInfo(ShineCollect* packet) {
//    if(collectedShineCount < curCollectedShines.size() - 1) {
//        curCollectedShines[collectedShineCount] = packet->shineId;
//        collectedShineCount++;
//        
//    }
//}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updatePlayerConnect(PlayerConnect* packet) {
    
    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, true);

    if (!curInfo) {
        return;
    }

    if (curInfo->isConnected) {

        Logger::log("Info is already being used by another connected player!\n");
        packet->mUserID.print("Connection ID");
        curInfo->playerID.print("Target Info");

    } else {

        packet->mUserID.print("Player Connected! ID");

        curInfo->playerID = packet->mUserID;
        curInfo->isConnected = true;
        strcpy(curInfo->puppetName, packet->clientName);

        mConnectCount++;
    }
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateGameInfo(GameInf *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo) {
        return;
    }

    if(curInfo->isConnected) {

        curInfo->scenarioNo = packet->scenarioNo;

        if(strcmp(packet->stageName, "") != 0 && strlen(packet->stageName) > 3) {
            strcpy(curInfo->stageName, packet->stageName);
        }

        curInfo->is2D = packet->is2D;
    }
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateTagInfo(TagInf *packet) {
    
    // if the packet is for our player, edit info for our player
    if (packet->mUserID == mUserID && GameModeManager::instance()->isMode(GameMode::HIDEANDSEEK)) {

        HideAndSeekMode* mMode = GameModeManager::instance()->getMode<HideAndSeekMode>();
        HideAndSeekInfo* curInfo = GameModeManager::instance()->getInfo<HideAndSeekInfo>();

        if (packet->updateType & TagUpdateType::STATE) {
            mMode->setPlayerTagState(packet->isIt);
        }

        if (packet->updateType & TagUpdateType::TIME) {
            curInfo->mHidingTime.mSeconds = packet->seconds;
            curInfo->mHidingTime.mMinutes = packet->minutes;
        }

        return;

    }

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo) {
        return;
    }

    curInfo->isIt = packet->isIt;
    curInfo->seconds = packet->seconds;
    curInfo->minutes = packet->minutes;
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::sendToStage(ChangeStagePacket* packet) {
    if (mSceneInfo && mSceneInfo->mSceneObjHolder) {

        GameDataHolderAccessor accessor(mSceneInfo->mSceneObjHolder);

        if (packet->scenarioNo > 0) {
            setScenario(accessor.mData->mWorldList->tryFindWorldIndexByStageName(packet->changeStage), packet->scenarioNo);
        }

        Logger::log("Sending Player to %s at Entrance %s in Scenario %d\n", packet->changeStage,
                     packet->changeID, packet->scenarioNo);
        
        ChangeStageInfo info(accessor.mData, packet->changeID, packet->changeStage, false, packet->scenarioNo, static_cast<ChangeStageInfo::SubScenarioType>(packet->subScenarioType));
        GameDataFunction::tryChangeNextStage(accessor, &info);
    }
}
/**
 * @brief 
 * Send a udp holepunch packet to the server
 */
void Client::sendUdpHolePunch() {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    HolePunch *packet = new HolePunch();
	
    packet->mUserID = sInstance->mUserID;

    sInstance->mSocket->queuePacket(packet);
}
/**
 * @brief 
 * Send a udp init packet to server
 */
void Client::sendUdpInit() {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    UdpInit *packet = new UdpInit();
	
    packet->mUserID = sInstance->mUserID;
	packet->port = sInstance->mSocket->getLocalUdpPort();
	
    sInstance->mSocket->queuePacket(packet);
}
/**
 * @brief 
 * 
 * @param packet 
 */
void Client::disconnectPlayer(PlayerDC *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo || !curInfo->isConnected) {
        return;
    }
    
    curInfo->isConnected = false;

    curInfo->scenarioNo = -1;
    strcpy(curInfo->stageName, "");
    curInfo->isInSameStage = false;

    mConnectCount--;
}

/**
 * @brief 
 * 
 * @param shineId 
 * @return true 
 * @return false 
 */
bool Client::isShineCollected(int shineId) {

    for (size_t i = 0; i < curCollectedShines.size(); i++)
    {
        if(curCollectedShines[i] >= 0) {
            if(curCollectedShines[i] == shineId) {
                return true;
            }
        }
    }
    
    return false;

}

/**
 * @brief 
 * 
 * @param id 
 * @return int 
 */
PuppetInfo* Client::findPuppetInfo(const nn::account::Uid& id, bool isFindAvailable) {

    PuppetInfo *firstAvailable = nullptr;

    for (size_t i = 0; i < getMaxPlayerCount() - 1; i++) {

        PuppetInfo* curInfo = mPuppetInfoArr[i];

        if (curInfo->playerID == id) {
            return curInfo;
        } else if (isFindAvailable && !firstAvailable && !curInfo->isConnected) {
            firstAvailable = curInfo;
        }
    }

    if (!firstAvailable) {
        Logger::log("Unable to find Assigned Puppet for Player!\n");
        id.print("User ID");
    }

    return firstAvailable;
}

/**
 * @brief 
 * 
 * @param holder 
 */
void Client::setStageInfo(GameDataHolderAccessor holder) {
    if (sInstance) {

        sInstance->mStageName = GameDataFunction::getCurrentStageName(holder);
        sInstance->mScenario = holder.mData->mGameDataFile->getScenarioNo(); //holder.mData->mGameDataFile->getMainScenarioNoCurrent();
        
        sInstance->mPuppetHolder->setStageInfo(sInstance->mStageName.cstr(), sInstance->mScenario);
    }
}

void Client::sendStage(GameDataHolderWriter writer, const ChangeStageInfo* stageInfo) {
    if (sInstance)
    {
        GameDataHolderAccessor accessor(sInstance->mCurStageScene);

        setScenario(stageInfo->changeStageName.cstr(), stageInfo->scenarioNo);
        //setMessage(1, "onGrandShineStageChange");
        //setMessage(2, stageInfo->changeStageName.cstr());

        if (GameDataFunction::getWorldIndexWaterfall() ==
            GameDataFunction::getCurrentWorldId(accessor)
                 || GameDataFunction::isUnlockedCurrentWorld(accessor)) {
            GameDataFunction::tryChangeNextStage(accessor, stageInfo);
        } else {
            int i = 0;
            for (i = GameDataFunction::getWorldIndexSpecial2(); i > 0; i--)
            {
                if (GameDataFunction::isUnlockedWorld(accessor, i))
                {
                    break;
                }
            }
            ChangeStageInfo info(accessor.mData, "",
                                 GameDataFunction::getMainStageName(accessor, i), false, -1,
                static_cast<ChangeStageInfo::SubScenarioType>(0));
            GameDataFunction::tryChangeNextStage(accessor, &info);
        }
    }
}

void Client::setScenario(int worldID, int scenario)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sendProgressWorldPacket(worldID, scenario);
    sInstance->worldScenarios[worldID] = scenario;

}

bool Client::setScenario(const char* worldName, int scenario) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    int worldID = accessor.mData->mWorldList->tryFindWorldIndexByStageName(worldName);
    if (accessor.mData->mWorldList->getMoonRockScenarioNo(worldID) <= scenario && scenario >= 1) {
        setScenario(worldID, scenario);
        return true;
    }
    return false;
}

int Client::getScenario(const char* worldName)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return -1;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    int worldID = accessor.mData->mWorldList->tryFindWorldIndexByStageName(worldName);

    /*if (sInstance->worldScenarios[worldID] < GameDataFunction::getWorldScenarioNo(accessor, worldID))
    {
        setScenario(worldID, GameDataFunction::getWorldScenarioNo(accessor, worldID));
    }*/
    return sInstance->worldScenarios[worldID];

}

int Client::getScenario(int worldID)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return -1;
    }

    return sInstance->worldScenarios[worldID];
}

void Client::sendCorrectScenario(const ChangeStageInfo* stageInfo)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    /*char str[12] = {'S', 'c', 'e', 'n', 'a', 'r', 'i', 'o', ' ', ' ', ' '};
    str[9] = static_cast<char>(
        48 + sInstance->worldScenarios[accessor.mData->mWorldList->tryFindWorldIndexByStageName(
                                   stageInfo->changeStageName.cstr())]);

    sInstance->apChatLine1 = str;
    
    char str2[12] = {'W', 'o', 'r', 'l', 'd', 'I', 'D', ' ', ' ', ' '};
    str2[9] = static_cast<char>(
        48 + accessor.mData->mWorldList->tryFindWorldIndexByStageName(stageInfo->changeStageName.cstr()));
    sInstance->apChatLine2 = str2;*/

    // try changing isReturn (param_4)
    ChangeStageInfo info(accessor.mData, stageInfo->changeStageId.cstr(),
                         stageInfo->changeStageName.cstr(),
                         false,
                         getScenario(stageInfo->changeStageName.cstr()),
                         static_cast<ChangeStageInfo::SubScenarioType>(0));
    GameDataFunction::tryChangeNextStage(accessor, &info);
}

void Client::setCheckIndex(int index)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->checkIndex = index;
}

void Client::sendCheckPacket(int locationId, int itemType) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    Check *packet = new Check();
    packet->locationId = locationId;
    packet->itemType = itemType;

    sInstance->mSocket->queuePacket(packet);
}

void Client::sendCheckPacket(int itemType, const char* objId, const char* stageName) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    Check *packet = new Check();
    packet->itemType = itemType;
    strcpy(packet->objId, objId);
    strcpy(packet->stage, stageName);

    sInstance->mSocket->queuePacket(packet);
}

void Client::receiveCheck(Check* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int itemType = packet->itemType;

    struct ShopItem::ShopItemInfo amiiboData = {1, 1};
    struct ShopItem::ShopItemInfo* amiibo = &amiiboData;
    struct ShopItem::ItemInfo info = {1, {}, static_cast<ShopItem::ItemType>(itemType), 1, amiibo, true};

    struct ShopItem::ItemInfo* infoPtr;
    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    switch (itemType)
    { 
    case -2:
        if (packet->index < getCheckIndex())
        {
            GameDataFunction::addCoin(accessor, packet->amount);
        }
        break;
    case -1:
        if (collectedShineCount < curCollectedShines.size() - 1) {
            curCollectedShines[collectedShineCount] = packet->locationId;
            collectedShineCount++;
        }
        break;
    case 0:
        strcpy(info.mName, costumeNames[packet->locationId]);
        info.mType = static_cast<ShopItem::ItemType>(itemType);
        infoPtr = &info;
        accessor.mData->mGameDataFile->buyItem(infoPtr, false);
        GameDataFunction::wearCostume(accessor, info.mName);
        break;
    case 1:
        strcpy(info.mName, costumeNames[packet->locationId]);
        info.mType = static_cast<ShopItem::ItemType>(itemType);
        infoPtr = &info;
        accessor.mData->mGameDataFile->buyItem(infoPtr, false);
        GameDataFunction::wearCap(accessor, info.mName);
        break;
    case 2:
        strcpy(info.mName, souvenirNames[packet->locationId]);
        info.mType = static_cast<ShopItem::ItemType>(itemType);
        infoPtr = &info;
        accessor.mData->mGameDataFile->buyItem(infoPtr, false);
        break;
    case 3:
        strcpy(info.mName, stickerNames[packet->locationId]);
        info.mType = static_cast<ShopItem::ItemType>(itemType);
        infoPtr = &info;
        accessor.mData->mGameDataFile->buyItem(infoPtr, false);
        break;
    }

    if (getCheckIndex() < packet->index) {
        setCheckIndex(packet->index);
    }

}

int Client::getWorldUnlockCount(int worldId) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return sInstance->worldPayCounts[worldId];
}

void Client::addShine(int uid)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int shines = sInstance->collectedShines[uid / 32];

    int index = (uid / 32) * 32;
    int i = 1;
    while (i < 0x80000000) {
        if (index == uid) {
            shines = shines | i;
            break;
        }
        i = i << 1;
        index += 1;
    }


    sInstance->collectedShines[uid / 32] = shines;
}

bool Client::hasShine(int uid)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int shines = sInstance->collectedShines[uid / 32];

    int index = (uid / 32) * 32;
    int i = 1;
    while (i < 0x80000000) {
        if (index == uid) {
            shines = shines & i;
            return (shines == i);
        }
        i = i << 1;
        index += 1;
    }
}

int Client::getShineChecks(int index)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return sInstance->collectedShines[index];
}

void Client::setShineChecks(int index, int checks)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->collectedShines[index] = checks;
}

void Client::addOutfit(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int index = getIndexCostumeList(info->mName) + 44 * static_cast<int>(info->mType);

    int outfits = sInstance->collectedOutfits[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x80) {
        if (curIndex == index) {
            outfits = outfits | i;
            break;
        }
        i = i << 1;
        curIndex += 1;
    }


    sInstance->collectedOutfits[index / 8] = outfits;
}

bool Client::hasOutfit(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int index = getIndexCostumeList(info->mName) + 44 * static_cast<int>(info->mType);
    if (index == -1) {
        //setMessage(2, info->mName);
        return false;
    }

    u8 outfits = sInstance->collectedOutfits[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x80) {
        if (curIndex == index) {
            outfits = outfits & i;
            return (outfits == i);
        }
        i = i << 1;
        curIndex += 1;
    }
}

int Client::getOutfitChecks(int index)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return static_cast<int>(sInstance->collectedOutfits[index]);
}

void Client::setOutfitChecks(int index, int checks) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    u8 u8Checks = static_cast<u8>(checks);
    sInstance->collectedOutfits[index] = u8Checks;
}

void Client::addSticker(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int index = getIndexStickerList(info->mName);

    int stickers = sInstance->collectedStickers[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x80) {
        if (curIndex == index) {
            stickers = stickers | i;
            break;
        }
        i = i << 1;
        curIndex += 1;
    }


    sInstance->collectedStickers[index / 8] = stickers;
}

bool Client::hasSticker(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int index = getIndexStickerList(info->mName);
    if (index == -1) {
        //setMessage(2, info->mName);
        return false;
    }

    u8 stickers = sInstance->collectedStickers[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x80) {
        if (curIndex == index) {
            stickers = stickers & i;
            return (stickers == i);
        }
        i = i << 1;
        curIndex += 1;
    }
}

int Client::getStickerChecks(int index) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return static_cast<int>(sInstance->collectedStickers[index]);
}

void Client::setStickerChecks(int index, int checks) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    u8 u8Checks = static_cast<u8>(checks);
    sInstance->collectedStickers[index] = u8Checks;
}

void Client::addSouvenir(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int index = getIndexSouvenirList(info->mName);

    int souvenirs = sInstance->collectedSouvenirs[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x80) {
        if (curIndex == index) {
            souvenirs = souvenirs | i;
            break;
        }
        i = i << 1;
        curIndex += 1;
    }


    sInstance->collectedSouvenirs[index / 8] = souvenirs;
}

bool Client::hasSouvenir(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int index = getIndexSouvenirList(info->mName);
    if (index == -1) {
        //setMessage(2, info->mName);
        return false;
    }

    u8 souvenirs = sInstance->collectedSouvenirs[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x80) {
        if (curIndex == index) {
            souvenirs = souvenirs & i;
            return (souvenirs == i);
        }
        i = i << 1;
        curIndex += 1;
    }
}

int Client::getSouvenirChecks(int index) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return static_cast<int>(sInstance->collectedSouvenirs[index]);
}

void Client::setSouvenirChecks(int index, int checks) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    u8 u8Checks = static_cast<u8>(checks);
    sInstance->collectedSouvenirs[index] = u8Checks;
}

bool Client::hasItem(const ShopItem::ItemInfo* info)
{
    switch (static_cast<int>(info->mType))
    { 
        case 1:
            return hasOutfit(info);
        case 0:
            return hasOutfit(info);
        case 3:
            return hasSticker(info);
        case 2:
            return hasSouvenir(info);
        default:
            // Moon and useitem
            return false;
    }
}

void Client::addItem(const ShopItem::ItemInfo* info)
{
    switch (static_cast<int>(info->mType)) {
        case 1:
            addOutfit(info);
            break;
        case 0:
            addOutfit(info);
            break;
        case 3:
            addSticker(info);
            break;
        case 2:
            addSouvenir(info);
            break;
        default:
            // Moon and useitem
            break;
    }
}

void Client::addCapture(const char *capture)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int index = getIndexCaptureList(capture);

    int checkedCaptures = sInstance->collectedCaptures[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x80) {
        if (curIndex == index) {
            checkedCaptures = checkedCaptures | i;
            break;
        }
        i = i << 1;
        curIndex += 1;
    }


    sInstance->collectedCaptures[index / 8] = checkedCaptures;
}

bool Client::hasCapture(const char* capture) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int index = getIndexCaptureList(capture);
    if (index == -1)
    {
        return false;
    }

    u8 checkedCaptures = sInstance->collectedCaptures[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x80) {
        if (curIndex == index) {
            checkedCaptures = checkedCaptures & i;
            return (checkedCaptures == i);
        }
        i = i << 1;
        curIndex += 1;
    }
}

int Client::getCaptureChecks(int index) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return static_cast<int>(sInstance->collectedCaptures[index]);
}

void Client::setCaptureChecks(int index, int checks) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    u8 u8Checks = static_cast<u8>(checks);
    sInstance->collectedCaptures[index] = u8Checks;
}

void Client::startShineCount() {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    sInstance->mCurStageScene->mSceneLayout->startShineCountAnim(false);
    sInstance->mCurStageScene->mSceneLayout->updateCounterParts();  // updates shine chip layout to (maybe) prevent softlocks
}

void Client::setMessage(int num, const char* msg)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    switch (num) {
    case 1: 
        sInstance->apChatLine1 = msg;
        break;
    case 2: 
        sInstance->apChatLine2 = msg;
        break;
    case 3: 
        sInstance->apChatLine3 = msg;
        break;
    }
}

void Client::addApInfo(ApInfo* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::WFixedSafeString<40> info1;
    sead::WFixedSafeString<40> info2;
    sead::WFixedSafeString<40> info3;
    info1 = info1.cEmptyString;
    info2 = info2.cEmptyString;
    info3 = info3.cEmptyString;

    for (int i = 0; i < 40; i++)
    {
        if (packet->info1[i] == '\0') {
            break;
        }
        info1.append(static_cast<char16>(packet->info1[i]));
    }

    for (int i = 0; i < 40; i++)
    {
        if (packet->info2[i] == '\0') {
            break;
        }
        info2.append(static_cast<char16>(packet->info2[i]));
    }

    for (int i = 0; i < 40; i++)
    {
        if (packet->info3[i] == '\0') {
            break;
        }
        info3.append(static_cast<char16>(packet->info3[i]));
    }

    int type = static_cast<int>(packet->infoType);
    //setMessage(2, "AP Info Entered");


    if (type == 0)
    {
        //setMessage(1, "Game Info Entered");
        //if (!info1.isEmpty()) 
        //{
            sInstance->apGameNames[packet->index1] =
                sInstance->apGameNames[packet->index1].cEmptyString;
            sInstance->apGameNames[packet->index1].append(info1.cstr());
            sInstance->numApGames++;
        //}
        //if (!info2.isEmpty()) 
        //{
            sInstance->apGameNames[packet->index2] =
                sInstance->apGameNames[packet->index2].cEmptyString;
            sInstance->apGameNames[packet->index2].append(info2.cstr());
            sInstance->numApGames++;
            //}
        //if (!info3.isEmpty()) 
        //{
            sInstance->apGameNames[packet->index3] =
                sInstance->apGameNames[packet->index3].cEmptyString;
            sInstance->apGameNames[packet->index3].append(info3.cstr());
            sInstance->numApGames++;
            //}
    } 
        
    if (type == 1)
    {
        //if (!info1.isEmpty())
        //{
            sInstance->apSlotNames[packet->index1] =
                sInstance->apSlotNames[packet->index1].cEmptyString;
            sInstance->apSlotNames[packet->index1].append(info1.cstr());
            sInstance->numApSlots++;
        //}
        //if (!info2.isEmpty()) 
        //{
            sInstance->apSlotNames[packet->index2] =
                sInstance->apSlotNames[packet->index2].cEmptyString;
            sInstance->apSlotNames[packet->index2].append(info2.cstr());
                sInstance->numApSlots++;
        //}
        //if (!info3.isEmpty()) 
        //{
            sInstance->apSlotNames[packet->index3] =
                sInstance->apSlotNames[packet->index3].cEmptyString;
            sInstance->apSlotNames[packet->index3].append(info3.cstr());
                sInstance->numApSlots++;
        //}
    }
      
    if (type == 2)
    {
        //if (!info1.isEmpty())
        //{
            sInstance->apItemNames[packet->index1] =
                sInstance->apItemNames[packet->index1].cEmptyString;
            sInstance->apItemNames[packet->index1].append(info1.cstr());
            sInstance->numApItems++;
        //}
        //if (!info2.isEmpty()) 
        //{
            sInstance->apItemNames[packet->index2] =
                sInstance->apItemNames[packet->index2].cEmptyString;
            sInstance->apItemNames[packet->index2].append(info2.cstr());
            sInstance->numApItems++;
        //}
        //if (!info3.isEmpty())
        //{
            sInstance->apItemNames[packet->index3] =
                sInstance->apItemNames[packet->index3].cEmptyString;
            sInstance->apItemNames[packet->index3].append(info3.cstr());
            sInstance->numApItems++;
        //}
    }

}

void Client::updateShopReplace(ShopReplacePacket* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    int type = static_cast<int>(packet->infoType);
    // Cap
    if (type == 0)
    {
        sInstance->shopCapTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                 packet->itemIndex0, packet->itemClassification0};
        sInstance->shopCapTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                 packet->itemIndex1, packet->itemClassification1};
        sInstance->shopCapTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                 packet->itemIndex2, packet->itemClassification2};
        sInstance->shopCapTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                 packet->itemIndex3, packet->itemClassification3};
        sInstance->shopCapTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                 packet->itemIndex4, packet->itemClassification4};
        sInstance->shopCapTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                 packet->itemIndex5, packet->itemClassification5};
        sInstance->shopCapTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                 packet->itemIndex6, packet->itemClassification6};
        sInstance->shopCapTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                 packet->itemIndex7, packet->itemClassification7};
        sInstance->shopCapTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                 packet->itemIndex8, packet->itemClassification8};
        sInstance->shopCapTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                 packet->itemIndex9, packet->itemClassification9};
        sInstance->shopCapTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                  packet->itemIndex10,
                                                  packet->itemClassification10};
        sInstance->shopCapTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                  packet->itemIndex11,
                                                  packet->itemClassification11};
        sInstance->shopCapTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                  packet->itemIndex12,
                                                  packet->itemClassification12};
        sInstance->shopCapTextReplacements[13] = {packet->gameIndex13, packet->playerIndex13,
                                                  packet->itemIndex13,
                                                  packet->itemClassification13};
        sInstance->shopCapTextReplacements[14] = {packet->gameIndex14, packet->playerIndex14,
                                                  packet->itemIndex14,
                                                  packet->itemClassification14};
        sInstance->shopCapTextReplacements[15] = {packet->gameIndex15, packet->playerIndex15,
                                                  packet->itemIndex15,
                                                  packet->itemClassification15};
        sInstance->shopCapTextReplacements[16] = {packet->gameIndex16, packet->playerIndex16,
                                                  packet->itemIndex16,
                                                  packet->itemClassification16};
        sInstance->shopCapTextReplacements[17] = {packet->gameIndex17, packet->playerIndex17,
                                                  packet->itemIndex17,
                                                  packet->itemClassification17};
        sInstance->shopCapTextReplacements[18] = {packet->gameIndex18, packet->playerIndex18,
                                                  packet->itemIndex18,
                                                  packet->itemClassification18};
        sInstance->shopCapTextReplacements[19] = {packet->gameIndex19, packet->playerIndex19,
                                                  packet->itemIndex19,
                                                  packet->itemClassification19};
        sInstance->shopCapTextReplacements[20] = {packet->gameIndex20, packet->playerIndex20,
                                                  packet->itemIndex20,
                                                  packet->itemClassification20};
        sInstance->shopCapTextReplacements[21] = {packet->gameIndex21, packet->playerIndex21,
                                                  packet->itemIndex21,
                                                  packet->itemClassification21};
        sInstance->shopCapTextReplacements[22] = {packet->gameIndex22, packet->playerIndex22,
                                                  packet->itemIndex22,
                                                  packet->itemClassification22};
        sInstance->shopCapTextReplacements[23] = {packet->gameIndex23, packet->playerIndex23,
                                                  packet->itemIndex23,
                                                  packet->itemClassification23};
        sInstance->shopCapTextReplacements[24] = {packet->gameIndex24, packet->playerIndex24,
                                                  packet->itemIndex24,
                                                  packet->itemClassification24};
        sInstance->shopCapTextReplacements[25] = {packet->gameIndex25, packet->playerIndex25,
                                                  packet->itemIndex25,
                                                  packet->itemClassification25};
        sInstance->shopCapTextReplacements[26] = {packet->gameIndex26, packet->playerIndex26,
                                                  packet->itemIndex26,
                                                  packet->itemClassification26};
        sInstance->shopCapTextReplacements[27] = {packet->gameIndex27, packet->playerIndex27,
                                                  packet->itemIndex27,
                                                  packet->itemClassification27};
        sInstance->shopCapTextReplacements[28] = {packet->gameIndex28, packet->playerIndex28,
                                                  packet->itemIndex28,
                                                  packet->itemClassification28};
        sInstance->shopCapTextReplacements[29] = {packet->gameIndex29, packet->playerIndex29,
                                                  packet->itemIndex29,
                                                  packet->itemClassification29};
        sInstance->shopCapTextReplacements[30] = {packet->gameIndex30, packet->playerIndex30,
                                                  packet->itemIndex30,
                                                  packet->itemClassification30};
        sInstance->shopCapTextReplacements[31] = {packet->gameIndex31, packet->playerIndex31,
                                                  packet->itemIndex31,
                                                  packet->itemClassification31};
        sInstance->shopCapTextReplacements[32] = {packet->gameIndex32, packet->playerIndex32,
                                                  packet->itemIndex32,
                                                  packet->itemClassification32};
        sInstance->shopCapTextReplacements[33] = {packet->gameIndex33, packet->playerIndex33,
                                                  packet->itemIndex33,
                                                  packet->itemClassification33};
        sInstance->shopCapTextReplacements[34] = {packet->gameIndex34, packet->playerIndex34,
                                                  packet->itemIndex34,
                                                  packet->itemClassification34};
        sInstance->shopCapTextReplacements[35] = {packet->gameIndex35, packet->playerIndex35,
                                                  packet->itemIndex35,
                                                  packet->itemClassification35};
        sInstance->shopCapTextReplacements[36] = {packet->gameIndex36, packet->playerIndex36,
                                                  packet->itemIndex36,
                                                  packet->itemClassification36};
        sInstance->shopCapTextReplacements[37] = {packet->gameIndex37, packet->playerIndex37,
                                                  packet->itemIndex37,
                                                  packet->itemClassification37};
        sInstance->shopCapTextReplacements[38] = {packet->gameIndex38, packet->playerIndex38,
                                                  packet->itemIndex38,
                                                  packet->itemClassification38};
        sInstance->shopCapTextReplacements[39] = {packet->gameIndex39, packet->playerIndex39,
                                                  packet->itemIndex39,
                                                  packet->itemClassification39};
        sInstance->shopCapTextReplacements[40] = {packet->gameIndex40, packet->playerIndex40,
                                                  packet->itemIndex40,
                                                  packet->itemClassification40};
        sInstance->shopCapTextReplacements[41] = {packet->gameIndex41, packet->playerIndex41,
                                                  packet->itemIndex41,
                                                  packet->itemClassification41};
        sInstance->shopCapTextReplacements[42] = {packet->gameIndex42, packet->playerIndex42,
                                                  packet->itemIndex42,
                                                  packet->itemClassification42};
        sInstance->shopCapTextReplacements[43] = {packet->gameIndex43, packet->playerIndex43,
                                                  packet->itemIndex43,
                                                  packet->itemClassification43};
    }
    // Cloth
    if (type == 1)
    {
        sInstance->shopClothTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                   packet->itemIndex0, packet->itemClassification0};
        sInstance->shopClothTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                   packet->itemIndex1, packet->itemClassification1};
        sInstance->shopClothTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                   packet->itemIndex2, packet->itemClassification2};
        sInstance->shopClothTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                   packet->itemIndex3, packet->itemClassification3};
        sInstance->shopClothTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                   packet->itemIndex4, packet->itemClassification4};
        sInstance->shopClothTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                   packet->itemIndex5, packet->itemClassification5};
        sInstance->shopClothTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                   packet->itemIndex6, packet->itemClassification6};
        sInstance->shopClothTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                   packet->itemIndex7, packet->itemClassification7};
        sInstance->shopClothTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                   packet->itemIndex8, packet->itemClassification8};
        sInstance->shopClothTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                   packet->itemIndex9, packet->itemClassification9};
        sInstance->shopClothTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                    packet->itemIndex10,
                                                    packet->itemClassification10};
        sInstance->shopClothTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                    packet->itemIndex11,
                                                    packet->itemClassification11};
        sInstance->shopClothTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                    packet->itemIndex12,
                                                    packet->itemClassification12};
        sInstance->shopClothTextReplacements[13] = {packet->gameIndex13, packet->playerIndex13,
                                                    packet->itemIndex13,
                                                    packet->itemClassification13};
        sInstance->shopClothTextReplacements[14] = {packet->gameIndex14, packet->playerIndex14,
                                                    packet->itemIndex14,
                                                    packet->itemClassification14};
        sInstance->shopClothTextReplacements[15] = {packet->gameIndex15, packet->playerIndex15,
                                                    packet->itemIndex15,
                                                    packet->itemClassification15};
        sInstance->shopClothTextReplacements[16] = {packet->gameIndex16, packet->playerIndex16,
                                                    packet->itemIndex16,
                                                    packet->itemClassification16};
        sInstance->shopClothTextReplacements[17] = {packet->gameIndex17, packet->playerIndex17,
                                                    packet->itemIndex17,
                                                    packet->itemClassification17};
        sInstance->shopClothTextReplacements[18] = {packet->gameIndex18, packet->playerIndex18,
                                                    packet->itemIndex18,
                                                    packet->itemClassification18};
        sInstance->shopClothTextReplacements[19] = {packet->gameIndex19, packet->playerIndex19,
                                                    packet->itemIndex19,
                                                    packet->itemClassification19};
        sInstance->shopClothTextReplacements[20] = {packet->gameIndex20, packet->playerIndex20,
                                                    packet->itemIndex20,
                                                    packet->itemClassification20};
        sInstance->shopClothTextReplacements[21] = {packet->gameIndex21, packet->playerIndex21,
                                                    packet->itemIndex21,
                                                    packet->itemClassification21};
        sInstance->shopClothTextReplacements[22] = {packet->gameIndex22, packet->playerIndex22,
                                                    packet->itemIndex22,
                                                    packet->itemClassification22};
        sInstance->shopClothTextReplacements[23] = {packet->gameIndex23, packet->playerIndex23,
                                                    packet->itemIndex23,
                                                    packet->itemClassification23};
        sInstance->shopClothTextReplacements[24] = {packet->gameIndex24, packet->playerIndex24,
                                                    packet->itemIndex24,
                                                    packet->itemClassification24};
        sInstance->shopClothTextReplacements[25] = {packet->gameIndex25, packet->playerIndex25,
                                                    packet->itemIndex25,
                                                    packet->itemClassification25};
        sInstance->shopClothTextReplacements[26] = {packet->gameIndex26, packet->playerIndex26,
                                                    packet->itemIndex26,
                                                    packet->itemClassification26};
        sInstance->shopClothTextReplacements[27] = {packet->gameIndex27, packet->playerIndex27,
                                                    packet->itemIndex27,
                                                    packet->itemClassification27};
        sInstance->shopClothTextReplacements[28] = {packet->gameIndex28, packet->playerIndex28,
                                                    packet->itemIndex28,
                                                    packet->itemClassification28};
        sInstance->shopClothTextReplacements[29] = {packet->gameIndex29, packet->playerIndex29,
                                                    packet->itemIndex29,
                                                    packet->itemClassification29};
        sInstance->shopClothTextReplacements[30] = {packet->gameIndex30, packet->playerIndex30,
                                                    packet->itemIndex30,
                                                    packet->itemClassification30};
        sInstance->shopClothTextReplacements[31] = {packet->gameIndex31, packet->playerIndex31,
                                                    packet->itemIndex31,
                                                    packet->itemClassification31};
        sInstance->shopClothTextReplacements[32] = {packet->gameIndex32, packet->playerIndex32,
                                                    packet->itemIndex32,
                                                    packet->itemClassification32};
        sInstance->shopClothTextReplacements[33] = {packet->gameIndex33, packet->playerIndex33,
                                                    packet->itemIndex33,
                                                    packet->itemClassification33};
        sInstance->shopClothTextReplacements[34] = {packet->gameIndex34, packet->playerIndex34,
                                                    packet->itemIndex34,
                                                    packet->itemClassification34};
        sInstance->shopClothTextReplacements[35] = {packet->gameIndex35, packet->playerIndex35,
                                                    packet->itemIndex35,
                                                    packet->itemClassification35};
        sInstance->shopClothTextReplacements[36] = {packet->gameIndex36, packet->playerIndex36,
                                                    packet->itemIndex36,
                                                    packet->itemClassification36};
        sInstance->shopClothTextReplacements[37] = {packet->gameIndex37, packet->playerIndex37,
                                                    packet->itemIndex37,
                                                    packet->itemClassification37};
        sInstance->shopClothTextReplacements[38] = {packet->gameIndex38, packet->playerIndex38,
                                                    packet->itemIndex38,
                                                    packet->itemClassification38};
        sInstance->shopClothTextReplacements[39] = {packet->gameIndex39, packet->playerIndex39,
                                                    packet->itemIndex39,
                                                    packet->itemClassification39};
        sInstance->shopClothTextReplacements[40] = {packet->gameIndex40, packet->playerIndex40,
                                                    packet->itemIndex40,
                                                    packet->itemClassification40};
        sInstance->shopClothTextReplacements[41] = {packet->gameIndex41, packet->playerIndex41,
                                                    packet->itemIndex41,
                                                    packet->itemClassification41};
        sInstance->shopClothTextReplacements[42] = {packet->gameIndex42, packet->playerIndex42,
                                                    packet->itemIndex42,
                                                    packet->itemClassification42};
        sInstance->shopClothTextReplacements[43] = {packet->gameIndex43, packet->playerIndex43,
                                                    packet->itemIndex43,
                                                    packet->itemClassification43};
    }
    // Sticker
    if (type == 2)
    {
        sInstance->shopStickerTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                     packet->itemIndex0,
                                                     packet->itemClassification0};
        sInstance->shopStickerTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                     packet->itemIndex1,
                                                     packet->itemClassification1};
        sInstance->shopStickerTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                     packet->itemIndex2,
                                                     packet->itemClassification2};
        sInstance->shopStickerTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                     packet->itemIndex3,
                                                     packet->itemClassification3};
        sInstance->shopStickerTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                     packet->itemIndex4,
                                                     packet->itemClassification4};
        sInstance->shopStickerTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                     packet->itemIndex5,
                                                     packet->itemClassification5};
        sInstance->shopStickerTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                     packet->itemIndex6,
                                                     packet->itemClassification6};
        sInstance->shopStickerTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                     packet->itemIndex7,
                                                     packet->itemClassification7};
        sInstance->shopStickerTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                     packet->itemIndex8,
                                                     packet->itemClassification8};
        sInstance->shopStickerTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                     packet->itemIndex9,
                                                     packet->itemClassification9};
        sInstance->shopStickerTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                      packet->itemIndex10,
                                                      packet->itemClassification10};
        sInstance->shopStickerTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                      packet->itemIndex11,
                                                      packet->itemClassification11};
        sInstance->shopStickerTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                      packet->itemIndex12,
                                                      packet->itemClassification12};
        sInstance->shopStickerTextReplacements[13] = {packet->gameIndex13, packet->playerIndex13,
                                                      packet->itemIndex13,
                                                      packet->itemClassification13};
        sInstance->shopStickerTextReplacements[14] = {packet->gameIndex14, packet->playerIndex14,
                                                      packet->itemIndex14,
                                                      packet->itemClassification14};
        sInstance->shopStickerTextReplacements[15] = {packet->gameIndex15, packet->playerIndex15,
                                                      packet->itemIndex15,
                                                      packet->itemClassification15};
        sInstance->shopStickerTextReplacements[16] = {packet->gameIndex16, packet->playerIndex16,
                                                      packet->itemIndex16,
                                                      packet->itemClassification16};
    }
    // Gift
    if (type == 3)
    {
        sInstance->shopGiftTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                  packet->itemIndex0, packet->itemClassification0};
        sInstance->shopGiftTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                  packet->itemIndex1, packet->itemClassification1};
        sInstance->shopGiftTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                  packet->itemIndex2, packet->itemClassification2};
        sInstance->shopGiftTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                  packet->itemIndex3, packet->itemClassification3};
        sInstance->shopGiftTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                  packet->itemIndex4, packet->itemClassification4};
        sInstance->shopGiftTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                  packet->itemIndex5, packet->itemClassification5};
        sInstance->shopGiftTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                  packet->itemIndex6, packet->itemClassification6};
        sInstance->shopGiftTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                  packet->itemIndex7, packet->itemClassification7};
        sInstance->shopGiftTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                  packet->itemIndex8, packet->itemClassification8};
        sInstance->shopGiftTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                  packet->itemIndex9, packet->itemClassification9};
        sInstance->shopGiftTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                   packet->itemIndex10,
                                                   packet->itemClassification10};
        sInstance->shopGiftTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                   packet->itemIndex11,
                                                   packet->itemClassification11};
        sInstance->shopGiftTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                   packet->itemIndex12,
                                                   packet->itemClassification12};
        sInstance->shopGiftTextReplacements[13] = {packet->gameIndex13, packet->playerIndex13,
                                                   packet->itemIndex13,
                                                   packet->itemClassification13};
        sInstance->shopGiftTextReplacements[14] = {packet->gameIndex14, packet->playerIndex14,
                                                   packet->itemIndex14,
                                                   packet->itemClassification14};
        sInstance->shopGiftTextReplacements[15] = {packet->gameIndex15, packet->playerIndex15,
                                                   packet->itemIndex15,
                                                   packet->itemClassification15};
        sInstance->shopGiftTextReplacements[16] = {packet->gameIndex16, packet->playerIndex16,
                                                   packet->itemIndex16,
                                                   packet->itemClassification16};
        sInstance->shopGiftTextReplacements[17] = {packet->gameIndex17, packet->playerIndex17,
                                                   packet->itemIndex17,
                                                   packet->itemClassification17};
        sInstance->shopGiftTextReplacements[18] = {packet->gameIndex18, packet->playerIndex18,
                                                   packet->itemIndex18,
                                                   packet->itemClassification18};
        sInstance->shopGiftTextReplacements[19] = {packet->gameIndex19, packet->playerIndex19,
                                                   packet->itemIndex19,
                                                   packet->itemClassification19};
        sInstance->shopGiftTextReplacements[20] = {packet->gameIndex20, packet->playerIndex20,
                                                   packet->itemIndex20,
                                                   packet->itemClassification20};
        sInstance->shopGiftTextReplacements[21] = {packet->gameIndex21, packet->playerIndex21,
                                                   packet->itemIndex21,
                                                   packet->itemClassification21};
        sInstance->shopGiftTextReplacements[22] = {packet->gameIndex22, packet->playerIndex22,
                                                   packet->itemIndex22,
                                                   packet->itemClassification22};
        sInstance->shopGiftTextReplacements[23] = {packet->gameIndex23, packet->playerIndex23,
                                                   packet->itemIndex23,
                                                   packet->itemClassification23};
        sInstance->shopGiftTextReplacements[24] = {packet->gameIndex24, packet->playerIndex24,
                                                   packet->itemIndex24,
                                                   packet->itemClassification24};
        sInstance->shopGiftTextReplacements[25] = {packet->gameIndex25, packet->playerIndex25,
                                                   packet->itemIndex25,
                                                   packet->itemClassification25};
    }
    // Moon
    if (type == 4)
    {
        sInstance->shopMoonTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                  packet->itemIndex0, packet->itemClassification0};
        sInstance->shopMoonTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                  packet->itemIndex1, packet->itemClassification1};
        sInstance->shopMoonTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                  packet->itemIndex2, packet->itemClassification2};
        sInstance->shopMoonTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                  packet->itemIndex3, packet->itemClassification3};
        sInstance->shopMoonTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                  packet->itemIndex4, packet->itemClassification4};
        sInstance->shopMoonTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                  packet->itemIndex5, packet->itemClassification5};
        sInstance->shopMoonTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                  packet->itemIndex6, packet->itemClassification6};
        sInstance->shopMoonTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                  packet->itemIndex7, packet->itemClassification7};
        sInstance->shopMoonTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                  packet->itemIndex8, packet->itemClassification8};
        sInstance->shopMoonTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                  packet->itemIndex9, packet->itemClassification9};
        sInstance->shopMoonTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                   packet->itemIndex10,
                                                   packet->itemClassification10};
        sInstance->shopMoonTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                   packet->itemIndex11,
                                                   packet->itemClassification11};
        sInstance->shopMoonTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                   packet->itemIndex12,
                                                   packet->itemClassification12};
    }

}

const char16_t* Client::getShopReplacementText(const char* fileName, const char* key) 
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return u"";
    }

    sead::WFixedSafeString<200> message;
    message = message.cEmptyString;
    bool isExplain = false;
    sead::FixedSafeString<40> convert;
    convert = "";
    convert.append(key);
    if (convert.calcLength() != convert.removeSuffix("_Explain")) {
        key = convert.cstr();
        isExplain = true;
    }
    shopReplaceText curItem = {255,255,255,255};
    

    if (strcmp("ItemCap", fileName) == 0)
    {
        curItem = sInstance->shopCapTextReplacements[getIndexCostumeList(key)];
    }
    else if (strcmp("ItemCloth", fileName) == 0) 
    {
        curItem = sInstance->shopClothTextReplacements[getIndexCostumeList(key)];
    }
    else if (strcmp("ItemSticker", fileName) == 0) 
    {
        curItem = sInstance->shopStickerTextReplacements[getIndexStickerList(key)];
    }
    else if (strcmp("ItemGift", fileName) == 0) 
    {
        curItem = sInstance->shopGiftTextReplacements[getIndexSouvenirList(key)];
    }
    else if (strcmp("ItemMoon", fileName) == 0) 
    {
        // Find out key for each kingdom as still is unknown
        curItem = sInstance->shopMoonTextReplacements[getIndexMoonItemList(key)];
    } else {
        // Not included items like Life Up Hearts
        return u"";
    }

    if (curItem.gameIndex == 254)
    {
        setMessage(1, "No Item Data Received.");
    }

    if (isExplain)
    {
        message.append(u"Comes from the world of ");
        //if (sInstance->apGameNames[curItem.gameIndex].isEmpty()) {
            message.append(sInstance->apGameNames[curItem.gameIndex].cstr());
        //} else {
            //message.append(u"Missing Game");
       // }

        message.append(u".\nSeems to belong to ");
        //if (sInstance->apSlotNames[curItem.slotIndex].isEmpty()) {
            message.append(sInstance->apSlotNames[curItem.slotIndex].cstr());
        //} else {
            //message.append(u"Missing Slot Name");
        //}
        message.append(u".\n");
        if (curItem.itemClassification == 0) {
            message.append(u"It looks like junk, but may as well ask...");
        } else if (curItem.itemClassification == 0b0010) {
            message.append(u"It looks useful.");
           
        } else if (curItem.itemClassification == 254) {
            message.append(u"Error or Not in the Item Pool.");
        }
        else {
            message.append(u"It looks really important!");
        }
    } else {
        //if (sInstance->apSlotNames[curItem.slotIndex].isEmpty()) {
            message.append(sInstance->apItemNames[curItem.apItemNameIndex].cstr());
        //} else {
            //message.append(u"Missing Item Name");
        //}
    }

    return message.cstr();
}

/**
 * @brief 
 * 
 * @param puppet 
 * @return true 
 * @return false 
 */
bool Client::tryAddPuppet(PuppetActor *puppet) {
    if(sInstance) {
        return sInstance->mPuppetHolder->tryRegisterPuppet(puppet);
    }else {
        return false;
    }
}

/**
 * @brief 
 * 
 * @param puppet 
 * @return true 
 * @return false 
 */
bool Client::tryAddDebugPuppet(PuppetActor *puppet) {
    if(sInstance) {
        return sInstance->mPuppetHolder->tryRegisterDebugPuppet(puppet);
    }else {
        return false;
    }
}

/**
 * @brief 
 * 
 * @param idx 
 * @return PuppetActor* 
 */
PuppetActor *Client::getPuppet(int idx) {
    if(sInstance) {
        return sInstance->mPuppetHolder->getPuppetActor(idx);
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 * @return PuppetInfo* 
 */
PuppetInfo *Client::getLatestInfo() {
    if(sInstance) {
        return Client::getPuppetInfo(sInstance->mPuppetHolder->getSize() - 1);
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 * @param idx 
 * @return PuppetInfo* 
 */
PuppetInfo *Client::getPuppetInfo(int idx) {
    if(sInstance) {
        // unsafe get
        PuppetInfo *curInfo = sInstance->mPuppetInfoArr[idx];

        if (!curInfo) {
            Logger::log("Attempting to Access Puppet Out of Bounds! Value: %d\n", idx);
            return nullptr;
        }

        return curInfo;
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 */
void Client::resetCollectedShines() {
    collectedShineCount = 0;
    curCollectedShines.fill(-1);
}

/**
 * @brief 
 * 
 * @param shineId 
 */
void Client::removeShine(int shineId) {
    for (size_t i = 0; i < curCollectedShines.size(); i++)
    {
        if(curCollectedShines[i] == shineId) {
            curCollectedShines[i] = -1;
            collectedShineCount--;
        }
    }
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool Client::isNeedUpdateShines() {
    return sInstance ? sInstance->collectedShineCount > 0 : false;
}

/**
 * @brief 
 * 
 */
void Client::updateShines() {
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    // skip shine sync if player is in cap kingdom scenario zero (very start of the game)
    if (sInstance->mStageName == "CapWorldHomeStage" && (sInstance->mScenario == 0 || sInstance->mScenario == 1)) {
        return;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);
    
    for (size_t i = 0; i < sInstance->getCollectedShinesCount(); i++)
    {
        int shineID = sInstance->getShineID(i);

        if(shineID < 0) continue;

        Logger::log("Shine UID: %d\n", shineID);

        GameDataFile::HintInfo* shineInfo = CustomGameDataFunction::getHintInfoByUniqueID(accessor, shineID);

        if (shineInfo) {
            if (!GameDataFunction::isGotShine(accessor, shineInfo->mStageName.cstr(), shineInfo->mObjId.cstr())) {

                Shine* stageShine = findStageShine(shineID);
                
                if (stageShine) {

                    if (al::isDead(stageShine)) {
                        stageShine->makeActorAlive();
                    }
                    
                    
                    //stageShine->getDirect();
                    //stageShine->onSwitchGet();
                }

                accessor.mData->mGameDataFile->setGotShine(shineInfo);
            }
        }
    }
    
    sInstance->resetCollectedShines();
    startShineCount();
}

/**
 * @brief
 *
 */
void Client::updateItems(ItemCollect *packet) {
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }


    struct ShopItem::ShopItemInfo amiiboData = {1, 1};
    struct ShopItem::ShopItemInfo *amiibo = &amiiboData;
    struct ShopItem::ItemInfo info = {1, {}, static_cast<ShopItem::ItemType>(packet->type), 1, amiibo, true};
    strcpy(info.mName, packet->name);
    info.mType = (ShopItem::ItemType)(packet->type);
    struct ShopItem::ItemInfo* infoPtr = &info;
    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    accessor.mData->mGameDataFile->buyItem(infoPtr, false);
    
    if (isInCostumeList(packet->name))
        switch (packet->type) {
        case 0:
            GameDataFunction::wearCostume(accessor, packet->name);
            break;

        case 1:
            GameDataFunction::wearCap(accessor, packet->name);
            break;

        default:
            break;
        }

}

/**
 * @brief
 *
 */
void Client::updateFiller(FillerCollect* packet) {
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    switch (packet->type) {
    case 4:
        GameDataFunction::addCoin(accessor, 50);
        break;
    case 5:
        GameDataFunction::addCoin(accessor, 100);
        break;
    case 6:
        GameDataFunction::addCoin(accessor, 250);
        break;
    case 7:
        GameDataFunction::addCoin(accessor, 500);
        break;
    case 8:
        GameDataFunction::addCoin(accessor, 1000);
        break;
    case 9:
        struct ShopItem::ShopItemInfo amiiboData = {1, 1};
        struct ShopItem::ShopItemInfo* amiibo = &amiiboData;
        struct ShopItem::ItemInfo info = {1, {}, (ShopItem::ItemType)0, 1, amiibo, true};
        strcpy(info.mName, "LifeUpItem");
        info.mType = (ShopItem::ItemType)(4);
        struct ShopItem::ItemInfo* infoPtr = &info;
        accessor.mData->mGameDataFile->buyItem(infoPtr, false);
        break;

    }

}

/**
 * @brief
 *
 */
void Client::updateChatMessages(ArchipelagoChatMessage* packet)
{
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    sInstance->apChatLine1 = packet->message1;
    sInstance->apChatLine2 = packet->message2;
    sInstance->apChatLine3 = packet->message3;
}

void Client::updateSlotData(SlotData* packet) {
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }
    sInstance->worldPayCounts[1] = packet->cascade;
    sInstance->worldPayCounts[2] = packet->sand;
    sInstance->worldPayCounts[3] = packet->wooded;
    sInstance->worldPayCounts[4] = packet->lake;
    sInstance->worldPayCounts[6] = packet->lost;
    sInstance->worldPayCounts[7] = packet->metro;
    sInstance->worldPayCounts[8] = packet->seaside;
    sInstance->worldPayCounts[9] = packet->snow;
    sInstance->worldPayCounts[10] = packet->luncheon;
    sInstance->worldPayCounts[11] = packet->ruined;
    sInstance->worldPayCounts[12] = packet->bowser;
    sInstance->worldPayCounts[15] = packet->dark;
    sInstance->worldPayCounts[16] = packet->darker;
    sInstance->regionals = packet->regionals;
    sInstance->captures = packet->captures;

    sInstance->numApGames = 0;
    sInstance->numApSlots = 0;
    sInstance->numApItems = 0;
}

void Client::updateWorlds(UnlockWorld* packet)
{
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);
    GameDataFunction::unlockWorld(accessor, packet->worldID);
}

void Client::sendProgressWorldPacket(int worldID, int scenario)
{
    if (!sInstance)
    {
        Logger::log("Client Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    ProgressWorld* packet = new ProgressWorld();
    packet->worldID = worldID;
    packet->scenario = scenario;

    sInstance->mSocket->queuePacket(packet);
}

void Client::updateProgress(ProgressWorld* packet)
{
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    sInstance->worldScenarios[packet->worldID] = packet->scenario;
    
}

/**
 * @brief 
 * 
 */
void Client::update() {
    if (sInstance) {
        
        sInstance->mPuppetHolder->update();

        if (isNeedUpdateShines()) {
            updateShines();
        }

        GameModeManager::instance()->update();
    }
}

/**
 * @brief 
 * 
 */
void Client::clearArrays() {
    if(sInstance) {
        sInstance->mPuppetHolder->clearPuppets();
        sInstance->mShineArray.clear();

    }
}

/**
 * @brief 
 * 
 * @return PuppetInfo* 
 */
PuppetInfo *Client::getDebugPuppetInfo() {
    if(sInstance) {
        return &sInstance->mDebugPuppetInfo;
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 * @return PuppetActor* 
 */
PuppetActor *Client::getDebugPuppet() {
    if(sInstance) {
        return sInstance->mPuppetHolder->getDebugPuppet();
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 * @return Keyboard* 
 */
Keyboard* Client::getKeyboard() {
    if (sInstance) {
        return sInstance->mKeyboard;
    }
    return nullptr;
}

/**
 * @brief 
 * 
 * @return const char* 
 */
const char* Client::getCurrentIP() {
    if (sInstance) {
        return sInstance->mServerIP.cstr();
    }
    return nullptr;
}

/**
 * @brief 
 * 
 * @return const int 
 */
const int Client::getCurrentPort() {
    if (sInstance) {
        return sInstance->mServerPort;
    }
    return -1;
}

/**
 * @brief sets server IP to supplied string, used specifically for loading IP from the save file.
 * 
 * @param ip 
 */
void Client::setLastUsedIP(const char* ip) {
    if (sInstance) {
        sInstance->mServerIP = ip;
    }
}

/**
 * @brief sets server port to supplied string, used specifically for loading port from the save file.
 * 
 * @param port 
 */
void Client::setLastUsedPort(const int port) {
    if (sInstance) {
        sInstance->mServerPort = port;
    }
}

/**
 * @brief creates new scene info and copies supplied info to the new info, as well as stores a const ptr to the current stage scene.
 * 
 * @param initInfo 
 * @param stageScene 
 */
void Client::setSceneInfo(const al::ActorInitInfo& initInfo, const StageScene *stageScene) {

    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    sInstance->mSceneInfo = new al::ActorSceneInfo();

    memcpy(sInstance->mSceneInfo, &initInfo.mActorSceneInfo, sizeof(al::ActorSceneInfo));

    sInstance->mCurStageScene = stageScene;
}

/**
 * @brief stores shine pointer supplied into a ptr array if space is available, and shine is not collected.
 * 
 * @param shine 
 * @return true if shine was able to be successfully stored
 * @return false if shine is already collected, or ptr array is full
 */
bool Client::tryRegisterShine(Shine* shine) {
    if (sInstance) {
        if (!sInstance->mShineArray.isFull()) {
            if (!shine->isGot()) {
                sInstance->mShineArray.pushBack(shine);
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief finds the actor pointer stored in the shine ptr array based off shine ID
 * 
 * @param shineID Unique ID used for shine actor
 * @return Shine* if shine ptr array contains actor with supplied shine ID.
 */
Shine* Client::findStageShine(int shineID) {
    if (sInstance) {
        for (int i = 0; i < sInstance->mShineArray.size(); i++) {

            Shine* curShine = sInstance->mShineArray[i];

            if (curShine) {

                auto hintInfo =
                    CustomGameDataFunction::getHintInfoByIndex(curShine, curShine->mShineIdx);
                
                if (hintInfo->mUniqueID == shineID) {
                    return curShine;
                }
            }
        }
    }
    return nullptr;
}
