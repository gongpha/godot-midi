#ifdef _GDEXTENSION
#include <godot_cpp/core/memory.hpp>
using namespace godot;
#else
#include "core/os/memory.h"
#endif

inline void tml_memfree(void* p_ptr) {
	// avoiding godot error when pointer is null
	if (!p_ptr) {
		return;
	}
	memfree(p_ptr);
}

// use godot memory allocator
#define TML_NO_STDIO
#define TML_MALLOC memalloc
#define TML_REALLOC memrealloc
#define TML_FREE tml_memfree
#define TML_MEMCPY memcpy

#define TML_IMPLEMENTATION
#include "../thirdparty/tinysoundfont/tml.h"