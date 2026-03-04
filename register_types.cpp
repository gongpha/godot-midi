#include "register_types.h"

#ifdef _GDEXTENSION
#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
using namespace godot;
#else
#include "modules/register_module_types.h"
#endif

#include "src/library.h"

void initialize_midi_module(ModuleInitializationLevel p_level) {
	initialize_library_midi(p_level);
}
void uninitialize_midi_module(ModuleInitializationLevel p_level) {
	uninitialize_library_midi(p_level);
}

#ifdef _GDEXTENSION
extern "C"
{
	GDExtensionBool GDE_EXPORT library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
	{
		GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
		init_obj.register_initializer(initialize_midi_module);
		init_obj.register_terminator(uninitialize_midi_module);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}
#endif