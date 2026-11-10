#include "config.hpp"
#include "form_codec.hpp"
#include <Preferences.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>


int checks = 0;
#define CHECK(condition) do { \
    ++checks;                 \ 
    if(!(condition)) {        \
        std::fprintf(stderr,"Failed line %d: %s\n", __LINE__, #condition); \
        std::exit(1); }       \
    }while(0)


int main()
{
    char out[128]; 
    int value=42;
    
    CHECK(FormCodec::decode("Home+Wi-Fi%26%2B%25", out, sizeof(out)) && !strcmp(out,"Home Wi-Fi&+%"));
    CHECK(FormCodec::decode("%D0%9A%D0%BE%D1%82",out,sizeof(out)) && !strcmp(out,"Кот"));
    
    for(const char* s : {"%","%1","%GG","%00","%0A","%7F"}) 
        CHECK(!FormCodec::decode(s,out,sizeof(out)));
    
    CHECK(!FormCodec::decode("abcd",out,4));
    CHECK(FormCodec::decode("abc",out,4) && !strcmp(out,"abc"));
    CHECK(!FormCodec::decode("a",out,0));
    CHECK(FormCodec::integer("-2",value) && value==-2);
    CHECK(FormCodec::integer("2147483647",value) && value==INT_MAX);
    CHECK(FormCodec::integer("-2147483648",value) && value==INT_MIN);

    for(const char* s : {""," "," 1","1 ","1x","2147483648","-2147483649","9999999999999999999999999"}) 
        CHECK(!FormCodec::integer(s,value));
    
    CameraConfig::Data d;
    CHECK(CameraConfig::valid(d));
    
    d.image.size=255; CHECK(!CameraConfig::valid(d)); d.image.size=2;
    d.image.fps=0; CHECK(!CameraConfig::valid(d)); d.image.fps=20;
    d.image.brightness=-3; CHECK(!CameraConfig::valid(d)); d.image.brightness=2;
    d.image.quality=9; CHECK(!CameraConfig::valid(d)); d.image.quality=40;
    
    CHECK(CameraConfig::valid(d));
    strcpy(d.password,"1234567"); 
    
    CHECK(!CameraConfig::valid(d));
    strcpy(d.password,"12345678"); 
    
    CHECK(CameraConfig::valid(d));
    memset(d.password,'a',64); 
    
    d.password[64]=0; 
    CHECK(CameraConfig::valid(d));
    
    d.password[63]='z'; 
    CHECK(!CameraConfig::valid(d));
    
    d.password[63]=0; 
    CHECK(CameraConfig::valid(d));
    
    memset(d.ssid,'s',sizeof(d.ssid)); 
    CHECK(!CameraConfig::valid(d)); 
    d.ssid[32]=0;
    
    strcpy(d.apPassword,"1234567"); 
    CHECK(!CameraConfig::valid(d)); 
    strcpy(d.apPassword,"12345678");
    
    CHECK(CameraConfig::begin());
    CHECK(!strcmp(CameraConfig::get().apPassword,"bowl-camera"));
    strcpy(d.ssid,"Kitchen & cats"); CHECK(CameraConfig::save(d));
    
    CHECK(CameraConfig::begin()); 
    CHECK(!strcmp(CameraConfig::get().ssid,"Kitchen & cats")); 
    CHECK(CameraConfig::get().image.size==2);
    
    Preferences::fail=true; 
    auto changed=d; 
    strcpy(changed.ssid,"Failed write"); 
    CHECK(!CameraConfig::save(changed)); 
    CHECK(!strcmp(CameraConfig::get().ssid,d.ssid)); 
    
    Preferences::fail=false;
    Preferences::bytes[12 + offsetof(CameraConfig::Data, ssid)] ^= 1; 
    
    CHECK(CameraConfig::begin()); 
    CHECK(CameraConfig::get().ssid[0]==0);
    CHECK(CameraConfig::save(d)); 
    
    Preferences::bytes[4] ^= 1; 
    CHECK(CameraConfig::begin()); 
    CHECK(CameraConfig::get().ssid[0]==0);
    
    Preferences::bytes.resize(3); 
    CHECK(CameraConfig::begin()); 
    CHECK(CameraConfig::get().ssid[0]==0);
    
    std::printf("%d checks passed (form decoding, ranges, simulated NVS restart and write failure).\n",checks);
}
