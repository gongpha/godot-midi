#include "soundfont2.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/variant/typed_dictionary.h"

#include "tsf_impl.h"

SoundFont2::SoundFont2() {
	soundfont = nullptr;
}

SoundFont2::~SoundFont2() {
	if (soundfont) {
		tsf_close(soundfont);
		soundfont = nullptr;
	}
}

void SoundFont2::_bind_methods() {
	ClassDB::bind_static_method("SoundFont2", D_METHOD("load_from_buffer", "data"), &SoundFont2::load_from_buffer);
	ClassDB::bind_method(D_METHOD("get_preset_list", "bank"), &SoundFont2::get_preset_list);
}

Ref<SoundFont2> SoundFont2::load_from_buffer(const Vector<uint8_t> &p_stream_data) {
	Ref<SoundFont2> sf2;
	sf2.instantiate();
	sf2->soundfont = tsf_load_memory(p_stream_data.ptr(), p_stream_data.size());
	if (!sf2->soundfont) {
		ERR_FAIL_V_MSG(Ref<SoundFont2>(), "Failed to load SoundFont from buffer.");
	}
	return sf2;
}

Dictionary SoundFont2::get_preset_list(int p_bank) const {
	Dictionary result;
	ERR_FAIL_COND_V(!soundfont, result);
	for (int i = 0; i < 128; i++) {
		int idx = tsf_get_presetindex(soundfont, p_bank, i);
		if (idx >= 0) {
			const char *pname = tsf_get_presetname(soundfont, idx);
			result[i] = pname ? String::utf8(pname) : String();
		}
	}
	return result;
}

//

void ResourceFormatLoaderSoundFont::_bind_methods() {
#ifdef _GDEXTENSION
	ClassDB::bind_static_method("ResourceFormatLoaderSoundFont", "load", &ResourceFormatLoaderSoundFont::load);
	ClassDB::bind_method(D_METHOD("get_recognized_extensions"), &ResourceFormatLoaderSoundFont::get_recognized_extensions);
	ClassDB::bind_method(D_METHOD("handles_type", "type"), &ResourceFormatLoaderSoundFont::handles_type);
	ClassDB::bind_method(D_METHOD("get_resource_type", "path"), &ResourceFormatLoaderSoundFont::get_resource_type);
#endif
}

#ifdef _GDEXTENSION

Variant ResourceFormatLoaderSoundFont::_load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const {
	const Vector<uint8_t> stream_data = FileAccess::get_file_as_bytes(p_path);
	ERR_FAIL_COND_V_MSG(stream_data.is_empty(), Ref<SoundFont2>(), vformat("Cannot open file '%s'.", p_path));
	return SoundFont2::load_from_buffer(stream_data);
}

PackedStringArray ResourceFormatLoaderSoundFont::_get_recognized_extensions() const {
	PackedStringArray exts;
	exts.push_back("sf2");
	return exts;
}

bool ResourceFormatLoaderSoundFont::_handles_type(const StringName &type) const {
	return ClassDB::is_parent_class(type, "SoundFont2");
}
String ResourceFormatLoaderSoundFont::_get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() == "sf2") {
		return "SoundFont2";
	}
	return String();
}

#else

Ref<Resource> ResourceFormatLoaderSoundFont::load(
	const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode
) {
	const Vector<uint8_t> stream_data = FileAccess::get_file_as_bytes(p_path);
	ERR_FAIL_COND_V_MSG(stream_data.is_empty(), Ref<SoundFont2>(), vformat("Cannot open file '%s'.", p_path));
	return SoundFont2::load_from_buffer(stream_data);
}

void ResourceFormatLoaderSoundFont::get_recognized_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("sf2");
}

bool ResourceFormatLoaderSoundFont::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "SoundFont2");
}

String ResourceFormatLoaderSoundFont::get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() == "sf2") {
		return "SoundFont2";
	}
	return String();
}

#endif