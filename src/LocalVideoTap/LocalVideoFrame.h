#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace LocalVideo {
inline bool PublicationTime(int64_t now,int64_t sample,int64_t stream,int64_t& due) {
    const int64_t lead=sample-stream;
    if(lead>20000000 || lead < -100000000)return false;
    due=now+(lead>0?lead:0);
    return true;
}
// Normalize a cached BGRA frame into top-down RGBA; ignore per-row padding.
inline bool PackBgra(const uint8_t* src,size_t size,uint32_t width,uint32_t height,
    uint32_t stride,bool bottomUp,std::vector<uint8_t>& rgba) {
    if(!src || !width || !height || width>4096 || height>4096 || uint64_t(width)*height>3840*2160 ||
        stride<uint64_t(width)*4 || uint64_t(stride)*height>size)return false;
    rgba.resize(size_t(width)*height*4);
    for(uint32_t y=0;y<height;++y){
        const auto* row=src+size_t(bottomUp?height-1-y:y)*stride;
        auto* dst=rgba.data()+size_t(y)*width*4;
        for(uint32_t x=0;x<width;++x){dst[x*4]=row[x*4+2];dst[x*4+1]=row[x*4+1];dst[x*4+2]=row[x*4];dst[x*4+3]=255;}
    }
    return true;
}
}
