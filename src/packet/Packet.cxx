#include "Packet.hxx"
#include "../util.hxx"

Packet::Packet() {
    this->PacketType = PacketTypeEnum::Invalid;
    this->Timestamp = get_timestamp();
}
