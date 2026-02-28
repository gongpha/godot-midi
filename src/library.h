#ifdef _GDEXTENSION
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
using namespace godot;
#else
#include "modules/register_module_types.h"
#include "core/io/resource_loader.h"
#endif

#include "audio_stream_midi.h"
#include "audio_stream_soundfont_player.h"
#include "ui/virtual_keyboard.h"

static Ref<ResourceFormatLoaderMIDI> resource_loader_midi;
static Ref<ResourceFormatLoaderSoundFont> resource_loader_soundfont;

void initialize_library(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(MIDI);
	GDREGISTER_CLASS(ResourceFormatLoaderMIDI);
	GDREGISTER_CLASS(SoundFont2);
	GDREGISTER_CLASS(ResourceFormatLoaderSoundFont);
	GDREGISTER_CLASS(AudioStreamMIDI);
	GDREGISTER_CLASS(AudioStreamPlaybackMIDISF2);
	GDREGISTER_CLASS(AudioStreamSoundfontPlayer);
	GDREGISTER_CLASS(AudioStreamPlaybackSoundfont);
	GDREGISTER_CLASS(VirtualKeyboard);

	resource_loader_midi.instantiate();
	resource_loader_soundfont.instantiate();

#ifdef _GDEXTENSION
	ResourceLoader::get_singleton()->add_resource_format_loader(resource_loader_midi);
	ResourceLoader::get_singleton()->add_resource_format_loader(resource_loader_soundfont);
#else
	ResourceLoader::add_resource_format_loader(resource_loader_midi);
	ResourceLoader::add_resource_format_loader(resource_loader_soundfont);
#endif
}

void uninitialize_library(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

#ifdef _GDEXTENSION
	ResourceLoader::get_singleton()->remove_resource_format_loader(resource_loader_midi);
	ResourceLoader::get_singleton()->remove_resource_format_loader(resource_loader_soundfont);
#else
	ResourceLoader::remove_resource_format_loader(resource_loader_midi);
	ResourceLoader::remove_resource_format_loader(resource_loader_soundfont);
#endif
	resource_loader_midi.unref();
	resource_loader_soundfont.unref();
}