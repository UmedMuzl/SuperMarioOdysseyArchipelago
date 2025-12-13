#pragma once

#include "al/actor/ActorDimensionKeeper.h"
#include "types.h"

#include "game/Interfaces/IUseDimension.h"
#include "al/LiveActor/LiveActor.h"

class GrowFlowerPot : public al::LiveActor, public IUseDimension {
    public:
    void* qword110;
    al::PlacementId* mPlacementId;
};