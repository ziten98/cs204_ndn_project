#include "headers.hxx"
#include "definitions.hxx"
#include "util.hxx"
#include "packet/InterestPacket.hxx"
#include "packet/AnnouncementPacket.hxx"
#include "packet/DataPacket.hxx"
#include "Neighbor.hxx"

// The global loop object, will be used everywhere
uv_loop_t *loop = new uv_loop_t;
// The node name of this server;
std::string node_name;

std::vector<Neighbor*> neighbors;
bool stop_annoncement_timer = false;

// rsa is used for encrypt
RSA* rsa;


/**
 * The callback for announcement connect trying.
 * Stop the timer if announcement succeeds.
 */
void ndn_announcement_connect_cb(uv_connect_t* con, int status) {
    if(status == 0) {
        stop_annoncement_timer = true;
    }

    ndn_connect_cb_send_close(con, status);
}


/**
 * Make prefix announcement to all neighbors at certain interval.
 */
void ndn_announcement_timer_cb(uv_timer_t *handle) {
    if(stop_annoncement_timer) {
        uv_timer_stop(handle);
        if (!uv_is_closing((uv_handle_t*)handle)) {
            uv_close((uv_handle_t*)handle, ndn_close_cb);
        }
    }

    spdlog::info("making prefix announcement on /{}", node_name);

    for(Neighbor *neighbor : neighbors) {
        // only announce to routers
        if(neighbor->name[0] != 'R') {
            continue;
        }
        AnnouncementPacket packet;
        packet.Name = fmt::format("/{}", node_name);
        packet.Tag += fmt::format("source: {}\n", node_name);
        packet.Hops = 0;
        packet.Delay = 0;

        uv_buf_t *buf = new uv_buf_t;
        *buf = packet.SerializeToBuf();
        spdlog::debug("send buffer of data: \n {:a}", spdlog::to_hex(buf->base, buf->base + buf->len));

        uv_tcp_t *sock = new uv_tcp_t;
        uv_tcp_init(loop, sock);

        struct sockaddr_in dest_addr;
        uv_ip4_addr(neighbor->ip.c_str(), NDN_NODE_LISTEN_TCP_PORT, &dest_addr);

        uv_connect_t *con = new uv_connect_t;
        con->data = buf;
        uv_tcp_connect(con, sock, (const sockaddr*)&dest_addr, ndn_announcement_connect_cb);
    }

}


/**
 * Handler when an interest packet is received
 */
void on_interest(InterestPacket* packet, uv_stream_t* sock) {
    spdlog::info("received an interest packet of path {}", packet->Name);

    if(!string_begins(packet->Name, "/" + node_name)) {
        spdlog::error("received an interest packet of path {} that do not in the domain I serve, which is {}", packet->Name, "/" + node_name);
        return;
    }

    // skip the prefix and the slash
    std::string local_path = string_skip_front(packet->Name, node_name.length() + 2);

    std::size_t slash_pos = local_path.find('/');
    if(slash_pos == std::string::npos) {
        spdlog::error("malformed request path: {}", packet->Name);
        return;
    }
    std::string filename = local_path.substr(0, slash_pos);

    // compute chunks information
    uv_buf_t file_buf;

    try{
        std::string filepath = "./files/" + filename;
        unsigned long filesize = std::filesystem::file_size(std::filesystem::path(filepath));
        int max_chunk_id = 0;
        if(filesize % NDN_MAX_FILE_CHUNK == 0) {
            max_chunk_id = filesize / NDN_MAX_FILE_CHUNK - 1;
        }else {
            max_chunk_id = filesize / NDN_MAX_FILE_CHUNK;
        }

        std::string request_type = local_path.substr(slash_pos + 1);
        if(string_begins(request_type, "chunks_count")) {

            file_buf.len = 4;
            file_buf.base = new char[4];
            *((int*)(file_buf.base)) = max_chunk_id + 1;

        }else if(string_begins(request_type, "chunk/")) {

            std::string chunk_id_str = request_type.substr(std::strlen("chunk/"));
            int chunk_id = std::stoi(chunk_id_str);

            if (chunk_id > max_chunk_id) {
                spdlog::error("malformed request path: {}", packet->Name);
                return;
            }

            int read_length = 0;
            if(chunk_id < max_chunk_id) {
                read_length = NDN_MAX_FILE_CHUNK;
            }else{
                read_length = filesize - NDN_MAX_FILE_CHUNK * max_chunk_id;
            }

            spdlog::debug("read file chunk {}", chunk_id);
            file_buf = read_file_to_binary(filepath.c_str(), chunk_id * NDN_MAX_FILE_CHUNK, read_length);
        }else{
            spdlog::error("malformed request path: {}", packet->Name);
            return;
        }

    }catch(std::exception& e) {
        spdlog::error("malformed request path: {}", packet->Name);
        return;
    }
    
    
    // create data packet
    DataPacket data_packet;
    data_packet.Name = packet->Name;
    data_packet.Content.resize(file_buf.len);
    for(int i=0; i<file_buf.len; i++) {
        data_packet.Content[i] = file_buf.base[i];
    }
    delete[] file_buf.base;

    SHA256((unsigned char*)data_packet.Content.data(), data_packet.Content.size(), data_packet.SHA256Sum);
    uv_buf_t signature_buf = create_signature((unsigned char*)data_packet.Content.data(), data_packet.Content.size(), rsa);
    data_packet.Signature.resize(signature_buf.len);
    memcpy(data_packet.Signature.data(), signature_buf.base, signature_buf.len);
    delete[] signature_buf.base;

    data_packet.FreshnessPeriodMs = NDN_DATA_DEFAULT_FRESHNESS_MS;
    data_packet.Tag = fmt::format("From {}\n", node_name);
    uv_buf_t buf = data_packet.SerializeToBuf();

    uv_write_t *write_req = new uv_write_t;
    uv_write(write_req, sock, &buf, 1, ndn_write_cb_close);

    delete[] buf.base;
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
        }else{
            // handling buffer
            uv_buf_t whole_buf = *((uv_buf_t*)sock->data);
            delete (uv_buf_t*)sock->data;

            int packet_type = *((int*)(whole_buf.base));
            unsigned short sender_port;
            std::string sender_ip = get_sender_ip_and_port((const uv_tcp_t*)sock, &sender_port);
            switch (packet_type) {
                case (int)PacketTypeEnum::Interest:
                {
                    InterestPacket* packet = new InterestPacket(whole_buf);
                    packet->from_ip = sender_ip;
                    packet->from_port = sender_port;
                    on_interest(packet, sock);
                }
                    break;
                default:
                    spdlog::info("received a packet of unknown type {}", packet_type);
                    break;
            }

            delete[] whole_buf.base;

            if (!uv_is_closing((uv_handle_t*)sock)) {
                uv_close((uv_handle_t*)sock, ndn_close_cb);
            }
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
    std::string topo_string;
    try{
        topo_string = read_file_to_string("./topo.json");
    }catch(NDNException& err) {
        spdlog::critical(err.what());
        exit(1);
    }
    
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


    // read rsa signature
    rsa = load_private_key("./private.pem");

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

    // initialize timer to announce prefix
    uv_timer_t *announcement_timer = new uv_timer_t;
    uv_timer_init(loop, announcement_timer);
    uv_timer_start(announcement_timer, ndn_announcement_timer_cb, NDN_SERVER_ANNOUNCEMENT_INTERVAL_MS, NDN_SERVER_ANNOUNCEMENT_INTERVAL_MS);
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