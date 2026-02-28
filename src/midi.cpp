#include "midi.h"

#ifdef _GDEXTENSION
#else
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#endif

#include "tml_impl.h"

MIDI::MIDI() {
	midi = nullptr;
}

MIDI::~MIDI() {
	if (midi) {
		tml_free(midi);
		midi = nullptr;
	}
}

#ifdef _GDEXTENSION
void MIDI::_bind_methods() {
	ClassDB::bind_static_method("MIDI", D_METHOD("load_from_buffer", "data"), &MIDI::load_from_buffer);
}

Ref<MIDI> MIDI::load_from_buffer(const PackedByteArray &p_stream_data) {
	Ref<MIDI> m;
	m.instantiate();
	m->midi = tml_load_memory(p_stream_data.ptr(), p_stream_data.size());
	if (!m->midi) {
		ERR_FAIL_V_MSG(Ref<MIDI>(), "Failed to load MIDI from buffer.");
	}
	return m;
}
#else
void MIDI::_bind_methods() {
	ClassDB::bind_static_method("MIDI", D_METHOD("load_from_buffer", "data"), &MIDI::load_from_buffer);
}

Ref<MIDI> MIDI::load_from_buffer(const Vector<uint8_t> &p_stream_data) {
	Ref<MIDI> m;
	m.instantiate();
	m->midi = tml_load_memory(p_stream_data.ptr(), p_stream_data.size());
	if (!m->midi) {
		ERR_FAIL_V_MSG(Ref<MIDI>(), "Failed to load MIDI from buffer.");
	}
	return m;
}
#endif

//

void ResourceFormatLoaderMIDI::_bind_methods() {
}

#ifdef _GDEXTENSION

Variant ResourceFormatLoaderMIDI::_load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const {
	const PackedByteArray stream_data = FileAccess::get_file_as_bytes(p_path);
	ERR_FAIL_COND_V_MSG(stream_data.is_empty(), Ref<MIDI>(), vformat("Cannot open file '%s'.", p_path));
	return MIDI::load_from_buffer(stream_data);
}

PackedStringArray ResourceFormatLoaderMIDI::_get_recognized_extensions() const {
	PackedStringArray exts;
	exts.push_back("mid");
	exts.push_back("midi");
	return exts;
}

bool ResourceFormatLoaderMIDI::_handles_type(const StringName &type) const {
	return ClassDB::is_parent_class(type, "MIDI");
}
String ResourceFormatLoaderMIDI::_get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() == "mid") {
		return "MIDI";
	}
	if (p_path.get_extension().to_lower() == "midi") {
		return "MIDI";
	}
	return String();
}

#else

Ref<Resource> ResourceFormatLoaderMIDI::load(
	const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode
) {
	const Vector<uint8_t> stream_data = FileAccess::get_file_as_bytes(p_path);
	ERR_FAIL_COND_V_MSG(stream_data.is_empty(), Ref<MIDI>(), vformat("Cannot open file '%s'.", p_path));
	return MIDI::load_from_buffer(stream_data);
}

void ResourceFormatLoaderMIDI::get_recognized_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("mid");
	r_extensions->push_back("midi");
}

bool ResourceFormatLoaderMIDI::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "MIDI");
}

String ResourceFormatLoaderMIDI::get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() == "mid") {
		return "MIDI";
	}
	if (p_path.get_extension().to_lower() == "midi") {
		return "MIDI";
	}
	return String();
}

#endif

