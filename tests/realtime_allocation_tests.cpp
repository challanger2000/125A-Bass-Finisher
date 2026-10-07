#include "TestSupport.h"
#include "MetalFinisherDSP.h"
#include "LowCutMapping.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(_WIN32)
#include <malloc.h>
#endif

using HighGainGuitarFinisher::dsp::MetalFinisherDSP;
using HighGainGuitarFinisher::dsp::lowCutNormalizedFromFrequency;

namespace {
bool gTrack = false;
std::size_t gCount = 0;
void note() noexcept { if (gTrack) ++gCount; }
}

void* operator new(std::size_t size) {
    note();
    if (void* p = std::malloc(size)) return p;
    throw std::bad_alloc {};
}
void* operator new[](std::size_t size) {
    note();
    if (void* p = std::malloc(size)) return p;
    throw std::bad_alloc {};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

#if defined(__cpp_aligned_new)
void* operator new(std::size_t size, std::align_val_t a) {
    note();
#if defined(_WIN32)
    if (void* p = _aligned_malloc(size, static_cast<std::size_t>(a))) return p;
#else
    void* p=nullptr;
    if (posix_memalign(&p, static_cast<std::size_t>(a), size)==0) return p;
#endif
    throw std::bad_alloc {};
}
void* operator new[](std::size_t size, std::align_val_t a) { return ::operator new(size, a); }
void operator delete(void* p, std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
#endif

int main() {
    constexpr double fs = 48000.0;
    constexpr double pi = 3.141592653589793238462643383279502884;

    MetalFinisherDSP dsp;
    dsp.prepare(fs);
    dsp.setFinish(1.0);
    dsp.setMass(1.0);
    dsp.setLowCut(lowCutNormalizedFromFrequency(70.0));
    dsp.setMode(0.5);

    for (int i=0;i<4096;++i) {
        const double t=static_cast<double>(i)/fs;
        double l=0.3*std::sin(2.0*pi*82.41*t)+0.1*std::sin(2.0*pi*1200.0*t);
        double r=0.29*std::sin(2.0*pi*98.0*t)+0.09*std::sin(2.0*pi*1800.0*t);
        dsp.processFrame(l,r);
    }

    gCount=0;
    gTrack=true;
    double checksum=0.0;
    for (int i=0;i<48000;++i) {
        const double t=static_cast<double>(i)/fs;
        double l=0.31*std::sin(2.0*pi*55.0*t)+0.18*std::sin(2.0*pi*110.0*t)+0.08*std::sin(2.0*pi*1800.0*t);
        double r=0.30*std::sin(2.0*pi*61.74*t)+0.17*std::sin(2.0*pi*123.47*t)+0.07*std::sin(2.0*pi*2300.0*t);
        dsp.processFrame(l,r);
        checksum += 0.5*(l+r);
    }
    gTrack=false;

    BF_REQUIRE(std::isfinite(checksum));
    BF_REQUIRE(gCount==0);
    return 0;
}
