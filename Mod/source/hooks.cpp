#include <sys/types.h>
#include "al/execute/ExecuteDirector.h"
#include "al/execute/ExecuteOrder.h"
#include "al/execute/ExecuteTable.h"
#include "al/execute/ExecuteTableHolderDraw.h"
#include "al/execute/ExecuteTableHolderUpdate.h"
#include "al/scene/Scene.h"
#include "al/util/GraphicsUtil.h"
#include "al/util/KitUtil.h"
#include "basis/seadNew.h"
#include "logger.hpp"
#include "rs/util.hpp"
#include "server/Client.hpp"
#include "al/byaml/ByamlIter.h"
#include "al/util.hpp"
#include "game/Actors/WorldEndBorderKeeper.h"
#include "game/Player/PlayerActorHakoniwa.h"
#include "game/StageScene/StageSceneStateOption.h"
#include "game/StageScene/StageSceneStatePauseMenu.h"
#include "game/StageScene/StageSceneStateServerConfig.hpp"
#include "main.hpp"
#include "al/byaml/writer/ByamlWriter.h"
#include "math/seadVector.h"
#include "rs/util/InputUtil.h"
#include "sead/prim/seadSafeString.h"
#include "sead/container/seadSafeArray.h"
#include "server/hns/HideAndSeekMode.hpp"

bool comboBtnHook(int port) {
    if (GameModeManager::instance()->isActive()) { // only switch to combo if any gamemode is active
        return !al::isPadHoldL(port) && al::isPadTriggerDown(port);
    } else {
        return al::isPadTriggerDown(port);
    }
}

void saveWriteHook(al::ByamlWriter* saveByml) {

    const char *serverIP = Client::getCurrentIP();
    const int serverPort = Client::getCurrentPort();

    if (serverIP) {
        saveByml->addString("ServerIP", serverIP);
    } else {
        saveByml->addString("ServerIP", "127.0.0.1");
    }

    if (serverPort) {
        saveByml->addInt("ServerPort", serverPort);
    } else {
        saveByml->addInt("ServerPort", 0);
    }

    saveByml->pop();
}

bool saveReadHook(int* padRumbleInt, al::ByamlIter const& saveByml, char const* padRumbleKey) {

    const char *serverIP = "";
    int serverPort = 0;

    if (al::tryGetByamlString(&serverIP, saveByml, "ServerIP")) {
        Client::setLastUsedIP(serverIP);
    }

    if (al::tryGetByamlS32(&serverPort, saveByml, "ServerPort")) {
        Client::setLastUsedPort(serverPort);
    }
    
    return al::tryGetByamlS32(padRumbleInt, saveByml, padRumbleKey);
}

// GameDataFile::tryWriteByByaml
// 
// GameDataFile::tryReadByamlData line 198
void saveFileWriteHook(al::ByamlWriter* saveByaml)
{
    for (int i = 0; i < 17; i++) {
        sead::FixedSafeString<18> label;
        label = "World";
        if (i / 10 > 0) {
            label.append(static_cast<char>(48 + i / 10));
        }
        label.append(static_cast<char>(48 + i % 10));
        label.append("Scenario");
        saveByaml->addInt(label.cstr(), Client::getScenario(i));
    }

    for (int i = 0; i < 25; i++)
    {
        sead::FixedSafeString<15> label;
        label = "ShineChecks";
        if (i / 10 > 0)
        {
            label.append(static_cast<char>(48 + i / 10));
        }
        label.append(static_cast<char>(48 + i % 10));
        saveByaml->addInt(label.cstr(), Client::getShineChecks(i));
    }

    for (int i = 0; i < 12; i++)
    {
        sead::FixedSafeString<16> label;
        label = "OutfitChecks";
        if (i / 10 > 0)
        {
            label.append(static_cast<char>(48 + i / 10));
        }
        label.append(static_cast<char>(48 + i % 10));
        saveByaml->addInt(label.cstr(), Client::getOutfitChecks(i));
    }

    for (int i = 0; i < 4; i++)
    {
        sead::FixedSafeString<17> label;
        label = "StickerChecks";
        if (i / 10 > 0)
        {
            label.append(static_cast<char>(48 + i / 10));
        }
        label.append(static_cast<char>(48 + i % 10));
        saveByaml->addInt(label.cstr(), Client::getStickerChecks(i));
    }

    for (int i = 0; i < 5; i++)
    {
        sead::FixedSafeString<18> label;
        label = "SouvenirChecks";
        if (i / 10 > 0)
        {
            label.append(static_cast<char>(48 + i / 10));
        }
        label.append(static_cast<char>(48 + i % 10));
        saveByaml->addInt(label.cstr(), Client::getSouvenirChecks(i));
    }

    for (int i = 0; i < 8; i++)
    {
        sead::FixedSafeString<19> label;
        label = "CaptureChecks";
        if (i / 10 > 0)
        {
            label.append(static_cast<char>(48 + i / 10));
        }
        label.append(static_cast<char>(48 + i % 10));
        saveByaml->addInt(label.cstr(), Client::getCaptureChecks(i));
    }

    saveByaml->addInt("CheckIndex", Client::getCheckIndex());

    saveByaml->pop();
   
}

bool saveFileReadHook(al::ByamlIter *saveByaml, bool* firstNetworkBool, char const* firstNetworkKey)
{
    int data = 0;

    for (int i = 0; i < 17; i++) {
        sead::FixedSafeString<18> label;
        label = "World";
        if (i / 10 > 0) {
            label.append(static_cast<char>(48 + i / 10));
        }
        label.append(static_cast<char>(48 + i % 10));
        label.append("Scenario");
        if (saveByaml->tryGetIntByKey(&data, label.cstr())) {
            Client::setScenario(i, data);
        }
    }

    /*for (int i = 0; i < 25; i++) {
        sead::FixedSafeString<15> label;
        label = "ShineChecks";
        if (i / 10 > 0) {
            label.append(static_cast<char>(48 + i / 10));
        }
        label.append(static_cast<char>(48 + i % 10));
        if (saveByaml->tryGetIntByKey(&data, label.cstr())) {
            Client::setShineChecks(i, data);
        }
    }*/

    for (int i = 0; i < 8; i++) {
        sead::FixedSafeString<19> label;
        label = "CaptureChecks";
        if (i / 10 > 0) {
            label.append(static_cast<char>(48 + i / 10));
        }
        label.append(static_cast<char>(48 + i % 10));
        if (saveByaml->tryGetIntByKey(&data, label.cstr())) {
            Client::setCaptureChecks(i, data);
        }
    }

    if (saveByaml->tryGetIntByKey(&data, "CheckIndex")) {
        Client::setCheckIndex(data);
    }

    return saveByaml->tryGetBoolByKey(firstNetworkBool, firstNetworkKey);
}


bool registerShineToList(Shine* shineActor) {

    if (shineActor->mShineIdx >= 0) {
        Client::tryRegisterShine(shineActor);
    } 

    return al::isAlive(shineActor);
}

void overrideNerveHook(StageSceneStatePauseMenu* thisPtr, al::Nerve* nrvSet) {

    if (al::isPadHoldZL(-1)) {
        al::setNerve(thisPtr, &nrvStageSceneStatePauseMenuServerConfig);
    } else {
        al::setNerve(thisPtr, nrvSet);
    }
}

StageSceneStateServerConfig *sceneStateServerConfig = nullptr;

void initStateHook(StageSceneStatePauseMenu *thisPtr, char const *stateName, al::Scene *host, al::LayoutInitInfo const &initInfo, FooterParts *footer,
                   GameDataHolder *data, bool unkBool) {
    thisPtr->mStateOption =
        new StageSceneStateOption(stateName, host, initInfo, footer, data, unkBool);

    sceneStateServerConfig = new StageSceneStateServerConfig("ServerConfig", host, initInfo, footer, data, unkBool);
}

void initNerveStateHook(StageSceneStatePauseMenu* stateParent, StageSceneStateOption* stateOption,
                        al::Nerve const* executingNerve, char const* stateName) {

    al::initNerveState(stateParent, stateOption, executingNerve, stateName);

    al::initNerveState(stateParent, sceneStateServerConfig, &nrvStageSceneStatePauseMenuServerConfig, "CustomNerveOverride");
}

// skips starting both coin counters
void startCounterHook(CoinCounter* thisPtr) {
    if (!GameModeManager::instance()->isActive()) {
        thisPtr->tryStart();
    }
}

// Simple hook that can be used to override isModeE3 checks to enable/disable certain behaviors
bool modeE3Hook() {
    return GameModeManager::instance()->isActive();
}

// Skips ending the play guide layout if a mode is active, since the mode would have already ended it
void playGuideEndHook(al::SimpleLayoutAppearWaitEnd* thisPtr) {
    if (!GameModeManager::instance()->isActive()) {
        thisPtr->end();
    }
}

// Gravity Hooks

void initHackCapHook(al::LiveActor *cappy) {
    al::initActorPoseTQGSV(cappy);
}

al::PlayerHolder* createTicketHook(StageScene* curScene) {
    // only creates custom gravity camera ticket if hide and seek mode is active
    if (GameModeManager::instance()->isMode(GameMode::HIDEANDSEEK)) {
        al::CameraDirector* director = curScene->getCameraDirector();
        if (director) {
            if (director->mFactory) {
                al::CameraTicket* gravityCamera = director->createCameraFromFactory(
                    "CameraPoserCustom", nullptr, 0, 5, sead::Matrix34f::ident);

                HideAndSeekMode* mode = GameModeManager::instance()->getMode<HideAndSeekMode>();

                mode->setCameraTicket(gravityCamera);
            }
        }
    }

    return al::getScenePlayerHolder(curScene);
}

bool borderPullBackHook(WorldEndBorderKeeper* thisPtr) {

    bool isFirstStep = al::isFirstStep(thisPtr);

    if (isFirstStep) {
        if (GameModeManager::instance()->isModeAndActive(GameMode::HIDEANDSEEK)) {

            HideAndSeekMode* mode = GameModeManager::instance()->getMode<HideAndSeekMode>();

            if (mode->isUseGravity()) {
                killMainPlayer(thisPtr->mActor);
            }
        }
    }
    
    return isFirstStep;
}

void drawTableHook(al::ExecuteDirector* thisPtr, const al::ExecuteSystemInitInfo &initInfo) {
    
    thisPtr->mUpdateTableCount = updateTableSize;
    thisPtr->mUpdateTables = new al::ExecuteTableHolderUpdate*[thisPtr->mUpdateTableCount]();

    for (int i = 0; i < thisPtr->mUpdateTableCount; i++) {
        thisPtr->mUpdateTables[i] = new al::ExecuteTableHolderUpdate();
        const al::ExecuteTable &curTable = updateTable[i];
        // Logger::log("Update Table Name: %s Count: %d\n", curTable.mName, curTable.mExecuteOrderCount);
        thisPtr->mUpdateTables[i]->init(curTable.mName, initInfo, curTable.mExecuteOrders, curTable.mExecuteOrderCount);
    }

    thisPtr->mDrawTableCount = drawTableSize;
    thisPtr->mDrawTables = new al::ExecuteTableHolderDraw*[thisPtr->mDrawTableCount]();
    
    for (int i = 0; i < thisPtr->mDrawTableCount; i++) {
        thisPtr->mDrawTables[i] = new al::ExecuteTableHolderDraw();
        const al::ExecuteTable* curTable = &drawTable[i];
        // Logger::log("Draw Table Name: %s Count: %d\n", curTable->mName, curTable->mExecuteOrderCount);
        thisPtr->mDrawTables[i]->init(curTable->mName, initInfo, curTable->mExecuteOrders, curTable->mExecuteOrderCount);
    }

    thisPtr->mRequestKeeper = new al::ExecuteRequestKeeper(thisPtr->mRequestMax);
}

void updateStateHook(al::Scene* scene) {
    al::executeUpdateList(scene->mActorKit, "OnlineUpdateExecutors", "PuppetActor");
    rs::updateEffectSystemEnv(scene);
}

void updateDrawHook(al::ExecuteDirector* thisPtr, const char* listName, const char* kit) {
    
    thisPtr->drawList("OnlineDrawExecutors", "PuppetActor");

    Logger::log("Updating Draw List for: %s %s\n", listName, kit);
    thisPtr->drawList(listName, kit);
}