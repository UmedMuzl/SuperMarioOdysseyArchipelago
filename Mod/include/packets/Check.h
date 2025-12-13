#pragma once

#include "Packet.h"

struct PACKED Check : Packet {
    Check() : Packet() {this->mType = PacketType::CHECK; mPacketSize = sizeof(Check) - sizeof(Packet);};
    int locationId = -1;
    int itemType = -1;
    int index = -1;
    char objId[0x10] = {};
    char stage[0x30] = {};
    int amount = -1;
};