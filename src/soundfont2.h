#pragma once

#ifdef _GDEXTENSION
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_format_loader.hpp>
using namespace godot;
#else
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#endif

class tsf;

class AudioStreamPlaybackMIDISF2;

class SoundFont2 : public Resource {
	GDCLASS(SoundFont2, Resource);

	tsf* soundfont;

	friend class AudioStreamPlaybackMIDISF2;

protected:
	static void _bind_methods();

public:

	static Ref<SoundFont2> load_from_buffer(const Vector<uint8_t> &p_stream_data);

	Dictionary get_preset_list(int p_bank) const;

	tsf* get_soundfont() const {
		return soundfont;
	}

	SoundFont2();
	~SoundFont2();
};

class ResourceFormatLoaderSoundFont : public ResourceFormatLoader {
	GDCLASS(ResourceFormatLoaderSoundFont, ResourceFormatLoader);

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