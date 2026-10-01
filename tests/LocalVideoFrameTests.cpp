#include "../src/LocalVideoTap/LocalVideoFrame.h"
#include <cassert>
#include <iostream>
int main(){
    // Two visible pixels per row followed by allocator padding. Values are BGRA.
    const uint8_t padded[]{0,0,255,0, 0,255,0,0, 99,99,99,99,
                           255,0,0,0, 255,255,255,0, 88,88,88,88};
    const std::vector<uint8_t> top{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
    const std::vector<uint8_t> bottom{0,0,255,255, 255,255,255,255, 255,0,0,255, 0,255,0,255};
    std::vector<uint8_t> out;
    assert(LocalVideo::PackBgra(padded,sizeof(padded),2,2,12,false,out) && out==top);
    assert(LocalVideo::PackBgra(padded,sizeof(padded),2,2,12,true,out) && out==bottom);
    assert(!LocalVideo::PackBgra(padded,12,2,2,12,false,out));
    assert(!LocalVideo::PackBgra(padded,sizeof(padded),2,2,4,false,out));
    int64_t due=0;
    assert(LocalVideo::PublicationTime(1000000000,5000000,10000000,due) && due==1000000000);
    assert(LocalVideo::PublicationTime(1000000000,11000000,10000000,due) && due==1001000000);
    assert(!LocalVideo::PublicationTime(1000000000,40000000,10000000,due));
    assert(!LocalVideo::PublicationTime(1000000000,0,110000000,due));
    std::cout<<"PASS: padded rows, channel order, orientation, bounds and late-frame timing\n";
}
