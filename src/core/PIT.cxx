#include "PIT.hxx"
#include "../util.hxx"
#include "../Neighbor.hxx"
#include "CS.hxx"

extern std::string node_name;
extern std::vector<Neighbor*> neighbors;
std::unordered_map<std::string, FreqCountLine*> content_freq_count;

std::string PITLine::ToString() {
    std::string request_face_str = "(";
    for(uv_stream_t* sock : this->request_face) {
        unsigned short sender_port;
        std::string sender_ip = get_sender_ip_and_port((const uv_tcp_t*)sock, &sender_port);
        request_face_str += fmt::format("{}:{}, ", sender_ip, sender_port);
    }
    request_face_str += ')';
    return fmt::format("prefix: {}   request face: {}", this->prefix, request_face_str);
}


void PIT::OnNewRequest(InterestPacket* packet, FIB* fib, uv_stream_t* sock, SendInterestPacketToFunc send_interest_func, SendDataPacketToFunc send_data_func) {
    for(Neighbor* neighbor : neighbors) {
        if(neighbor->ip == packet->from_ip) {
            neighbor->interest_packet_cur_recv_count++;
            neighbor->data_packet_pending_send_count++;
            break;
        }
    }

    if(!content_freq_count.contains(packet->Name)) {
        FreqCountLine* line = new FreqCountLine;
        line->content_name = packet->Name;
        content_freq_count[packet->Name] = line;
    }else{
        content_freq_count[packet->Name]->total++;
    }

    #ifdef NDN_USE_CACHE
        // first check if the request can be served locally
        uv_buf_t cache_buf = CS_get(packet->Name);
        if(cache_buf.base) {
            spdlog::debug("cache hit for {}", packet->Name);
            content_freq_count[packet->Name]->cached++;
            
            DataPacket cache_packet(cache_buf);
            delete[] cache_buf.base;

            send_data_func(sock, cache_packet);
            return ;
        }
    #endif

    // if cache not hit
    bool request_exist = false;
    for(PITLine *line : this->table) {
        if(line->prefix == packet->Name) {
            line->request_face.push_back(sock);

            request_exist = true;
            break;
        }
    }

    // If this is the first time the resource being requested
    if(!request_exist) {
        PITLine *line = new PITLine;
        line->prefix = packet->Name;
        line->request_face.push_back(sock);

        std::vector<FIBLine*> fibLines = fib->LookUp(packet->Name);
        if(fibLines.size() == 0) {
            spdlog::error("can not find route for {} on {}", packet->Name, node_name);
        }else{
            this->table.push_back(line);

            #ifdef NDN_REPLICATE_REQUEST
                for(FIBLine* fibLine : fibLines) {
                    for(Neighbor* neighbor : neighbors) {
                        if(neighbor->ip == fibLine->face) {
                            neighbor->interest_packet_cur_send_count++;
                            neighbor->data_packet_pending_recv_count++;
                            break;
                        }
                    }

                    send_interest_func(fibLine->face, *packet);
                }
            #else
                for(Neighbor* neighbor : neighbors) {
                    if(neighbor->ip == fibLines[0]->face) {
                        neighbor->interest_packet_cur_send_count++;
                        neighbor->data_packet_pending_recv_count++;
                        break;
                    }
                }
                send_interest_func(fibLines[0]->face, *packet);
            #endif
        }
    }

}


void PIT::OnDataReturned(DataPacket* packet, SendDataPacketToFunc func) {
    for(Neighbor* neighbor : neighbors) {
        if(neighbor->ip == packet->from_ip) {
            neighbor->data_packet_pending_recv_count--;
            neighbor->data_packet_cur_recv_count++;
            break;
        }
    }

    #ifdef NDN_USE_CACHE
        uv_buf_t cache_buf = packet->SerializeToBuf();
        CS_store(packet->Name, cache_buf, packet->FreshnessPeriodMs);
        delete[] cache_buf.base;
    #endif

    bool exist_interest = false;

    auto it = this->table.begin();
    while(it != this->table.end()) {
        if(string_begins(packet->Name, (*it)->prefix)) {
            // pending interest matches
            exist_interest = true;

            for(uv_stream_t* sock : (*it)->request_face) {
                std::string ip = get_sender_ip_and_port((const uv_tcp_t*)sock, NULL);

                // count statistics
                for(Neighbor* neighbor : neighbors) {
                    if(neighbor->ip == ip) {
                        neighbor->data_packet_pending_send_count--;
                        neighbor->data_packet_cur_send_count++;
                    }
                }
                func(sock, *packet);
            }
            auto it_to_delete = it++;
            this->table.erase(it_to_delete);

            delete (*it_to_delete);
        }else{
            it++;
        }
    }

    if(!exist_interest) {
        spdlog::error("can not find pending interest when data is returned");
    }
}