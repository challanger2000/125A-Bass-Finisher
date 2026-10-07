#include "TestSupport.h"
#include "BypassCrossfade.h"
#include <algorithm>
#include <iostream>

using HighGainGuitarFinisher::BypassCrossfade;

namespace {
void verify(double fs){
    BypassCrossfade fade;
    fade.prepare(fs,false);
    fade.setBypassed(true);
    int samples=0;
    double prev=0.0;
    while(!fade.fullyBypassed() && samples<static_cast<int>(fs*0.020)){
        const double now=fade.advance();
        BF_REQUIRE(now>=prev);
        prev=now;
        ++samples;
    }
    BF_REQUIRE(fade.fullyBypassed());
    const double ms=1000.0*static_cast<double>(samples)/fs;
    BF_REQUIRE(ms>=4.95 && ms<=5.10);
    fade.setBypassed(false);
    while(fade.mix()>0.0) fade.advance();
    BF_REQUIRE(fade.mix()==0.0);
}
}

int main(){
    for(double fs:{44100.0,48000.0,96000.0,192000.0}) verify(fs);
    BypassCrossfade f;
    f.prepare(48000.0,true);
    BF_REQUIRE(f.mix()==1.0 && f.fullyBypassed());
    f.reset(false);
    BF_REQUIRE(f.mix()==0.0 && !f.fullyBypassed());
    std::cout<<"Bass Finisher bypass crossfade passed\n";
    return 0;
}
