#pragma once
#include <vector>
#include <cstring>

class Preferences 
{
    public:
        static inline std::vector<unsigned char> bytes;
        static inline bool fail = false;
        bool begin(const char*, bool) { return true; }
        
        size_t getBytesLength(const char*) 
            { return bytes.size(); }
        
            size_t getBytes(const char*, void* target, size_t size) 
        { 
            if(size != bytes.size()) 
                return 0; 
            memcpy(target,bytes.data(),size); 
            return size; 
        }
        
        size_t putBytes(const char*, const void* source, size_t size) 
        { 
            if(fail) 
                return 0; 
            const auto* p=static_cast<const unsigned char*>(source); 
            bytes.assign(p,p+size); 
            return size; 
        }
};
