#ifdef _GDEXTENSION
#include <godot_cpp/core/memory.hpp>
using namespace godot;
#else
#include "core/os/memory.h"
#endif

inline void tsf_memfree(void* p_ptr) {
	// avoiding godot error when pointer is null
	if (!p_ptr) {
		return;
	}
	memfree(p_ptr);
}

// use godot memory allocator
#define TSF_NO_STDIO
#define TSF_MALLOC memalloc
#define TSF_REALLOC memrealloc
#define TSF_FREE tsf_memfree
#define TSF_MEMCPY memcpy
#define TSF_MEMSET memset

inline double tsf_math_log10(double p_x) {
	return Math::log(p_x) / 2.30258509299;
}
inline float tsf_math_log10(float p_x) {
	return Math::log(p_x) / 2.30258509299f;
}

#define TSF_POW Math::pow
#define TSF_POWF Math::pow
#define TSF_EXPF Math::exp
#define TSF_LOG Math::log
#define TSF_TAN Math::tan
#define TSF_LOG10 tsf_math_log10
#define TSF_SQRT
#define TSF_SQRTF Math::sqrt

// suppress warnings in third-party headers to avoid failing dev_build (llvm & gcc guys)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wnull-pointer-subtraction"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wnull-pointer-subtraction"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#define TSF_IMPLEMENTATION
#include "../thirdparty/tinysoundfont/tsf.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif