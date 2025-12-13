#pragma once

#include "Packet.h"

struct PACKED SlotData : Packet {
    SlotData() : Packet() {
        this->mType = PacketType::SLOTDATA;
        mPacketSize = sizeof(SlotData) - sizeof(Packet);
    };
    ushort cascade = 0;
    ushort sand = 0;
    ushort wooded = 0;
    ushort lake = 0;
    ushort lost = 0;
    ushort metro = 0;
    ushort seaside = 0;
    ushort snow = 0;
    ushort luncheon = 0;
    ushort ruined = 0;
    ushort bowser = 0;
    ushort dark = 0;
    ushort darker = 0;
    bool regionals = false;
    bool captures = false;
};