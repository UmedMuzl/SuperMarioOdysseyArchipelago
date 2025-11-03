from enum import Enum
from ctypes import c_short as short, c_ushort as ushort, c_int, c_byte as sbyte, c_ubyte as byte, c_byte, c_ubyte
from typing import Any

class PacketType(Enum):
    Unknown : short = 0
    Init : short = 1
    PlayerInfo : short = 2
    HackCapInfo : short = 3
    GameInfo : short = 4
    TagInfo : short = 5
    Connect : short = 6
    Disconnect : short = 7
    CostumeInfo : short = 8
    Shine : short = 9
    CaptureInfo : short = 10
    ChangeStage : short = 11
    Command : short = 12
    Item : short = 13
    Filler : short = 14
    ArchipelagoChat : short = 15
    SlotData : short = 16
    RegionalCollect : short = 18
    DeathLink : short = 19
    Progress : short = 20
    ShineChecks : short = 21

class ConnectionType(Enum):
    Connect = 0
    Reconnect = 1

#region Check Packets

class ShinePacket:
    id : c_int
    SIZE : short = 4

    def __init__(self, packet_bytes : bytearray = None, shine_id : int = None):
        if packet_bytes:
            self.deserialize(packet_bytes)
        else:
            self.id = c_int(shine_id)

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        int_value : int = self.id.value
        data += int_value.to_bytes(4, "little")
        if len(data) > self.SIZE:
            raise f"ShinePacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        self.id  = c_int(int.from_bytes(data[0:self.SIZE], "little"))

class CheckPacket:
    location_id : int
    item_type : int
    index : int
    amount : int
    SIZE : short = 16

    def __init__(self, packet_bytes : bytearray = None, location_id : int = None, item_type : int = None, index : int = None, amount : int = None):
        if packet_bytes:
            self.deserialize(packet_bytes)
        else:
            self.location_id = location_id
            self.item_type = item_type
            self.index = index
            self.amount = amount

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        data += self.location_id.to_bytes(4, "little")
        data += self.item_type.to_bytes(4, "little")
        data += self.index.to_bytes(4, "little")
        data += self.amount.to_bytes(4, "little")
        if len(data) > self.SIZE:
            raise f"CheckPacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        offset = 0
        self.location_id  = int.from_bytes(data[offset:offset + 4], "little")
        offset += 4
        self.item_type  = int.from_bytes(data[offset:offset + 4], "little")
        offset += 4
        self.index  = int.from_bytes(data[offset:offset + 4], "little")
        offset += 4
        self.amount  = int.from_bytes(data[offset:offset + 4], "little")
        offset += 4


class ShineChecksPacket:
    checks : list[c_int]
    SIZE : short = 4 * 24

    def __init__(self, packet_bytes : bytearray = None, checks : list[int] = None):
        if packet_bytes:
            self.deserialize(packet_bytes)
        else:
            for i in checks:
                self.checks.append(c_int(i))

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        # int_value : int = self.id.value
        # data += int_value.to_bytes(4, "little")
        if len(data) > self.SIZE:
            raise f"ShinePacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        self.checks = []
        offset = 0
        a : c_int = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4
        a = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        self.checks.append(a)
        offset = offset + 4

class ItemPacket:
    ITEM_NAME_SIZE : c_int = 0x80
    name : str
    item_type : c_int
    SIZE : short = 0x84

    def __init__(self, packet_bytes: bytearray = None, name: str = None, item_type: int = None) -> None:
        if packet_bytes:
            self.deserialize(packet_bytes)
        else:
            self.name = name
            self.item_type = c_int(item_type)

    def serialize(self) -> bytearray:
        data: bytearray = bytearray()
        data += self.name.encode()
        while len(data) < self.ITEM_NAME_SIZE:
            data += b"\x00"
        int_value : int = self.item_type.value
        data += int_value.to_bytes(4, "little")
        if len(data) > self.SIZE:
            raise f"ItemPacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        offset : int = 0
        self.name = data[offset:self.ITEM_NAME_SIZE].decode()
        self.name = self.name.replace("\0", "")
        offset += self.ITEM_NAME_SIZE
        self.item_type = c_int(int.from_bytes(data[offset:offset + 4], "little"))

class RegionalCoinPacket:
    OBJECT_ID_SIZE : c_int = 0x10
    STAGE_NAME_SIZE : c_int = 0x30
    object_id : str
    stage_name : str
    SIZE : short

    def serialize(self) -> bytearray:
        data: bytearray = bytearray()
        data += self.object_id.encode()
        while len(data) < self.OBJECT_ID_SIZE:
            data += b"\x00"
        data += self.stage_name.encode()
        while len(data) < self.STAGE_NAME_SIZE + self.OBJECT_ID_SIZE:
            data += b"\x00"
        if len(data) > self.SIZE:
            raise f"RegionalCoinPacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        offset : int = 0
        self.object_id = data[offset:self.OBJECT_ID_SIZE].decode()
        offset += self.OBJECT_ID_SIZE
        self.stage_name = data[offset:offset + self.STAGE_NAME_SIZE].decode()

class FillerPacket:
    item_type : c_int
    SIZE : short = 4

    def __init__(self, packet_bytes : bytearray = None, item_type : int = None):
        if packet_bytes:
            self.deserialize(packet_bytes)
        else:
            self.item_type = c_int(item_type)

    def serialize(self) -> bytearray:
        data: bytearray = bytearray()
        int_value : int = self.item_type.value
        data += int_value.to_bytes(4,"little")
        if len(data) > self.SIZE:
            raise f"FillerPacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)

#endregion

#region Server Packets

class ChatMessagePacket:
    MESSAGE_SIZE : int = 0x4B
    messages : list[str]
    SIZE : short = 0x4B * 3

    def __init__(self, packet_bytes : bytearray = None, messages : list[str] = None):
        if packet_bytes:
            self.deserialize(packet_bytes)
        else:
            self.messages = messages

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        size : int = 0
        for index in range(len(self.messages)):
            for char in self.messages[index]:
                if size < self.MESSAGE_SIZE:
                    data += char.encode()
                else:
                    raise "Message too long exception"

            while len(data) < self.MESSAGE_SIZE * (index + 1):
                data += b"\x00"
        if len(data) > self.SIZE:
            raise f"ChatMessagePacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data


    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)

        offset = 0
        self.messages.append(data[offset:offset + self.MESSAGE_SIZE].decode("utf8"))
        offset += self.MESSAGE_SIZE
        self.messages.append(data[offset:offset + self.MESSAGE_SIZE].decode("utf8"))
        offset += self.MESSAGE_SIZE
        self.messages.append(data[offset:offset + self.MESSAGE_SIZE].decode("utf8"))


class SlotDataPacket:
    clash : ushort
    raid : ushort
    regionals : bool
    captures : bool
    SIZE : short = 6

    def __init__(self, packet_bytes : bytearray = None, clash : int = None, raid : int = None, regionals : bool = None, captures : bool = None):
        if packet_bytes:
            self.deserialize(packet_bytes)
        else:
            self.raid = short(raid)
            self.regionals = regionals
            self.clash = short(clash)
            self.captures = captures

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        int_value : int = self.clash.value
        data += int_value.to_bytes(2, "little")
        int_value = self.raid.value
        data += int_value.to_bytes(2, "little")
        data += self.regionals.to_bytes(1, "little")
        data += self.captures.to_bytes(1, "little")
        if len(data) > self.SIZE:
            raise f"CountsPacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        offset = 0
        self.clash = ushort(int.from_bytes(data[offset:2], "little"))
        offset += 2
        self.raid = ushort(int.from_bytes(data[offset:offset + 2], "little"))
        offset += 2
        self.regionals = bool.from_bytes(data[offset:offset + 1], "little")
        offset += 1
        self.captures = bool.from_bytes(data[offset:offset + 1], "little")





#endregion

class ProgressPacket:
    world_id : c_int
    scenario : c_int
    SIZE : short = 8

    def __init__(self, packet_bytes = None, world_id : int = 0, scenario : int = -1):
        if packet_bytes:
            self.deserialize(packet_bytes)
        else:
            self.world_id = c_int(int(world_id))
            self.scenario = c_int(scenario)

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        int_value : int = self.world_id.value
        data += int_value.to_bytes(4, "little")
        int_value = self.scenario.value
        data += int_value.to_bytes(2, "little")
        if len(data) > self.SIZE:
            raise f"ProgressPacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    # Shouldn't be necessary
    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        offset = 0
        self.world_id = c_int(int.from_bytes(data[offset:offset + 4], "little"))
        offset += 4
        self.scenario = c_int(int.from_bytes(data[offset:offset + 4], "little"))

class ChangeStagePacket:
    ID_SIZE : int  = 0x10
    STAGE_SIZE : int = 0x30
    stage : str
    stage_id : str
    scenario : sbyte
    sub_scenario_type : byte
    SIZE : short = 0x42

    def __init__(self, stage : str, stage_id : str = "", scenario : int = -1, sub_scenario_type : int = 0):
        self.stage = stage
        self.stage_id = stage_id
        self.scenario = c_byte(int(scenario))
        self.sub_scenario_type = c_ubyte(sub_scenario_type)

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        data += self.stage.encode()
        while len(data) < self.STAGE_SIZE:
            data += b"\x00"
        data += self.stage_id.encode()
        while len(data) < self.STAGE_SIZE + self.ID_SIZE:
            data += b"\x00"
        int_value : int = self.scenario.value
        data += int_value.to_bytes(1, "little", signed=True)
        int_value2 : int = self.sub_scenario_type.value
        data += int_value2.to_bytes(1, "little", signed=False)
        if len(data) > self.SIZE:
            raise f"ChangeStagePacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        offset : int = 0
        self.stage = data[offset:self.STAGE_SIZE].decode()
        offset += self.STAGE_SIZE
        self.stage_id = data[offset:offset + self.ID_SIZE].decode()
        offset += self.ID_SIZE
        self.scenario = sbyte(int.from_bytes(data[offset:offset + 1], "little"))
        offset += 1
        self.sub_scenario_type = byte(int.from_bytes(data[offset:offset + 1], "little"))


class DeathLinkPacket:
    SIZE : short = 0x0

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)

#region Connection Packets

class ConnectPacket:
    SIZE : short = 4
    connection_type : ConnectionType

    def __init__(self, packet_bytes : bytearray = None , connection_type : ConnectionType = ConnectionType.Connect):
        if packet_bytes:
            self.deserialize(packet_bytes)
        else:
            self.connection_type = connection_type

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        value = self.connection_type.value
        data += value.to_bytes(4,"little")
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)

        self.connection_type = ConnectionType(int.from_bytes(data[0:4],"little"))

class DisconnectPacket:
    # Empty Packet just to signal disconnect
    SIZE : short = 0

class InitPacket:
    max_players : ushort = ushort(4)
    SIZE : short = 2

    def serialize(self) -> bytearray:
        data : bytearray = bytearray()
        as_integer : int = self.max_players.value
        data += as_integer.to_bytes(2, "little")
        if len(data) > self.SIZE:
            raise f"InitPacket failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data


    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        self.max_players = ushort(int.from_bytes(data[0:self.SIZE], "little"))

#endregion

class PacketHeader:
    GUID_SIZE : int = 16
    guid : bytes
    packet_type : PacketType
    packet_size : short
    SIZE : short = 16 + 4

    def __init__(self, header_bytes : bytearray = None , guid : bytes = None, packet_type : PacketType = PacketType.Init):
        if header_bytes:
            self.deserialize(header_bytes)
        else:
            self.guid = guid
            self.packet_type = packet_type

    def serialize(self) -> bytearray:
        data: bytearray = bytearray()
        data += self.guid
        while len(data) < self.GUID_SIZE:
            data += b"\x00"
        int_value: int = self.packet_type.value
        data += int_value.to_bytes(2, "little")
        int_value2 : int = self.packet_size
        data += int_value2.to_bytes(2, "little")
        if len(data) > self.SIZE:
            raise f"PacketHeader failed to serialize. bytearray exceeds maximum size {self.SIZE}."
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        if data is bytes:
            data = bytearray(data)
        offset = 0
        self.guid = bytes(data[offset:self.GUID_SIZE])
        offset += self.GUID_SIZE
        self.packet_type = PacketType(int.from_bytes(data[offset:offset + 2], "little"))
        offset += 2
        self.packet_size = short(int.from_bytes(data[offset:offset + 2], "little"))

class Packet:
    header : PacketHeader
    packet : Any

    def __init__(self, header_bytes : bytearray = None, guid : bytes = None, packet_type : PacketType = PacketType.Connect, packet_data : list = None):
        if header_bytes:
            self.header = PacketHeader(header_bytes=header_bytes)
        else:
            self.header = PacketHeader(guid=guid, packet_type=packet_type)
            match packet_type:
                case PacketType.Connect:
                    self.packet = ConnectPacket()
                case PacketType.Init:
                    self.packet = InitPacket()
                case PacketType.ChangeStage:
                    self.packet = ChangeStagePacket(stage=packet_data[0], scenario=packet_data[1])
                case PacketType.SlotData:
                    self.packet = SlotDataPacket(clash=packet_data[0], raid=packet_data[1], regionals=packet_data[2])
                case PacketType.ArchipelagoChat:
                    self.packet = ChatMessagePacket(messages=packet_data[0])
                case PacketType.DeathLink:
                    self.packet = DeathLinkPacket()
                case PacketType.Shine:
                    self.packet = ShinePacket(shine_id=packet_data[0])
                case PacketType.Item:
                    self.packet = ItemPacket(name=packet_data[0], item_type=packet_data[1])
                case PacketType.RegionalCollect:
                    self.packet = RegionalCoinPacket()
                case PacketType.Filler:
                    self.packet = FillerPacket(item_type=packet_data[0])
                case PacketType.Progress:
                    self.packet = ProgressPacket(world_id=packet_data[0], scenario=packet_data[1])
                case PacketType.ShineChecks:
                    self.packet = ShineChecksPacket(checks=packet_data[0])

    def serialize(self) -> bytearray:
        self.header.packet_size = self.packet.SIZE
        data : bytearray = bytearray()
        data += self.header.serialize()
        data += self.packet.serialize()
        return data

    def deserialize(self, data : bytes | bytearray) -> None:
        match self.header.packet_type:
            case PacketType.Connect:
                self.packet = ConnectPacket()
            case PacketType.ChangeStage:
                print("Client sending ChangeStagePacket Deprecated")
            # case PacketType.Command:
            #     self.packet = CommandP()
            case PacketType.Shine:
                self.packet = ShinePacket(packet_bytes=data)
            case PacketType.Item:
                self.packet = ItemPacket(packet_bytes=data)
            case PacketType.Filler:
                self.packet = FillerPacket(packet_bytes=data)
            case PacketType.RegionalCollect:
                self.packet = RegionalCoinPacket()
            case PacketType.ArchipelagoChat:
                self.packet = ChatMessagePacket()
            case PacketType.SlotData:
                self.packet = SlotDataPacket(packet_bytes=data)
            case PacketType.DeathLink:
                self.packet = DeathLinkPacket()
            case PacketType.Progress:
                self.packet = ProgressPacket(packet_bytes=data)
            case PacketType.ShineChecks:
                self.packet = ShineChecksPacket(packet_bytes=data)
