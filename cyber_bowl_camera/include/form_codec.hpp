#pragma once
#include <cstddef>


namespace FormCodec 
{
    bool decode(const char* source, char* destination, size_t capacity);
    bool integer(const char* source, int& value);
    bool hexPassword(const char* value);
}
