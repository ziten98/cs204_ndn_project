#include "CS.hxx"

sw::redis::Redis* redis;


/**
 * Initialize the content store for later use.
 */
void CS_init() {
    redis = new sw::redis::Redis("tcp://127.0.0.1:6379");
}



/**
 * Store data to content store.
 * @param name The path of the content.
 * @param buf The data of the content.
 * @param timeout_ms The freshness period in milliseconds.
 */
void CS_store(std::string name, uv_buf_t buf, long timeout_ms) {
    std::string_view val(buf.base, buf.len);

    redis->set(name, val, std::chrono::milliseconds(timeout_ms));

    spdlog::info("caching {}", name);
}


/**
 * Get data from content store.
 * Do not forget to delete it after use.
 */
uv_buf_t CS_get(std::string name) {
    auto val = redis->get(name);

    uv_buf_t result;
    if(val) {
        result.len = val->size();
        result.base = new char[result.len];

        memcpy(result.base, val->data(), result.len);
    }else{
        result.base = 0;
        result.len = 0;
    }

    return result;
}