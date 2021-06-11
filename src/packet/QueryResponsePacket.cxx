#include "QueryResponsePacket.hxx"

QueryResponsePacket::QueryResponsePacket() {
    this->PacketType = PacketTypeEnum::NodeQueryResponse;
}


void QueryResponsePacket::ParseBuf(uv_buf_t buf) {
    spdlog::critical("parsing is only supposed to do on the client program");
    exit(1);
}

uv_buf_t QueryResponsePacket::SerializeToBuf() {
    uv_buf_t result;
    result.len = 16 + this->Tag.length();
    result.base = new char[result.len];

    if(!result.base) {
        spdlog::critical("memory allocation failed at {} {} {}", __FILE__, __LINE__, __func__);
        exit(1);
    }
    
    char* base = result.base;

    *((int*)base) = (int)(this->PacketType);
    *((long*)(base + 4)) = this->Timestamp;
    *((int*)(base + 12)) = this->Tag.length();

    char* tag_base = base + 16;
    for(std::size_t i=0; i<Tag.length(); i++) {
        tag_base[i] = Tag[i];
    }

    return result;
}