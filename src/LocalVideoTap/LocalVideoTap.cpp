#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dshow.h>
#include <dvdmedia.h>
#include <initguid.h>
#include <streams.h>
#include "LocalVideoFrame.h"
#include <libyuv/convert_argb.h>
#include <libyuv/row.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// A separate frame tap with its own RGB32 output allocator. GPUPixel remains independent.
DEFINE_GUID(CLSID_MagpieLocalVideoTap, 0x671bf4b2, 0x6da8, 0x4f3b, 0xa1, 0x74, 0x7c, 0x89, 0x37, 0xc5, 0x61, 0xc2);
namespace {
constexpr wchar_t Name[] = L"Magpie Local Video Tap";
constexpr uint32_t MaxBytes = 3840 * 2160 * 4;
struct FrameHeader {
    uint32_t magic, version, width, height, stride, bytes;
    uint64_t sequence;
    int64_t timestamp100ns, ptsMicroseconds;
    uint32_t format, matrix, fullRange, reserved;
};
static_assert(sizeof(FrameHeader) == 64);
struct Status {
    uint32_t magic = 0x31564c4d, version = 1, pid = GetCurrentProcessId(), running = 0;
    uint32_t width = 0, height = 0, format = 0, reserved = 0;
    uint64_t sequence = 0;
    int64_t heartbeat = 0, interval = 0;
    wchar_t mapName[96]{};
    uint32_t sampleBytes=0, bitmapBytes=0, bitmapWidth=0, bitmapHeight=0;
    int64_t sampleTime=0, streamTime=0, arrival=0;
    uint64_t received=0;
};
static_assert(offsetof(Status, mapName) == 56);
int64_t Now() { LARGE_INTEGER n,f; QueryPerformanceCounter(&n); QueryPerformanceFrequency(&f); return n.QuadPart / f.QuadPart * 10000000 + n.QuadPart % f.QuadPart * 10000000 / f.QuadPart; }
bool TryLock(HANDLE mutex, DWORD timeout = 0) { const auto result=WaitForSingleObject(mutex,timeout);return result==WAIT_OBJECT_0 || result==WAIT_ABANDONED; }
struct Frame { FrameHeader header{}; int64_t due = 0; uint64_t generation = 0; std::vector<uint8_t> bytes; };

const BITMAPINFOHEADER* Bitmap(const CMediaType& mt) {
    if(*mt.FormatType()==FORMAT_VideoInfo && mt.FormatLength()>=sizeof(VIDEOINFOHEADER))return &reinterpret_cast<const VIDEOINFOHEADER*>(mt.Format())->bmiHeader;
    if(*mt.FormatType()==FORMAT_VideoInfo2 && mt.FormatLength()>=sizeof(VIDEOINFOHEADER2))return &reinterpret_cast<const VIDEOINFOHEADER2*>(mt.Format())->bmiHeader;
    return nullptr;
}
REFERENCE_TIME Interval(const CMediaType& mt) {
    if(*mt.FormatType()==FORMAT_VideoInfo)return reinterpret_cast<const VIDEOINFOHEADER*>(mt.Format())->AvgTimePerFrame;
    if(*mt.FormatType()==FORMAT_VideoInfo2)return reinterpret_cast<const VIDEOINFOHEADER2*>(mt.Format())->AvgTimePerFrame;
    return 0;
}
class Tap final : public CTransformFilter {
public:
    static CUnknown* WINAPI Create(LPUNKNOWN outer, HRESULT* hr) { try { auto* tap=new Tap(outer,hr); if(hr)*hr=S_OK; return tap; } catch (...) { if(hr)*hr=E_OUTOFMEMORY; return nullptr; } }
    Tap(LPUNKNOWN outer,HRESULT* hr) : CTransformFilter(NAME("Magpie Local Video Tap"),outer,CLSID_MagpieLocalVideoTap) {
        const auto suffix = std::to_wstring(GetCurrentProcessId());
        statusName_ = L"Local\\MagpieLocalVideo_" + suffix;
        statusMutex_ = CreateMutexW(nullptr,FALSE,(statusName_+L"_Mutex").c_str());
        statusMap_ = CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Status),statusName_.c_str());
        if(statusMap_) status_ = static_cast<Status*>(MapViewOfFile(statusMap_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Status)));
        GUID guid{}; CoCreateGuid(&guid); const auto* p = reinterpret_cast<const uint8_t*>(&guid);
        mapName_ = L"Local\\MagpieBrowserFrame_";
        for(int i=0;i<16;++i){mapName_ += L"0123456789ABCDEF"[p[i]>>4];mapName_ += L"0123456789ABCDEF"[p[i]&15];}
        enabled_ = CreateEventW(nullptr,TRUE,FALSE,(L"Local\\MagpieLocalVideoEnabled_"+suffix).c_str());
        if(status_ && statusMutex_){WaitForSingleObject(statusMutex_,INFINITE);*status_=Status{};wcscpy_s(status_->mapName,mapName_.c_str());ReleaseMutex(statusMutex_);}
        worker_ = std::thread([this]{Worker();});
    }
    ~Tap() override {
        {std::lock_guard<std::mutex> lock(mutex_); quitting_=true; running_=false; queue_.clear();}
        ready_.notify_all(); if(worker_.joinable()) worker_.join();
        if(view_) UnmapViewOfFile(view_); if(map_) CloseHandle(map_); if(frameMutex_) CloseHandle(frameMutex_);
        if(status_) UnmapViewOfFile(status_); if(statusMap_) CloseHandle(statusMap_); if(statusMutex_) CloseHandle(statusMutex_); if(enabled_) CloseHandle(enabled_);
    }
    HRESULT CheckInputType(const CMediaType* mt) override {
        if(!mt || *mt->Type()!=MEDIATYPE_Video)return VFW_E_TYPE_NOT_ACCEPTED;
        const auto& t=*mt->Subtype();
        const auto* b=Bitmap(*mt);
        if(!b || b->biWidth<64 || b->biHeight==LONG_MIN || std::abs(b->biHeight)<64 || int64_t(b->biWidth)*std::abs(b->biHeight)>3840*2160)return VFW_E_TYPE_NOT_ACCEPTED;
        if(t!=MEDIASUBTYPE_RGB32 && t!=MEDIASUBTYPE_ARGB32 && ((b->biWidth&1) || (b->biHeight&1)))return VFW_E_TYPE_NOT_ACCEPTED;
        return t==MEDIASUBTYPE_NV12 || t==MEDIASUBTYPE_YV12 || t==MEDIASUBTYPE_RGB32 || t==MEDIASUBTYPE_ARGB32 || t.Data1==MAKEFOURCC('I','4','2','0') || t==MEDIASUBTYPE_IYUV ? S_OK : VFW_E_TYPE_NOT_ACCEPTED;
    }
    HRESULT CheckTransform(const CMediaType* input,const CMediaType* output) override {
        const auto* a=Bitmap(*input);const auto* b=Bitmap(*output);
        return CheckInputType(input)==S_OK && a && b && *output->Subtype()==MEDIASUBTYPE_RGB32 && a->biWidth==b->biWidth && std::abs(a->biHeight)==std::abs(b->biHeight) ? S_OK : VFW_E_TYPE_NOT_ACCEPTED;
    }
    HRESULT GetMediaType(int position,CMediaType* mt) override {
        if(position<0)return E_INVALIDARG;if(position>1)return VFW_S_NO_MORE_ITEMS;
        if(!m_pInput || !m_pInput->IsConnected())return VFW_E_NOT_CONNECTED;
        const auto& in=m_pInput->CurrentMediaType();const auto* b=Bitmap(in);if(!b)return E_FAIL;
        const int w=b->biWidth,h=std::abs(b->biHeight);
        mt->InitMediaType();mt->SetType(&MEDIATYPE_Video);mt->SetSubtype(&MEDIASUBTYPE_RGB32);mt->SetTemporalCompression(FALSE);mt->SetSampleSize(w*h*4);
        BITMAPINFOHEADER* out=nullptr;
        if(position==0){mt->SetFormatType(&FORMAT_VideoInfo2);auto* v=reinterpret_cast<VIDEOINFOHEADER2*>(mt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER2)));if(!v)return E_OUTOFMEMORY;ZeroMemory(v,sizeof(*v));v->AvgTimePerFrame=Interval(in);v->rcSource=v->rcTarget={0,0,w,h};v->dwPictAspectRatioX=w;v->dwPictAspectRatioY=h;out=&v->bmiHeader;}
        else{mt->SetFormatType(&FORMAT_VideoInfo);auto* v=reinterpret_cast<VIDEOINFOHEADER*>(mt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));if(!v)return E_OUTOFMEMORY;ZeroMemory(v,sizeof(*v));v->AvgTimePerFrame=Interval(in);v->rcSource=v->rcTarget={0,0,w,h};out=&v->bmiHeader;}
        out->biSize=sizeof(*out);out->biWidth=w;out->biHeight=-h;out->biPlanes=1;out->biBitCount=32;out->biCompression=BI_RGB;out->biSizeImage=w*h*4;
        return S_OK;
    }
    HRESULT DecideBufferSize(IMemAllocator* allocator,ALLOCATOR_PROPERTIES* properties) override {
        const auto* b=Bitmap(m_pOutput->CurrentMediaType());if(!b)return E_FAIL;
        properties->cBuffers=2;properties->cbBuffer=b->biWidth*std::abs(b->biHeight)*4;properties->cbAlign=1;
        ALLOCATOR_PROPERTIES actual{};auto hr=allocator->SetProperties(properties,&actual);
        return FAILED(hr)?hr:(actual.cbBuffer<properties->cbBuffer?E_FAIL:S_OK);
    }
    STDMETHODIMP Run(REFERENCE_TIME start) override {auto hr=CTransformFilter::Run(start);if(SUCCEEDED(hr)){running_=true;ready_.notify_all();}return hr;}
    STDMETHODIMP Pause() override {Clear();return CTransformFilter::Pause();}
    STDMETHODIMP Stop() override {Clear();return CTransformFilter::Stop();}
    HRESULT BeginFlush() override {Clear();return CTransformFilter::BeginFlush();}
    HRESULT EndFlush() override {auto hr=CTransformFilter::EndFlush();running_=m_State==State_Running;return hr;}
    HRESULT NewSegment(REFERENCE_TIME start,REFERENCE_TIME stop,double rate) override {
        {std::lock_guard<std::mutex> lock(mutex_);queue_.clear();++generation_;}
        return CTransformFilter::NewSegment(start,stop,rate);
    }
    HRESULT Transform(IMediaSample* sample,IMediaSample* output) override {
        if(status_ && TryLock(statusMutex_)){++status_->received;status_->arrival=Now();ReleaseMutex(statusMutex_);}
        const auto& in=m_pInput->CurrentMediaType();const auto& out=m_pOutput->CurrentMediaType();
        const auto* b=Bitmap(in);const auto* ob=Bitmap(out);if(!b || !ob)return E_FAIL;
        const int w=b->biWidth,h=std::abs(b->biHeight),size=w*h*4;
        BYTE* src=nullptr;BYTE* dst=nullptr;if(FAILED(sample->GetPointer(&src)) || FAILED(output->GetPointer(&dst)))return E_FAIL;
        const int actual=sample->GetActualDataLength();const bool rgb=*in.Subtype()==MEDIASUBTYPE_RGB32 || *in.Subtype()==MEDIASUBTYPE_ARGB32;
        const int rows=rgb?h:h*3/2;if(actual<rows*(rgb?w*4:w) || actual%rows)return E_FAIL;
        const int stride=actual/rows;try { converted_.resize(size); } catch (...) { return E_OUTOFMEMORY; }
        if(rgb){for(int y=0;y<h;++y)memcpy(converted_.data()+y*w*4,src+(b->biHeight>0?h-1-y:y)*stride,w*4);}
        else {
            const auto* matrix=w>=1280?&libyuv::kYuvH709Constants:&libyuv::kYuvI601Constants;int result;
            if(*in.Subtype()==MEDIASUBTYPE_NV12)result=libyuv::NV12ToARGBMatrix(src,stride,src+stride*h,stride,converted_.data(),w*4,matrix,w,h);
            else {const bool yv=*in.Subtype()==MEDIASUBTYPE_YV12;result=libyuv::I420ToARGBMatrix(src,stride,src+stride*h+(yv?stride*h/4:0),stride/2,src+stride*h+(yv?0:stride*h/4),stride/2,converted_.data(),w*4,matrix,w,h);}
            if(result)return E_FAIL;
        }
        const int outputBytes=output->GetSize(),outputStride=outputBytes%h==0?outputBytes/h:w*4;
        if(outputStride<w*4 || outputStride> w*4+8192 || outputStride*h>outputBytes)return E_FAIL;
        for(int y=0;y<h;++y)memcpy(dst+(ob->biHeight>0?h-1-y:y)*outputStride,converted_.data()+y*w*4,w*4);
        output->SetActualDataLength(outputStride*h);
        REFERENCE_TIME start,end;if(SUCCEEDED(sample->GetTime(&start,&end)))output->SetTime(&start,&end);else output->SetTime(nullptr,nullptr);
        if(SUCCEEDED(sample->GetMediaTime(&start,&end)))output->SetMediaTime(&start,&end);else output->SetMediaTime(nullptr,nullptr);
        output->SetSyncPoint(sample->IsSyncPoint()==S_OK);output->SetDiscontinuity(sample->IsDiscontinuity()==S_OK);output->SetPreroll(sample->IsPreroll()==S_OK);
        if(!running_ || !enabled_ || WaitForSingleObject(enabled_,0)!=WAIT_OBJECT_0 || sample->IsPreroll()==S_OK) return S_OK;
        try { Submit(sample,w,h,Interval(in),converted_.data()); } catch (...) { /* A failed enhancement must not stop local playback. */ }
        return S_OK;
    }
private:
    void Clear(){running_=false;{std::lock_guard<std::mutex> lock(mutex_);queue_.clear();++generation_;}ready_.notify_all();}
    void Submit(IMediaSample* sample,uint32_t w,uint32_t h,int64_t interval,const uint8_t* src) {
        const auto& mt=m_pInput->CurrentMediaType();const auto* b=Bitmap(mt);
        if(status_ && TryLock(statusMutex_)){status_->reserved=mt.Subtype()->Data1;status_->sampleBytes=sample->GetActualDataLength();status_->bitmapBytes=b->biSizeImage;status_->bitmapWidth=b->biWidth;status_->bitmapHeight=b->biHeight;ReleaseMutex(statusMutex_);}
        REFERENCE_TIME start=0,end=0;CRefTime stream;
        if(FAILED(sample->GetTime(&start,&end)) || FAILED(StreamTime(stream)))return;
        if(status_ && TryLock(statusMutex_)){status_->sampleTime=start;status_->streamTime=stream;ReleaseMutex(statusMutex_);}
        Frame f;
        if(!LocalVideo::PublicationTime(Now(),start,stream,f.due))return;
        f.header={0x31465642,1,w,h,w*4,w*h*4,0,f.due,start/10,0,1,1,0};
        if(!LocalVideo::PackBgra(src,size_t(w)*h*4,w,h,w*4,false,f.bytes))return;
        std::lock_guard<std::mutex> lock(mutex_);f.generation=generation_;interval_=interval;
        if(queue_.size()>=8)queue_.pop_front();queue_.push_back(std::move(f));ready_.notify_all();
    }
    void Worker() {
        std::unique_lock<std::mutex> lock(mutex_);
        while(!quitting_){
            if(status_ && statusMutex_ && TryLock(statusMutex_)){status_->running=running_?1:0;status_->heartbeat=Now();status_->interval=interval_;ReleaseMutex(statusMutex_);}
            if(queue_.empty() || !running_){ready_.wait_for(lock,std::chrono::milliseconds(30));continue;}
            const auto delay=queue_.front().due-Now();
            if(delay>0){ready_.wait_for(lock,std::chrono::microseconds(std::min<int64_t>(delay/10,30000)));continue;}
            Frame f=std::move(queue_.front());queue_.pop_front();
            if(f.generation!=generation_ || !running_ || WaitForSingleObject(enabled_,0)!=WAIT_OBJECT_0)continue;
            if(!map_){frameMutex_=CreateMutexW(nullptr,FALSE,(mapName_+L"_Mutex").c_str());map_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,MaxBytes+64,mapName_.c_str());if(map_)view_=static_cast<uint8_t*>(MapViewOfFile(map_,FILE_MAP_ALL_ACCESS,0,0,0));}
            // The reader briefly owns this mutex while uploading the previous frame.
            // A bounded wait avoids dropping a decoded frame for a sub-millisecond collision.
            if(!view_ || !frameMutex_ || !TryLock(frameMutex_,2))continue;
            f.header.sequence=++sequence_; memcpy(view_+64,f.bytes.data(),f.bytes.size());memcpy(view_,&f.header,64);ReleaseMutex(frameMutex_);
            if(status_ && TryLock(statusMutex_)){status_->width=f.header.width;status_->height=f.header.height;status_->format=f.header.format;status_->sequence=sequence_;ReleaseMutex(statusMutex_);}
        }
    }
    HANDLE statusMutex_=nullptr,statusMap_=nullptr,enabled_=nullptr,frameMutex_=nullptr,map_=nullptr;
    Status* status_=nullptr;uint8_t* view_=nullptr;std::wstring statusName_,mapName_;
    std::mutex mutex_;std::condition_variable ready_;std::deque<Frame> queue_;std::thread worker_;
    std::vector<uint8_t> converted_;
    std::atomic<bool> running_{false};bool quitting_=false;uint64_t generation_=0,sequence_=0;int64_t interval_=0;
};
const AMOVIESETUP_MEDIATYPE MediaType={&MEDIATYPE_Video,&MEDIASUBTYPE_NULL};
const AMOVIESETUP_PIN Pins[]={ {L"Input",FALSE,FALSE,FALSE,FALSE,&CLSID_NULL,nullptr,1,&MediaType},{L"Output",FALSE,TRUE,FALSE,FALSE,&CLSID_NULL,nullptr,1,&MediaType} };
const AMOVIESETUP_FILTER Setup={&CLSID_MagpieLocalVideoTap,Name,MERIT_DO_NOT_USE,2,Pins};
}
CFactoryTemplate g_Templates[]={{Name,&CLSID_MagpieLocalVideoTap,Tap::Create,nullptr,&Setup}};
int g_cTemplates=sizeof(g_Templates)/sizeof(g_Templates[0]);
STDAPI DllRegisterServer(){return AMovieDllRegisterServer2(TRUE);}
STDAPI DllUnregisterServer(){return AMovieDllRegisterServer2(FALSE);}
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE,DWORD,LPVOID);
BOOL APIENTRY DllMain(HINSTANCE module,DWORD reason,LPVOID reserved){return DllEntryPoint(module,reason,reserved);}
