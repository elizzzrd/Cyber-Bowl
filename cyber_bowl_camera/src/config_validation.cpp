#include "config.hpp"
#include "form_codec.hpp"
#include <cstring>


bool CameraConfig::valid(const ImageSettings& s)
{
    return s.size <= 2 && s.quality >= 10 && s.quality <= 40 &&
           s.brightness >= -2 && s.brightness <= 2 &&
           s.contrast >= -2 && s.contrast <= 2 &&
           s.saturation >= -2 && s.saturation <= 2 && s.fps >= 1 && s.fps <= 20;
}


bool CameraConfig::valid(const Data& d)
{
    if (!valid(d.image) || !memchr(d.ssid, 0, sizeof(d.ssid)) ||
        !memchr(d.password, 0, sizeof(d.password)) ||
        !memchr(d.apPassword, 0, sizeof(d.apPassword))) return false;

    const size_t len = strlen(d.password);
    return (len == 0 || (len >= 8 && len <= 63) || FormCodec::hexPassword(d.password)) &&
           strlen(d.apPassword) >= 8 && strlen(d.apPassword) <= 63;
}
