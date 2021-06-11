#include "headers.hxx"
#include "definitions.hxx"
#include "util.hxx"
#include "packet/InterestPacket.hxx"
#include "packet/AnnouncementPacket.hxx"
#include "packet/DataPacket.hxx"
#include "packet/QueryResponsePacket.hxx"
#include "Neighbor.hxx"
#include "core/FIB.hxx"
#include "core/PIT.hxx"
#include "core/CS.hxx"

// The global loop object, will be used everywhere
uv_loop_t *loop = new uv_loop_t;
// The timer used to relay announcement
uv_timer_t *announcement_relay_timer = new uv_timer_t;
// Whether we need to make announcement relay, only do so if the route has been updated.
extern bool need_announcement_relay;
// The node name of this server;
std::string node_name;

std::vector<Neighbor*> neighbors;
FIB fib;
PIT pit;
extern std::unordered_map<std::string, FreqCountLine*> content_freq_count;

/**
 * The callback for announcement relay connect trying.
 * Stop announcing if announcement succeeds and the table is not updated.
 */
void ndn_announcement_connect_cb(uv_connect_t* con, int status) {
    if(status == 0) {
        need_announcement_relay = false;
    }

    ndn_connect_cb_send_close(con, status);
}

/**
 * Make prefix announcement relay to all neighbors except the source neighbor at certain interval.
 */
void ndn_announcement_relay_timer_cb(uv_timer_t* handle) {
    if(need_announcement_relay) {
        need_announcement_relay = false;

        spdlog::info("relay announcement at thread {}", syscall(__NR_gettid));
        for(Neighbor *neighbor : neighbors) {
            // only send announcement to router
            if(neighbor->name[0] != 'R') {
                continue;
            }

            struct sockaddr_in send_addr;
            uv_ip4_addr(neighbor->ip.c_str(), NDN_NODE_LISTEN_TCP_PORT, &send_addr);

            for(FIBLine *line : fib.table) {
                if(neighbor->ip == line->face) {
                    continue;
                }
                AnnouncementPacket packet;
                packet.Name = line->prefix;
                packet.Tag = line->tag + fmt::format("router: {}\n", node_name);
                packet.Hops = line->hops;
                packet.Delay = line->delay;

                uv_buf_t *buf = new uv_buf_t;
                *buf = packet.SerializeToBuf();
                
                uv_tcp_t *sock = new uv_tcp_t;
                uv_tcp_init(loop, sock);

                uv_connect_t *con = new uv_connect_t;
                con->data = buf;
                uv_tcp_connect(con, sock, (const sockaddr*)&send_addr, ndn_announcement_connect_cb);

            }
        }
    }
}



/**
 * Handler when a query packet is received
 */
void on_node_query(uv_stream_t* sock) {
    spdlog::info("received a node query packet");
    QueryResponsePacket packet;
    packet.Tag = fmt::format("Node Name: {}\n\n", node_name);
    
    packet.Tag += "=== Neighbors ===\n";
    for(Neighbor* neighbor : neighbors) {
        packet.Tag += fmt::format("{}\n", neighbor->name);
        packet.Tag += fmt::format("   ip: {}\n", neighbor->ip);
        packet.Tag += fmt::format("   interest packet: \n");
        packet.Tag += fmt::format("         current receive count: {}\n", neighbor->interest_packet_cur_recv_count);
        packet.Tag += fmt::format("         current send count: {}\n", neighbor->interest_packet_cur_send_count);
        packet.Tag += fmt::format("   data packet: \n");
        packet.Tag += fmt::format("         current receive count: {}\n", neighbor->data_packet_cur_recv_count);
        packet.Tag += fmt::format("         current send count: {}\n", neighbor->data_packet_cur_send_count);
        packet.Tag += fmt::format("         pending receive count: {}\n", neighbor->data_packet_pending_recv_count);
        packet.Tag += fmt::format("         pending send count: {}\n", neighbor->data_packet_pending_send_count);
    }
    packet.Tag += "\n\n";

    packet.Tag += "Content served frequency: \n";
    std::vector<FreqCountLine*> vec_freq;
    for(auto it : content_freq_count) {
        vec_freq.push_back(it.second);
    }
    std::sort(vec_freq.begin(), vec_freq.end(), [](FreqCountLine* a, FreqCountLine* b){
        return a->total > b->total;
    });

    std::size_t max_freq_print = std::min((std::size_t)20, vec_freq.size());
    for(std::size_t i=0; i<max_freq_print; i++) {
        packet.Tag += fmt::format("total: {}  cached: {} prefix: {}\n", vec_freq[i]->total, vec_freq[i]->cached, vec_freq[i]->content_name);
    }
    packet.Tag += "\n\n";

    packet.Tag += "=== Forwarding Information Base ===\n";
    for(FIBLine *line : fib.table) {
        packet.Tag += line->ToString();
        packet.Tag += '\n';
    }
    packet.Tag += "\n\n";

    packet.Tag += "=== Pending Interest Table ===\n";
    for(PITLine *line : pit.table) {
        packet.Tag += line->ToString();
        packet.Tag += '\n';
    }

    uv_buf_t buf = packet.SerializeToBuf();

    uv_write_t* send_req = new uv_write_t;

    uv_write(send_req, sock, &buf, 1, ndn_write_cb_close);

    delete[] buf.base;
}


/**
 * Handler when an announcement packet is received
 */
void on_announcement(AnnouncementPacket* packet) {
    spdlog::info("received an announcement packet from {}", packet->from_ip);
    spdlog::debug("name: {} hops: {} delay: {} tag: \n{}", packet->Name, packet->Hops, packet->Delay, packet->Tag);

    fib.InsertOrUpdate(packet);
}


void ndn_read_cb(uv_stream_t* sock, ssize_t nread, const uv_buf_t* buf);

/**
 * The callback function whenever a shutdown operation is finished.
 * A read operation will start if shutdown succeeds.
 * It will delete the uv_shutdown_t* req.
 */
void ndn_shutdown_cb_read(uv_shutdown_t* req, int status) {
    if(status < 0) {
        spdlog::critical("write error: {}", uv_strerror(status));
        exit(1);
    }

    uv_buf_t *buf = new uv_buf_t;
    buf->base = 0;
    buf->len = 0;

    req->handle->data = buf;

    uv_read_start((uv_stream_t*)req->handle, ndn_alloc_cb, ndn_read_cb);

    delete req;
}

/**
 * The callback function whenever a write operation is finished.
 * The tcp connection will be shutdown.
 * It will delete the uv_write_t* req.
 */
void ndn_write_cb_shutdown(uv_write_t* req, int status) {

    if(status < 0) {
        spdlog::critical("write error: {}", uv_strerror(status));
        exit(1);
    }

    uv_shutdown_t* shutdown_req = new uv_shutdown_t;
    uv_shutdown(shutdown_req, req->handle, ndn_shutdown_cb_read);

    delete req;
}


/**
 * The callback function whenever a connect operation is finished.
 * Then buf is send and we begin to read data in the write callback function.
 * The buf is assumed to be in the con->data, which is of uv_buf_t* type.
 */
void ndn_connect_cb_send_read(uv_connect_t* con, int status) {
    if(status < 0) {
        spdlog::error("connect error: {}", uv_strerror(status));

        uv_buf_t *buf = (uv_buf_t *)con->data;
        delete[] buf->base;
        delete buf;
        
        if (!uv_is_closing((uv_handle_t*)con->handle)) {
            uv_close((uv_handle_t*)con->handle, ndn_close_cb);
        }
        return;
    }

    uv_buf_t *buf = (uv_buf_t *)con->data;
    

    uv_write_t *req = new uv_write_t;
    uv_write(req, con->handle, buf, 1, ndn_write_cb_shutdown);

    delete[] buf->base;
    delete buf;
    delete con;
}



/**
 * Send interest packet to a specific IP. 
 */
void send_interest_packet_to(std::string ip, InterestPacket packet) {
    spdlog::info("send interest packet to {}", ip);

    packet.Timestamp = get_timestamp();
    packet.Tag += fmt::format("via {}\n", node_name);

    uv_buf_t *buf = new uv_buf_t;
    *buf = packet.SerializeToBuf();

    uv_tcp_t *sock = new uv_tcp_t;
    uv_tcp_init(loop, sock);

    struct sockaddr_in dest_addr;
    uv_ip4_addr(ip.c_str(), NDN_NODE_LISTEN_TCP_PORT, &dest_addr);

    uv_connect_t *con = new uv_connect_t;
    con->data = buf;
    uv_tcp_connect(con, sock, (const sockaddr*)&dest_addr, ndn_connect_cb_send_read);
    
}


/**
 * Send data packet to a specific IP. 
 */
void send_data_packet_to(uv_stream_t* sock, DataPacket packet) {
    unsigned short peer_port;
    std::string peer_ip = get_sender_ip_and_port((const uv_tcp_t*)sock, &peer_port);
    spdlog::info("send data packet to {}:{}", peer_ip, peer_port);

    packet.Timestamp = get_timestamp();
    packet.Tag += fmt::format("via {}\n", node_name);

    uv_buf_t buf = packet.SerializeToBuf();

    uv_write_t *req = new uv_write_t;
    uv_write(req, sock, &buf, 1, ndn_write_cb_close);

    delete[] buf.base;
}


/**
 * Handler when a data packet is received
 */
void on_data(DataPacket* packet) {
    spdlog::info("received a data packet from {}", packet->from_ip);
    
    pit.OnDataReturned(packet, send_data_packet_to);
}


/**
 * Handler when an interest packet is received
 */
void on_interest(InterestPacket* packet, uv_stream_t* sock) {
    spdlog::info("received an interest packet from {}: {}", packet->from_ip, packet->Name);

    pit.OnNewRequest(packet, &fib, sock, send_interest_packet_to, send_data_packet_to);
}



/**
 * The callback function whenever a tcp data block is read.
 * Assume the data field of uv_stream_t* sock is a pointer to uv_buf_t.
 * Continue read and concatenate the data until the EOF. Then start processing.
 */
void ndn_read_cb(uv_stream_t* sock, ssize_t nread, const uv_buf_t* buf) {
    if(nread == 0) {
        return;
    }else if(nread > 0) {
        uv_buf_t old_buf = *((uv_buf_t*)sock->data);
        uv_buf_t new_buf;
        new_buf.len = nread + old_buf.len;
        new_buf.base = new char[new_buf.len];

        #ifdef NDN_DEBUG
            spdlog::debug("concatenate tcp receive buf: size {} -> size {}", old_buf.len, new_buf.len);
        #endif

        if(!new_buf.base) {
            spdlog::critical("memory allocation failed");
            exit(1);
        }

        if(old_buf.len != 0) {
            memcpy(new_buf.base, old_buf.base, old_buf.len);
            delete[] old_buf.base;
        }

        memcpy(new_buf.base + old_buf.len, buf->base, nread);

        *((uv_buf_t*)sock->data) = new_buf;

        delete[] buf->base;
    }else {
        if(nread != UV_EOF) {
            // there is an error
            spdlog::error("read error {}", uv_strerror(nread));
            if (!uv_is_closing((uv_handle_t*)sock)) {
                uv_close((uv_handle_t*)sock, ndn_close_cb);
            }
            return;
        }else {
            // handling buffer
            uv_buf_t whole_buf = *((uv_buf_t*)sock->data);
            delete (uv_buf_t*)sock->data;

            int packet_type = *((int*)(whole_buf.base));
            unsigned short sender_port;
            std::string sender_ip = get_sender_ip_and_port((const uv_tcp_t*)sock, &sender_port);
            switch (packet_type) {
                case (int)PacketTypeEnum::Announcement:
                {
                    AnnouncementPacket* packet = new AnnouncementPacket(whole_buf);
                    packet->from_ip = sender_ip;
                    packet->from_port = sender_port;
                    on_announcement(packet);
                    if (!uv_is_closing((uv_handle_t*)sock)) {
                        uv_close((uv_handle_t*)sock, ndn_close_cb);
                    }
                }
                    break;
                case (int)PacketTypeEnum::Data:
                {
                    DataPacket* packet = new DataPacket(whole_buf);
                    packet->from_ip = sender_ip;
                    packet->from_port = sender_port;
                    on_data(packet);
                    if (!uv_is_closing((uv_handle_t*)sock)) {
                        uv_close((uv_handle_t*)sock, ndn_close_cb);
                    }
                }
                    break;
                case (int)PacketTypeEnum::Interest:
                {
                    InterestPacket* packet = new InterestPacket(whole_buf);
                    packet->from_ip = sender_ip;
                    packet->from_port = sender_port;
                    on_interest(packet, sock);
                }
                    break;
                case (int)PacketTypeEnum::NodeQuery:
                {
                    on_node_query(sock);
                    break;
                }
                case (int)PacketTypeEnum::Invalid:
                default:
                    spdlog::error("receive invalid packet of type {}", packet_type);
                    if (!uv_is_closing((uv_handle_t*)sock)) {
                        uv_close((uv_handle_t*)sock, ndn_close_cb);
                    }
                    break;
            }
            
            delete[] whole_buf.base;
        }
    }

}


/**
 * The callback called when a client is connected.
 */
void ndn_being_connected_cb(uv_stream_t* listen_server, int status) {
    if(status < 0) {
        // there is a transmission error
        spdlog::critical("transmission error {} in line {} {} {}", uv_strerror(status) , __LINE__, __FILE__, __func__);
        exit(1);
    }

    uv_tcp_t *sock = new uv_tcp_t;
    uv_tcp_init(loop, sock);

    int accept_result = uv_accept(listen_server, (uv_stream_t*)sock);
    if(accept_result != 0) {
        spdlog::error("accept error {}", uv_strerror(accept_result));

        if (!uv_is_closing((uv_handle_t*)sock)) {
            uv_close((uv_handle_t*)sock, ndn_close_cb);
        }
        return;
    }

    #ifdef NDN_DEBUG
        unsigned short sender_port;
        std::string sender_ip = get_sender_ip_and_port(sock, &sender_port);
        spdlog::debug("receive tcp connection from {}:{}", sender_ip, sender_port);
    #endif
    

    uv_buf_t *buf = new uv_buf_t;
    buf->base = 0;
    buf->len = 0;

    sock->data = buf;

    uv_read_start((uv_stream_t*)sock, ndn_alloc_cb, ndn_read_cb);
}


/**
 * Initialize server to listen on specific UDP port
 */
void init() {
    // read topo.json
    spdlog::info("loading topo.json");
    std::string topo_string = read_file_to_string("./topo.json");
    rapidjson::Document d;
    d.Parse(topo_string.c_str());
    node_name = d["name"].GetString();
    spdlog::info("current node: {}", node_name);

    const rapidjson::Value& json_neighbors = d["neighbors"];
    if(!json_neighbors.IsArray()) {
        spdlog::critical("unexpecetd json format in topo.json");
        exit(1);
    }

    for(unsigned int i=0; i<json_neighbors.Size(); i++) {
        Neighbor *neighbor = new Neighbor;
        neighbor->ip = json_neighbors[i]["ip"].GetString();
        neighbor->name = json_neighbors[i]["name"].GetString();
        neighbors.push_back(neighbor);

        spdlog::info("{} neighbor: {} {}", node_name, neighbor->name, neighbor->ip);
    }


    // initialize tcp server
    uv_tcp_t* listen_socket = new uv_tcp_t;
    uv_tcp_init(loop, listen_socket);
    struct sockaddr_in recv_addr;

    spdlog::info("server listen on tcp port {}", NDN_NODE_LISTEN_TCP_PORT);
    uv_ip4_addr("0.0.0.0", NDN_NODE_LISTEN_TCP_PORT, &recv_addr);
    uv_tcp_bind(listen_socket, (const struct sockaddr*)&recv_addr, 0);
    
    int listen_result = uv_listen((uv_stream_t*)listen_socket, 1024, ndn_being_connected_cb);

    if(listen_result) {
        spdlog::critical("listen error {}", uv_strerror(listen_result));
        exit(1);
    }

    // initialize timer
    uv_timer_init(loop, announcement_relay_timer);
    uv_timer_start(announcement_relay_timer, ndn_announcement_relay_timer_cb, NDN_ROUTER_ANNOUNCEMENT_RELAY_INTERVAL_MS, NDN_ROUTER_ANNOUNCEMENT_RELAY_INTERVAL_MS);


    // initialize content store
    CS_init();
}

int main() {
    init_loggers();
    
    uv_loop_init(loop);

    init();

    spdlog::info("uv loop running");
    uv_run(loop, UV_RUN_DEFAULT);
    spdlog::info("uv loop closing");

    uv_loop_close(loop);
    free(loop);
    
    return 0;
}