#include "DataPacket.hxx"


DataPacket::DataPacket() {
    this->PacketType = PacketTypeEnum::Data;
}

DataPacket::DataPacket(uv_buf_t buf) {
    this->PacketType = PacketTypeEnum::Data;
    ParseBuf(buf);
}


void DataPacket::ParseBuf(uv_buf_t buf) {
    char* base = buf.base;
    int NameLength = *((int*)(base + 4));
    int ContentLength = *((int*)(base + 8 + NameLength));
    int SignatureLength = *((int*)(base + 8 + NameLength + 4 + ContentLength + 32));
    this->Timestamp = *((long*)(base + 8 + NameLength + 4 + ContentLength + 36 + SignatureLength));
    this->FreshnessPeriodMs = *((long*)(base + 8 + NameLength + 4 + ContentLength + 36 + SignatureLength + 8));
    int TagLength = *((int*)(base + 8 + NameLength + 4 + ContentLength + 36 + SignatureLength + 16));

    char* name_base = base + 8;
    this->Name.clear();
    this->Name.reserve(NameLength + 10);
    for(int i=0; i<NameLength; i++) {
        this->Name.push_back(name_base[i]);
    }

    char* content_base = base + 8 + NameLength + 4;
    this->Content.clear();
    this->Content.reserve(ContentLength + 10);
    for(int i=0; i<ContentLength; i++) {
        this->Content.push_back(content_base[i]);
    }

    char* sha256_base = base + 8 + NameLength + 4 + ContentLength;
    memcpy(this->SHA256Sum, sha256_base, 32);

    char* signature_base = base + 8 + NameLength + 4 + ContentLength + 36;
    this->Signature.clear();
    this->Signature.resize(SignatureLength);
    memcpy(this->Signature.data(), signature_base, SignatureLength);

    char* tag_base = base + 8 + NameLength + 4 + ContentLength + 36 + SignatureLength + 20;
    this->Tag.clear();
    this->Tag.reserve(TagLength + 10);
    for(int i=0; i<TagLength; i++) {
        this->Tag.push_back(tag_base[i]);
    }
}


uv_buf_t DataPacket::SerializeToBuf() {
    uv_buf_t result;
    result.len = 8 + this->Name.length() + 4 + this->Content.size() + 36 + this->Signature.size() + 20 + this->Tag.length();
    result.base = new char[result.len];

    if(!result.base) {
        spdlog::critical("memory allocation failed at {} {} {}", __FILE__, __LINE__, __func__);
        exit(1);
    }

    char* base = result.base;
    *((int*)base) = (int)(this->PacketType);
    *((int*)(base + 4)) = this->Name.length();
    *((int*)(base + 8 + this->Name.length())) = this->Content.size();
    *((int*)(base + 8 + this->Name.length() + 4 + this->Content.size() + 32)) = this->Signature.size();
    *((long*)(base + 8 + this->Name.length() + 4 + this->Content.size() + 36 + this->Signature.size())) = this->Timestamp;
    *((long*)(base + 8 + this->Name.length() + 4 + this->Content.size() + 36 + this->Signature.size() + 8)) = this->FreshnessPeriodMs;
    *((int*)(base + 8 + this->Name.length() + 4 + this->Content.size() + 36 + this->Signature.size() + 16)) = this->Tag.length();

    char* name_base = base + 8;
    for(int i=0; i<this->Name.length(); i++) {
        name_base[i] = this->Name[i];
    }

    char* content_base = base + 8 + this->Name.length() + 4;
    for(std::size_t i=0; i<this->Content.size(); i++) {
        content_base[i] = this->Content[i];
    }

    char* sha256_base = base + 8 + this->Name.length() + 4 + this->Content.size();
    memcpy(sha256_base, this->SHA256Sum, 32);

    char* signature_base = base + 8 + this->Name.length() + 4 + this->Content.size() + 36;
    memcpy(signature_base, this->Signature.data(), this->Signature.size());

    char* tag_base = base + 8 + this->Name.length() + 4 + this->Content.size() + 36 + this->Signature.size() + 20;
    for(std::size_t i=0; i<this->Tag.length(); i++) {
        tag_base[i] = this->Tag[i];
    }

    return result;
}