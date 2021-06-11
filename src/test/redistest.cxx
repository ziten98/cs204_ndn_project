#include "../headers.hxx"
#include "../definitions.hxx"

// test the functionality of redis
#define TEST_BUFFER_SIZE 10000

int main() {
    using namespace sw::redis;

    auto redis = Redis("tcp://127.0.0.1:6379");

    redis.set("string", "hello world", std::chrono::milliseconds(100));
    auto str_val = redis.get("string");
    if(str_val && *str_val == "hello world") {
        spdlog::info("string test succeed");
    }else{
        spdlog::info("string test failed");
    }

    uv_sleep(200);
    str_val = redis.get("string");
    if(!str_val) {
        spdlog::info("string timeout test succeed");
    }else{
        spdlog::info("string timeout test failed");
    }

    char* data = new char[TEST_BUFFER_SIZE];

    srand(time(NULL));
    for(int i=0; i<TEST_BUFFER_SIZE; i++) {
        data[i] = (char)(rand() % 256);
    }

    redis.set("binary", StringView(data, TEST_BUFFER_SIZE), std::chrono::milliseconds(100));
    auto bin_val = redis.get("binary");
    
    if(bin_val && bin_val->size() == TEST_BUFFER_SIZE && memcmp(data, bin_val->data(), TEST_BUFFER_SIZE) == 0) {
        spdlog::info("binary test succeed");
    }else{
        spdlog::info("binary test failed");
    }

    uv_sleep(200);

    bin_val = redis.get("binary");
    if(!bin_val) {
        spdlog::info("binary timeout test succeed");
    }else{
        spdlog::info("binary timeout test failed");
    }

    return 0;
}