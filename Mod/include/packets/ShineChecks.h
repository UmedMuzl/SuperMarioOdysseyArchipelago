#pragma once

#include "Packet.h"

struct PACKED ShineChecks : Packet {
    ShineChecks() : Packet() {
        this->mType = PacketType::SHINECHECKS;
        mPacketSize = sizeof(ShineChecks) - sizeof(Packet);
    };
    int checks1;
    int checks2;
    int checks3;
    int checks4;
    int checks5;
    int checks6;
    int checks7;
    int checks8;
    int checks9;
    int checks10;
    int checks11;
    int checks12;
    int checks13;
    int checks14;
    int checks15;
    int checks16;
    int checks17;
    int checks18;
    int checks19;
    int checks20;
    int checks21;
    int checks22;
    int checks23;
    int checks24;
};