#pragma once

#include "al/actor/ActorInitInfo.h"

class SaveObjInfo {
public:
    struct SaveType;
    createSaveObjInfoWriteSaveData(al::ActorInitInfo const&, al::PlacementId const*);
    createSaveObjInfoNoWriteSaveDataInSameWorld(al::ActorInitInfo const&, al::PlacementId const*);
    createSaveObjInfoNoWriteSaveDataInSameWorldResetMiniGame(al::ActorInitInfo const&, al::PlacementId const*);
    createSaveObjInfoNoWriteSaveDataInSameScenario(al::ActorInitInfo const&, al::PlacementId const*);
    on();
    off();
    isOn();
    resetByPlacementId(al::PlacementId*);
    checkIsOn();
    SaveObjInfo(al::ActorInitInfo const&, al::PlacementId const*, SaveObjInfo::SaveType);
};