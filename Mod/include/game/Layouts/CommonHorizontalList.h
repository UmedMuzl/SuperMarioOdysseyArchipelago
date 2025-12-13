#pragma once

#include "al/layout/LayoutActor.h"
#include "al/nerve/NerveExecutor.h"
#include "nn/ui2d/Texture.h"
#include "prim/seadSafeString.h"

class CommonHorizontalList : public al::NerveExecutor {
public:
    void setEnableData(bool*);
};