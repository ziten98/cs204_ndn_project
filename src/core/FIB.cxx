#include "FIB.hxx"
#include "../util.hxx"
#include "../Neighbor.hxx"

extern std::vector<Neighbor*> neighbors;
bool need_announcement_relay = false;

std::string FIBLine::ToString() {
    return fmt::format("prefix: {}\nface: {}\ndelay: {}\nhops: {}\nscore: {}\ndynamic score: {}\ntag:------\n{}------\n", 
                        this->prefix, this->face, this->delay, this->hops, 
                        this->score, this->dynamic_score, this->tag);
}


void FIB::InsertOrUpdate(AnnouncementPacket* packet) {
    // spdlog::info("update FIB table at thread {}", syscall(__NR_gettid));
    FIBLine *line = new FIBLine;
    line->prefix = packet->Name;
    line->face = packet->from_ip;
    line->delay = get_timestamp() - packet->Timestamp + packet->Delay;
    line->hops = packet->Hops + 1;
    line->tag = packet->Tag;


    // find out whether the prefix is already exist, if not, append and return
    std::vector<FIBLine*> prefix_existing_lines;
    for(FIBLine* l : this->table) {
        if(l->prefix == line->prefix) {
            prefix_existing_lines.push_back(l);
        }
    }
    if(prefix_existing_lines.size() == 0) {
        this->table.push_back(line);

        this->ComputeScore();
        this->SortTable();

        need_announcement_relay = true;
        return;
    }

    // if prefix is already exist, check the hops
    if(prefix_existing_lines[0]->hops > line->hops) {
        for(FIBLine* l : prefix_existing_lines) {
            this->table.erase(std::remove(this->table.begin(), this->table.end(), l), this->table.end());
        }
        this->table.push_back(line);

        this->ComputeScore();
        this->SortTable();

        need_announcement_relay = true;
        return;
    }else if(prefix_existing_lines[0]->hops == line->hops){
        // the route with same hops already exists
        #ifdef NDN_LOAD_BALANCE
            bool face_exist = false;
            for(FIBLine* l : prefix_existing_lines) {
                if(l->face == line->face) {
                    face_exist = true;
                    break;
                }
            }
            if(face_exist) {
                delete line;
                return;
            }else {
                this->table.push_back(line);

                this->ComputeScore();
                this->SortTable();
                need_announcement_relay = true;
                return;
            }
        #else
            // do not try to insert multiple routes, do nothing here
        #endif
    }else{
        // the announced route has more hops, discard it
        delete line;
        return;
    }

}


std::vector<FIBLine*> FIB::LookUp(std::string request_path) {
    std::size_t cur_len = 0;
    FIBLine* cur_line = NULL;
    std::vector<FIBLine*> candidates;

    // find the longest prefix
    for(FIBLine* line : this->table) {
        if(string_begins(request_path, line->prefix)) {
            if(line->prefix.length() > cur_len) {
                cur_len = line->prefix.length();
                cur_line = line;

                candidates.clear();
                candidates.push_back(line);
            }else if (line->prefix.length() == cur_len) {
                candidates.push_back(line);
            }
        }
    }

    std::vector<FIBLine*> result;

    #ifdef NDN_LOAD_BALANCE
        // calculate the dynamic score and make a single choice
        for(FIBLine* line : candidates) {
            int pending_recv_count = 0;
            for(Neighbor* neighbor : neighbors) {
                if(neighbor->ip == line->face) {
                    pending_recv_count = neighbor->data_packet_pending_recv_count;
                    break;
                }
            }

            line->dynamic_score = line->score - pending_recv_count * 2;
        }
        std::sort(candidates.begin(), candidates.end(), [](FIBLine* a, FIBLine* b){
            return a->dynamic_score > b->dynamic_score;
        });
        result.push_back(candidates[0]);
    #else
        result.push_back(candidates[0]);
    #endif

    return result;
}


void FIB::ComputeScore() {
    for(FIBLine* line : table) {
        line->score = -line->hops * 30 - line->delay;
    }
}


void FIB::SortTable() {
    std::sort(this->table.begin(), this->table.end(), [](FIBLine* a, FIBLine* b){
        return a->score > b->score;
    });
}