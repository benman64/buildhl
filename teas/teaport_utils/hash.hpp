#include <cstdint>
#include <string>

namespace tea {
    uint32_t simple_hash32(const void* data, int len);
    std::string bin2hex_string(const void* bin, int bin_len);
    inline std::string bin2hex_string(const std::string& str) {
        return bin2hex_string(str.c_str(), str.size());
    }
    template<typename T>
    std::string pod2hex_string(const T& pod) {
        return bin2hex_string(&pod, sizeof(pod));
    }

    uint32_t simple_hash32(const void* data_in, int len);
    inline uint32_t simple_hash32(const std::string& str) {
        return simple_hash32(str.c_str(), str.size());
    }
}