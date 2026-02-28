#include "register_types.h"

#ifdef _GDEXTENSION
#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
using namespace godot;
#endif

#include "src/library.h"

void GDXMOD_LIBRARY_INITIALIZE_NAME {
	initialize_library(p_level);
}

void GDXMOD_LIBRARY_UNINITIALIZE_NAME {
	uninitialize_library(p_level);
}

#ifdef _GDEXTENSION
extern "C"
{
	GDExtensionBool GDE_EXPORT library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
	{
		GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
		init_obj.register_initializer(GDXMOD_LIBRARY_INITIALIZE_FUNC);
		init_obj.register_terminator(GDXMOD_LIBRARY_UNINITIALIZE_FUNC);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}
#endif