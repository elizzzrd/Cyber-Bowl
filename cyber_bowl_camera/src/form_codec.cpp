#include "form_codec.hpp"
#include <climits>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cctype>


namespace 
{
    int hex(char ch)
    {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    }
}


bool FormCodec::decode(const char* source, char* destination, size_t capacity)
{
    if (!source || !destination || capacity == 0) 
        return false;
    
    size_t pos = 0;
    
    while (*source) 
    {
        unsigned char ch = static_cast<unsigned char>(*source++);
    
        if (ch == '+') 
            ch = ' ';
        else if (ch == '%') 
        {
            if (!source[0] || !source[1]) 
                return false;
                
            const int hi = hex(source[0]), lo = hex(source[1]);
            if (hi < 0 || lo < 0) 
                return false;
            
            ch = static_cast<unsigned char>(hi * 16 + lo);
            source += 2;
        }

        if (ch == 0 || ch < 32 || ch == 127 || pos + 1 >= capacity) 
            return false;
        destination[pos++] = static_cast<char>(ch);
    }

    destination[pos] = '\0';
    return true;
}


bool FormCodec::integer(const char* source, int& value)
{
    if (!source || !*source || 
        std::isspace(static_cast<unsigned char>(*source))) 
        return false;
    
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(source, &end, 10);

    if (errno == ERANGE || *end || 
        parsed < INT_MIN || parsed > INT_MAX) 
        return false;
    
    value = static_cast<int>(parsed);
    return true;
}


bool FormCodec::hexPassword(const char* value)
{
    if (strlen(value) != 64) 
        return false;
    
    for (size_t i = 0; i < 64; ++i) {
        if (hex(value[i]) < 0) 
            return false;
    }

    return true;
}