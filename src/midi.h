#pragma once

#ifdef _GDEXTENSION
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_format_loader.hpp>
using namespace godot;
#else
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#endif

class tml_message;

class AudioStreamPlaybackMIDISF2;

class MIDI : public Resource {
	GDCLASS(MIDI, Resource);

	tml_message* midi;

	friend class AudioStreamPlaybackMIDISF2;

protected:
	static void _bind_methods();

public:

	static Ref<MIDI> load_from_buffer(const Vector<uint8_t> &p_stream_data);

	tml_message* get_midi() const {
		return midi;
	}

	MIDI();
	~MIDI();
};

class ResourceFormatLoaderMIDI : public ResourceFormatLoader {
	GDCLASS(ResourceFormatLoaderMIDI, ResourceFormatLoader);

protected:
	static void _bind_methods();

public:
#ifdef _GDEXTENSION
	virtual Variant _load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const;
	virtual PackedStringArray _get_recognized_extensions() const override;
	virtual bool _handles_type(const StringName &type) const override;
	virtual String _get_resource_type(const String &p_path) const override;
#else
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *r_extensions) const;
	virtual bool handles_type(const String &p_type) const;
	virtual String get_resource_type(const String &p_path) const;
#endif
};