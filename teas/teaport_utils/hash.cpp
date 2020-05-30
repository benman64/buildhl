#include "hash.hpp"

#include <vector>

namespace tea {
    static const char* hex_map = "01234567890abcdef";
    void bin2hex(const void* bin_in, char* hex, int bin_len) {
        const uint8_t* bin = reinterpret_cast<const uint8_t*>(bin_in);
        for (int i = 0; i < bin_len; ++i) {
             int hi = (bin[i] >> 4) & 0xF;
             int lo = bin[i] & 0xF;

             hex[i*2] = hex_map[hi];
             hex[i*2+1] = hex_map[lo];
        }
        hex[bin_len*2] = 0;
    }

    std::string bin2hex_string(const void* bin, int bin_len) {
        std::vector<char> hex_vec;
        hex_vec.resize(bin_len*2+1, 0);
        bin2hex(bin, &hex_vec[0], bin_len);

        return &hex_vec[0];
    }


    static int primes[] = {
        3,  5,   7, 11,
        13, 17, 19, 23,
        29, 31, 37, 41,
        43, 47, 53, 59,

        61,     67,     71,     73,
        79,     83,     89,     97,
        101,    103,    107,    109,
        113,    127,    131,    137
    };

    static uint32_t pow32(uint32_t x, int y) {
        uint32_t result = 1;
        for (int i = 0; i < y; ++i) {
            result *= x;
        }
        return result;
    }
    uint32_t simple_hash32(const void* data_in, int len) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(data_in);
        uint32_t hash = 0;

        for (int i = 0; i < len; ++i) {
            uint32_t next = data[i];

            uint32_t hi_prime = primes[(next & 0xF0 >> 7)];
            uint32_t lo_prime = primes[next & 0xF];
            next = next << ((i % 4)*8);
            hash ^= next;

            pow32(hash, lo_prime);
            hash *= hi_prime;
        }
        return hash;
    }
}