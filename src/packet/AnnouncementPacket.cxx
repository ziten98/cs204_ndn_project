#include "AnnouncementPacket.hxx"

AnnouncementPacket::AnnouncementPacket() {
    this->PacketType = PacketTypeEnum::Announcement;
}

AnnouncementPacket::AnnouncementPacket(uv_buf_t buf) {
    this->PacketType = PacketTypeEnum::Announcement;
    ParseBuf(buf);
}


void AnnouncementPacket::ParseBuf(uv_buf_t buf) {
    char* base = buf.base;
    int NameLength = *((int*)(base + 4));
    this->Hops = *((int*)(base + 8 + NameLength));
    this->Delay = *((int*)(base + 8 + NameLength + 4));
    this->Timestamp = *((long*)(base + 8 + NameLength + 8));
    int TagLength = *((int*)(base + 8 + NameLength + 16));

    this->Name.clear();
    this->Name.reserve(NameLength + 10);
    char* name_begin = base + 8;
    for(int i=0; i<NameLength; i++) {
        this->Name.push_back(name_begin[i]);
    }

    this->Tag.clear();
    this->Tag.reserve(TagLength + 10);
    char* tag_begin = base + 8 + NameLength + 20;
    for(int i=0; i<TagLength; i++) {
        this->Tag.push_back(tag_begin[i]);
    }
}


uv_buf_t AnnouncementPacket::SerializeToBuf() {
    uv_buf_t result;
    result.len = 8 + this->Name.length() + 20 + this->Tag.length();
    result.base = new char[result.len];

    if(!result.base) {
        spdlog::critical("memory allocation failed at {} {} {}", __FILE__, __LINE__, __func__);
        exit(1);
    }

    char* base = result.base;
    *((int*)base) = (int)(this->PacketType);
    *((int*)(base + 4)) = this->Name.length();
    *((int*)(base + 8 + this->Name.length())) = this->Hops;
    *((int*)(base + 8 + this->Name.length() + 4)) = this->Delay;
    *((long*)(base + 8 + this->Name.length() + 8)) = this->Timestamp;
    *((int*)(base + 8 + this->Name.length() + 16)) = this->Tag.length();

    char* name_begin = base + 8;
    for(std::size_t i=0; i<this->Name.length(); i++) {
        name_begin[i] = this->Name[i];
    }

    char* tag_begin = base + 8 + this->Name.length() + 20;
    for(std::size_t i=0; i<this->Tag.length(); i++) {
        tag_begin[i] = this->Tag[i];
    }

    return result;
}