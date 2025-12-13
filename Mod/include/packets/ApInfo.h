#pragma once

#include "Packet.h"

struct PACKED ApInfo : Packet {
    ApInfo() : Packet() {
        this->mType = PacketType::APINFO;
        mPacketSize = sizeof(ApInfo) - sizeof(Packet);
    };
    short infoType = -1;
    short index1 = -1;
    short index2 = -1;
    short index3 = -1;
    char info1[APNAMESIZE] = {};
    char info2[APNAMESIZE] = {};
    char info3[APNAMESIZE] = {};
};