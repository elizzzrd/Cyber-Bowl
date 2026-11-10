#pragma once
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <vector>
namespace Fake {
extern bool nvsFail;
extern unsigned nvsWrites;
extern std::map<std::string, std::vector<uint8_t>> nvs;
}
class Preferences {
public:
    bool begin(const char*, bool) { return true; }
    size_t getBytesLength(const char* key) { return Fake::nvs[key].size(); }
    size_t getBytes(const char* key, void* out, size_t size) {
        if (Fake::nvs[key].size() != size) return 0;
        memcpy(out, Fake::nvs[key].data(), size); return size;
    }
    size_t putBytes(const char* key, const void* data, size_t size) {
        if (Fake::nvsFail) return 0;
        ++Fake::nvsWrites;
        const auto* ptr = static_cast<const uint8_t*>(data);
        Fake::nvs[key] = {ptr, ptr + size}; return size;
    }
};
