#include "audio_stream_midi.h"

#include "core/error/error_macros.h"

#include "../thirdparty/tinysoundfont/tsf.h"
#include "../thirdparty/tinysoundfont/tml.h"

bool AudioStreamPlaybackMIDISF2::_has_any_solo() const {
	for (int i = 0; i < MIDI_CHANNEL_COUNT; i++) {
		if (channel_states[i].solo.is_set()) {
			return true;
		}
	}
	return false;
}

bool AudioStreamPlaybackMIDISF2::_is_channel_audible(int p_channel) const {
	if (p_channel < 0 || p_channel >= MIDI_CHANNEL_COUNT) {
		return false;
	}
	const ChannelState &cs = channel_states[p_channel];
	if (cs.muted.is_set()) {
		return false;
	}
	if (_has_any_solo() && !cs.solo.is_set()) {
		return false;
	}
	return true;
}

void AudioStreamPlaybackMIDISF2::_apply_midi_message(tml_message *p_msg) {
	if (!tsf_instance || !p_msg) {
		return;
	}

	int channel = p_msg->channel;
	int transpose = midi_stream->transpose;
	int ch_transpose = (channel >= 0 && channel < MIDI_CHANNEL_COUNT) ? channel_states[channel].transpose.get() : 0;

	int param1 = 0;
	int param2 = 0;

	switch (p_msg->type) {
		case TML_PROGRAM_CHANGE : {
			param1 = p_msg->program;
			int override_prog = (channel >= 0 && channel < MIDI_CHANNEL_COUNT) ? channel_states[channel].program_override.get() : -1;
			int actual_program = (override_prog >= 0) ? override_prog : param1;
			tsf_channel_set_presetnumber(tsf_instance, channel, actual_program, (channel == 9));
		} break;
		case TML_NOTE_ON : {
			int key = p_msg->key + transpose * 12 + ch_transpose;
			key = CLAMP(key, 0, 127);
			param1 = p_msg->key;
			param2 = p_msg->velocity;
			if (_is_channel_audible(channel)) {
				float vel = p_msg->velocity / 127.0f;
				if (channel >= 0 && channel < MIDI_CHANNEL_COUNT) {
					vel *= channel_states[channel].volume.get();
				}
				tsf_channel_note_on(tsf_instance, channel, key, vel);
			}
		} break;
		case TML_NOTE_OFF : {
			int key = p_msg->key + transpose * 12 + ch_transpose;
			key = CLAMP(key, 0, 127);
			param1 = p_msg->key; // emit original key for consistent visualization
			tsf_channel_note_off(tsf_instance, channel, key);
		} break;
		case TML_PITCH_BEND : {
			param1 = p_msg->pitch_bend;
			tsf_channel_set_pitchwheel(tsf_instance, channel, param1);
		} break;
		case TML_CONTROL_CHANGE : {
			param1 = p_msg->control;
			param2 = p_msg->control_value;
			tsf_channel_midi_control(tsf_instance, channel, param1, param2);
		} break;
		case TML_SET_TEMPO : {
			int usec_per_beat = tml_get_tempo_value(p_msg);
			if (usec_per_beat > 0) {
				param1 = (int)(60000000.0 / usec_per_beat); // BPM
			}
		} break;
		default:
			break;
	}

	// during seek, only suppress high-frequency events (notes, CC, pitch bend)
	// allow SET_TEMPO and PROGRAM_CHANGE through so GDScript can update BPM and instruments
	bool should_emit = true;
	if (suppress_signals) {
		switch (p_msg->type) {
			case TML_SET_TEMPO:
			case TML_PROGRAM_CHANGE:
				should_emit = true;
				break;
			default:
				should_emit = false;
				break;
		}
	}
	if (should_emit) {
		call_deferred(SNAME("emit_signal"), SNAME("applied_midi_message"), (int)p_msg->type, channel, param1, param2);
	}
}

void AudioStreamPlaybackMIDISF2::_process_midi_events(double p_up_to_msec) {
	while (current_msg && current_msg->time <= (unsigned int)p_up_to_msec) {
		_apply_midi_message(current_msg);
		current_msg = current_msg->next;
	}
}

void AudioStreamPlaybackMIDISF2::start(double p_from_pos) {
	active = true;
	loops = 0;
	seek(p_from_pos);
}

void AudioStreamPlaybackMIDISF2::stop() {
	active = false;
	if (tsf_instance) {
		tsf_note_off_all(tsf_instance);
	}
}

bool AudioStreamPlaybackMIDISF2::is_playing() const {
	return active;
}

int AudioStreamPlaybackMIDISF2::get_loop_count() const {
	return loops;
}

double AudioStreamPlaybackMIDISF2::get_playback_position() const {
	return playback_msec / 1000.0;
}

void AudioStreamPlaybackMIDISF2::seek(double p_time) {
	if (!tsf_instance || !first_msg) {
		return;
	}

	tsf_reset(tsf_instance);

	double target_msec = p_time * 1000.0;

	suppress_signals = true;

	tml_message *msg = first_msg;
	while (msg && msg->time <= (unsigned int)target_msec) {
		switch (msg->type) {
			case TML_PROGRAM_CHANGE:
			case TML_CONTROL_CHANGE:
			case TML_PITCH_BEND:
			case TML_SET_TEMPO:
				_apply_midi_message(msg);
				break;
			default:
				break;
		}
		msg = msg->next;
	}

	suppress_signals = false;

	current_msg = msg;
	playback_msec = target_msec;

	float sample_rate = AudioServer::get_singleton()->get_mix_rate();
	frames_mixed = (uint32_t)(sample_rate * p_time);
}

int AudioStreamPlaybackMIDISF2::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!active || !tsf_instance) {
		for (int i = 0; i < p_frames; i++) {
			p_buffer[i] = AudioFrame(0, 0);
		}
		return p_frames;
	}

	float sample_rate = AudioServer::get_singleton()->get_mix_rate();

	float tempo_scale = midi_stream->tempo_scale;
	bool use_loop = midi_stream->loop;

	int frames_remaining = p_frames;
	int offset = 0;

	const int BLOCK_SIZE = 64;

	_flush_pending_messages();

	while (frames_remaining > 0 && active) {
		int block = MIN(frames_remaining, BLOCK_SIZE);

		double block_msec = (double)block / sample_rate * 1000.0 * tempo_scale;
		playback_msec += block_msec;

		_process_midi_events(playback_msec);

		tsf_render_float(tsf_instance, (float *)&p_buffer[offset], block, 0);

		frames_mixed += block;
		offset += block;
		frames_remaining -= block;

		if (!current_msg && tsf_active_voice_count(tsf_instance) == 0) {
			if (use_loop) {
				loops++;
				seek(midi_stream->loop_offset);
			} else {
				for (int i = offset; i < p_frames; i++) {
					p_buffer[i] = AudioFrame(0, 0);
				}
				active = false;
				break;
			}
		}
	}

	return p_frames;
}

void AudioStreamPlaybackMIDISF2::tag_used_streams() {
	midi_stream->tag_used(get_playback_position());
}

AudioStreamPlaybackMIDISF2::~AudioStreamPlaybackMIDISF2() {
	if (tsf_instance) {
		tsf_close(tsf_instance);
		tsf_instance = nullptr;
	}
}

void AudioStreamPlaybackMIDISF2::push_midi_message(MIDIMessageType p_type, int p_channel, int p_param1, int p_param2) {
	pending_mutex.lock();
	pending_messages.push_back({ p_type, p_channel, p_param1, p_param2 });
	pending_mutex.unlock();
}

void AudioStreamPlaybackMIDISF2::set_channel_muted(int p_channel, bool p_muted) {
	ERR_FAIL_INDEX(p_channel, MIDI_CHANNEL_COUNT);
	if (p_muted) {
		channel_states[p_channel].muted.set();
	} else {
		channel_states[p_channel].muted.clear();
	}
	// silence channels that became non-audible
	for (int i = 0; i < MIDI_CHANNEL_COUNT; i++) {
		if (!_is_channel_audible(i)) {
			push_midi_message(MESSAGE_CONTROL_CHANGE, i, CONTROLLER_ALL_NOTES_OFF, 0);
		}
	}
}

bool AudioStreamPlaybackMIDISF2::is_channel_muted(int p_channel) const {
	ERR_FAIL_INDEX_V(p_channel, MIDI_CHANNEL_COUNT, false);
	return channel_states[p_channel].muted.is_set();
}

void AudioStreamPlaybackMIDISF2::set_channel_solo(int p_channel, bool p_solo) {
	ERR_FAIL_INDEX(p_channel, MIDI_CHANNEL_COUNT);
	if (p_solo) {
		channel_states[p_channel].solo.set();
	} else {
		channel_states[p_channel].solo.clear();
	}
	// silence channels that became non-audible
	for (int i = 0; i < MIDI_CHANNEL_COUNT; i++) {
		if (!_is_channel_audible(i)) {
			push_midi_message(MESSAGE_CONTROL_CHANGE, i, CONTROLLER_ALL_NOTES_OFF, 0);
		}
	}
}

bool AudioStreamPlaybackMIDISF2::is_channel_solo(int p_channel) const {
	ERR_FAIL_INDEX_V(p_channel, MIDI_CHANNEL_COUNT, false);
	return channel_states[p_channel].solo.is_set();
}

void AudioStreamPlaybackMIDISF2::set_channel_transpose(int p_channel, int p_semitones) {
	ERR_FAIL_INDEX(p_channel, MIDI_CHANNEL_COUNT);
	channel_states[p_channel].transpose.set(p_semitones);
}

int AudioStreamPlaybackMIDISF2::get_channel_transpose(int p_channel) const {
	ERR_FAIL_INDEX_V(p_channel, MIDI_CHANNEL_COUNT, 0);
	return channel_states[p_channel].transpose.get();
}

void AudioStreamPlaybackMIDISF2::set_channel_volume(int p_channel, float p_volume) {
	ERR_FAIL_INDEX(p_channel, MIDI_CHANNEL_COUNT);
	channel_states[p_channel].volume.set(CLAMP(p_volume, 0.0f, 1.0f));
}

float AudioStreamPlaybackMIDISF2::get_channel_volume(int p_channel) const {
	ERR_FAIL_INDEX_V(p_channel, MIDI_CHANNEL_COUNT, 1.0f);
	return channel_states[p_channel].volume.get();
}

void AudioStreamPlaybackMIDISF2::set_channel_program_override(int p_channel, int p_program) {
	ERR_FAIL_INDEX(p_channel, MIDI_CHANNEL_COUNT);
	channel_states[p_channel].program_override.set(p_program);
}

int AudioStreamPlaybackMIDISF2::get_channel_program_override(int p_channel) const {
	ERR_FAIL_INDEX_V(p_channel, MIDI_CHANNEL_COUNT, -1);
	return channel_states[p_channel].program_override.get();
}

int AudioStreamPlaybackMIDISF2::get_channel_preset_index(int p_channel) const {
	ERR_FAIL_INDEX_V(p_channel, MIDI_CHANNEL_COUNT, -1);
	ERR_FAIL_COND_V(!tsf_instance, -1);
	return tsf_channel_get_preset_index(tsf_instance, p_channel);
}

int AudioStreamPlaybackMIDISF2::get_channel_preset_number(int p_channel) const {
	ERR_FAIL_INDEX_V(p_channel, MIDI_CHANNEL_COUNT, -1);
	ERR_FAIL_COND_V(!tsf_instance, -1);
	return tsf_channel_get_preset_number(tsf_instance, p_channel);
}

String AudioStreamPlaybackMIDISF2::get_channel_preset_name(int p_channel) const {
	ERR_FAIL_INDEX_V(p_channel, MIDI_CHANNEL_COUNT, String());
	ERR_FAIL_COND_V(!tsf_instance, String());
	int idx = tsf_channel_get_preset_index(tsf_instance, p_channel);
	if (idx < 0) {
		return String();
	}
	const char *name = tsf_get_presetname(tsf_instance, idx);
	return name ? String::utf8(name) : String();
}

TypedArray<Dictionary> AudioStreamPlaybackMIDISF2::get_midi_channel_list() const {
	TypedArray<Dictionary> result;

	if (!midi_stream.is_valid() || midi_stream->midi.is_null() || !midi_stream->midi->get_midi()) {
		return result;
	}

	struct ChannelInfo {
		bool used = false;
		int program = 0;
		int note_count = 0;
	};
	ChannelInfo infos[MIDI_CHANNEL_COUNT] = {};

	tml_message *msg = midi_stream->midi->get_midi();
	while (msg) {
		int ch = msg->channel;
		if (ch >= 0 && ch < MIDI_CHANNEL_COUNT) {
			if (msg->type == TML_PROGRAM_CHANGE) {
				infos[ch].program = msg->program;
				infos[ch].used = true;
			} else if (msg->type == TML_NOTE_ON) {
				infos[ch].note_count++;
				infos[ch].used = true;
			}
		}
		msg = msg->next;
	}

	for (int i = 0; i < MIDI_CHANNEL_COUNT; i++) {
		if (!infos[i].used) {
			continue;
		}
		Dictionary d;
		d["channel"] = i;
		d["program"] = infos[i].program;
		d["note_count"] = infos[i].note_count;
		d["is_drum"] = (i == 9);

		if (tsf_instance) {
			int bank = (i == 9) ? 128 : 0;
			const char *name = tsf_bank_get_presetname(tsf_instance, bank, infos[i].program);
			d["preset_name"] = name ? String::utf8(name) : String();
		} else {
			d["preset_name"] = String();
		}
		result.push_back(d);
	}

	return result;
}

AudioStreamPlaybackMIDISF2::AudioStreamPlaybackMIDISF2() {
	for (int i = 0; i < MIDI_CHANNEL_COUNT; i++) {
		channel_states[i].volume.set(1.0f);
		channel_states[i].program_override.set(-1);
	}
}

void AudioStreamPlaybackMIDISF2::_apply_pending_message(const PendingMIDIMessage &p_msg) {
	if (!tsf_instance) {
		return;
	}

	int channel = p_msg.channel;
	int transpose = midi_stream.is_valid() ? midi_stream->transpose : 0;
	int ch_transpose = (channel >= 0 && channel < MIDI_CHANNEL_COUNT) ? channel_states[channel].transpose.get() : 0;

	switch (p_msg.type) {
		case MESSAGE_PROGRAM_CHANGE : {
			int override_prog = (channel >= 0 && channel < MIDI_CHANNEL_COUNT) ? channel_states[channel].program_override.get() : -1;
			int actual_program = (override_prog >= 0) ? override_prog : p_msg.param1;
			tsf_channel_set_presetnumber(tsf_instance, channel, actual_program, (channel == 9));
		} break;
		case MESSAGE_NOTE_ON : {
			if (!_is_channel_audible(channel)) {
				break;
			}
			int key = p_msg.param1 + transpose * 12 + ch_transpose;
			key = CLAMP(key, 0, 127);
			float vel = p_msg.param2 / 127.0f;
			if (channel >= 0 && channel < MIDI_CHANNEL_COUNT) {
				vel *= channel_states[channel].volume.get();
			}
			tsf_channel_note_on(tsf_instance, channel, key, vel);
		} break;
		case MESSAGE_NOTE_OFF : {
			int key = p_msg.param1 + transpose * 12 + ch_transpose;
			key = CLAMP(key, 0, 127);
			tsf_channel_note_off(tsf_instance, channel, key);
		} break;
		case MESSAGE_PITCH_BEND : {
			tsf_channel_set_pitchwheel(tsf_instance, channel, p_msg.param1);
		} break;
		case MESSAGE_CONTROL_CHANGE : {
			tsf_channel_midi_control(tsf_instance, channel, p_msg.param1, p_msg.param2);
		} break;
		default:
			break;
	}
}

void AudioStreamPlaybackMIDISF2::_flush_pending_messages() {
	pending_mutex.lock();
	LocalVector<PendingMIDIMessage> local_msgs(pending_messages);
	pending_messages.clear();
	pending_mutex.unlock();

	for (const PendingMIDIMessage &msg : local_msgs) {
		_apply_pending_message(msg);
	}
}

void AudioStreamPlaybackMIDISF2::_bind_methods() {
	ClassDB::bind_method(D_METHOD("push_midi_message", "type", "channel", "param1", "param2"), &AudioStreamPlaybackMIDISF2::push_midi_message, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("set_channel_muted", "channel", "muted"), &AudioStreamPlaybackMIDISF2::set_channel_muted);
	ClassDB::bind_method(D_METHOD("is_channel_muted", "channel"), &AudioStreamPlaybackMIDISF2::is_channel_muted);

	ClassDB::bind_method(D_METHOD("set_channel_solo", "channel", "solo"), &AudioStreamPlaybackMIDISF2::set_channel_solo);
	ClassDB::bind_method(D_METHOD("is_channel_solo", "channel"), &AudioStreamPlaybackMIDISF2::is_channel_solo);

	ClassDB::bind_method(D_METHOD("set_channel_transpose", "channel", "semitones"), &AudioStreamPlaybackMIDISF2::set_channel_transpose);
	ClassDB::bind_method(D_METHOD("get_channel_transpose", "channel"), &AudioStreamPlaybackMIDISF2::get_channel_transpose);

	ClassDB::bind_method(D_METHOD("set_channel_volume", "channel", "volume"), &AudioStreamPlaybackMIDISF2::set_channel_volume);
	ClassDB::bind_method(D_METHOD("get_channel_volume", "channel"), &AudioStreamPlaybackMIDISF2::get_channel_volume);

	ClassDB::bind_method(D_METHOD("set_channel_program_override", "channel", "program"), &AudioStreamPlaybackMIDISF2::set_channel_program_override);
	ClassDB::bind_method(D_METHOD("get_channel_program_override", "channel"), &AudioStreamPlaybackMIDISF2::get_channel_program_override);

	ClassDB::bind_method(D_METHOD("get_channel_preset_index", "channel"), &AudioStreamPlaybackMIDISF2::get_channel_preset_index);
	ClassDB::bind_method(D_METHOD("get_channel_preset_number", "channel"), &AudioStreamPlaybackMIDISF2::get_channel_preset_number);
	ClassDB::bind_method(D_METHOD("get_channel_preset_name", "channel"), &AudioStreamPlaybackMIDISF2::get_channel_preset_name);
	ClassDB::bind_method(D_METHOD("get_midi_channel_list"), &AudioStreamPlaybackMIDISF2::get_midi_channel_list);

	ADD_SIGNAL(MethodInfo("applied_midi_message",
		PropertyInfo(Variant::INT, "type"),
		PropertyInfo(Variant::INT, "channel"),
		PropertyInfo(Variant::INT, "param1"),
		PropertyInfo(Variant::INT, "param2"))
	);

	BIND_ENUM_CONSTANT(MESSAGE_NOTE_OFF);
	BIND_ENUM_CONSTANT(MESSAGE_NOTE_ON);
	BIND_ENUM_CONSTANT(MESSAGE_KEY_PRESSURE);
	BIND_ENUM_CONSTANT(MESSAGE_CONTROL_CHANGE);
	BIND_ENUM_CONSTANT(MESSAGE_PROGRAM_CHANGE);
	BIND_ENUM_CONSTANT(MESSAGE_CHANNEL_PRESSURE);
	BIND_ENUM_CONSTANT(MESSAGE_PITCH_BEND);
	BIND_ENUM_CONSTANT(MESSAGE_SET_TEMPO);

	BIND_ENUM_CONSTANT(CONTROLLER_BANK_SELECT_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_MODULATION_WHEEL_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_BREATH_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_FOOT_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_PORTAMENTO_TIME_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_DATA_ENTRY_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_VOLUME_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_BALANCE_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_PAN_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_EXPRESSION_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_EFFECTS1_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_EFFECTS2_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC1_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC2_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC3_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC4_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_BANK_SELECT_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_MODULATION_WHEEL_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_BREATH_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_FOOT_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_PORTAMENTO_TIME_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_DATA_ENTRY_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_VOLUME_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_BALANCE_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_PAN_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_EXPRESSION_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_EFFECTS1_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_EFFECTS2_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC1_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC2_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC3_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC4_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_SUSTAIN);
	BIND_ENUM_CONSTANT(CONTROLLER_PORTAMENTO);
	BIND_ENUM_CONSTANT(CONTROLLER_SOSTENUTO);
	BIND_ENUM_CONSTANT(CONTROLLER_SOFT_PEDAL);
	BIND_ENUM_CONSTANT(CONTROLLER_LEGATO);
	BIND_ENUM_CONSTANT(CONTROLLER_HOLD2);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL1);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL2);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL3);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL4);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL5);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL6);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL7);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL8);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL9);
	BIND_ENUM_CONSTANT(CONTROLLER_SOUND_CTRL10);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC5);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC6);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC7);
	BIND_ENUM_CONSTANT(CONTROLLER_GPC8);
	BIND_ENUM_CONSTANT(CONTROLLER_PORTAMENTO_CTRL);
	BIND_ENUM_CONSTANT(CONTROLLER_FX_REVERB);
	BIND_ENUM_CONSTANT(CONTROLLER_FX_TREMOLO);
	BIND_ENUM_CONSTANT(CONTROLLER_FX_CHORUS);
	BIND_ENUM_CONSTANT(CONTROLLER_FX_CELESTE_DETUNE);
	BIND_ENUM_CONSTANT(CONTROLLER_FX_PHASER);
	BIND_ENUM_CONSTANT(CONTROLLER_DATA_ENTRY_INCR);
	BIND_ENUM_CONSTANT(CONTROLLER_DATA_ENTRY_DECR);
	BIND_ENUM_CONSTANT(CONTROLLER_NRPN_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_NRPN_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_RPN_LSB);
	BIND_ENUM_CONSTANT(CONTROLLER_RPN_MSB);
	BIND_ENUM_CONSTANT(CONTROLLER_ALL_SOUND_OFF);
	BIND_ENUM_CONSTANT(CONTROLLER_ALL_CTRL_OFF);
	BIND_ENUM_CONSTANT(CONTROLLER_LOCAL_CONTROL);
	BIND_ENUM_CONSTANT(CONTROLLER_ALL_NOTES_OFF);
	BIND_ENUM_CONSTANT(CONTROLLER_OMNI_OFF);
	BIND_ENUM_CONSTANT(CONTROLLER_OMNI_ON);
	BIND_ENUM_CONSTANT(CONTROLLER_POLY_OFF);
	BIND_ENUM_CONSTANT(CONTROLLER_POLY_ON);
}

AudioStreamMIDI::AudioStreamMIDI() {
	tempo_scale = 1.0f;
	transpose = 0;
}

AudioStreamMIDI::~AudioStreamMIDI() {
}

void AudioStreamMIDI::set_soundfont(const Ref<SoundFont2> &p_soundfont) {
	soundfont = p_soundfont;
}

Ref<SoundFont2> AudioStreamMIDI::get_soundfont() const {
	return soundfont;
}

void AudioStreamMIDI::set_midi(const Ref<MIDI> &p_midi) {
	midi = p_midi;
}

Ref<MIDI> AudioStreamMIDI::get_midi() const {
	return midi;
}

void AudioStreamMIDI::set_tempo_scale(float p_scale) {
	ERR_FAIL_COND(p_scale <= 0.0f);
	tempo_scale = p_scale;
}

float AudioStreamMIDI::get_tempo_scale() const {
	return tempo_scale;
}

void AudioStreamMIDI::set_transpose(int p_shift) {
	transpose = p_shift;
}

int AudioStreamMIDI::get_transpose() const {
	return transpose;
}

void AudioStreamMIDI::set_loop(bool p_enable) {
	loop = p_enable;
}

bool AudioStreamMIDI::has_loop() const {
	return loop;
}

void AudioStreamMIDI::set_loop_offset(double p_seconds) {
	loop_offset = p_seconds;
}

double AudioStreamMIDI::get_loop_offset() const {
	return loop_offset;
}

Ref<AudioStreamPlayback> AudioStreamMIDI::instantiate_playback() {
	ERR_FAIL_COND_V_MSG(soundfont.is_null(), nullptr, "AudioStreamMIDI : No SoundFont2 assigned.");
	ERR_FAIL_COND_V_MSG(midi.is_null(), nullptr, "AudioStreamMIDI : No MIDI assigned.");
	ERR_FAIL_COND_V_MSG(!soundfont->get_soundfont(), nullptr, "AudioStreamMIDI : SoundFont2 has no loaded data.");
	ERR_FAIL_COND_V_MSG(!midi->get_midi(), nullptr, "AudioStreamMIDI : MIDI has no loaded data.");

	Ref<AudioStreamPlaybackMIDISF2> playback;
	playback.instantiate();
	playback->midi_stream = Ref<AudioStreamMIDI>(this);

	playback->tsf_instance = tsf_copy(soundfont->get_soundfont());
	ERR_FAIL_COND_V_MSG(!playback->tsf_instance, nullptr, "AudioStreamMIDI : Failed to copy SoundFont instance.");

	float sample_rate = AudioServer::get_singleton()->get_mix_rate();

	tsf_set_output(playback->tsf_instance, TSF_STEREO_INTERLEAVED, (int)sample_rate, 0.0f);
	tsf_set_max_voices(playback->tsf_instance, 256);

	playback->first_msg = midi->get_midi();
	playback->current_msg = midi->get_midi();
	playback->playback_msec = 0.0;
	playback->frames_mixed = 0;
	playback->active = false;
	playback->loops = 0;

	return playback;
}

String AudioStreamMIDI::get_stream_name() const {
	return "";
}

double AudioStreamMIDI::get_length() const {
	if (midi.is_null() || !midi->get_midi()) {
		return 0.0;
	}
	unsigned int time_length = 0;
	tml_get_info(midi->get_midi(), nullptr, nullptr, nullptr, nullptr, &time_length);
	return (double)time_length / 1000.0;
}

bool AudioStreamMIDI::is_monophonic() const {
	return false;
}

void AudioStreamMIDI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_soundfont", "soundfont"), &AudioStreamMIDI::set_soundfont);
	ClassDB::bind_method(D_METHOD("get_soundfont"), &AudioStreamMIDI::get_soundfont);

	ClassDB::bind_method(D_METHOD("set_midi", "midi"), &AudioStreamMIDI::set_midi);
	ClassDB::bind_method(D_METHOD("get_midi"), &AudioStreamMIDI::get_midi);

	ClassDB::bind_method(D_METHOD("set_tempo_scale", "scale"), &AudioStreamMIDI::set_tempo_scale);
	ClassDB::bind_method(D_METHOD("get_tempo_scale"), &AudioStreamMIDI::get_tempo_scale);

	ClassDB::bind_method(D_METHOD("set_transpose", "shift"), &AudioStreamMIDI::set_transpose);
	ClassDB::bind_method(D_METHOD("get_transpose"), &AudioStreamMIDI::get_transpose);

	ClassDB::bind_method(D_METHOD("set_loop", "enable"), &AudioStreamMIDI::set_loop);
	ClassDB::bind_method(D_METHOD("has_loop"), &AudioStreamMIDI::has_loop);

	ClassDB::bind_method(D_METHOD("set_loop_offset", "seconds"), &AudioStreamMIDI::set_loop_offset);
	ClassDB::bind_method(D_METHOD("get_loop_offset"), &AudioStreamMIDI::get_loop_offset);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "soundfont", PROPERTY_HINT_RESOURCE_TYPE, "SoundFont2"), "set_soundfont", "get_soundfont");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "midi", PROPERTY_HINT_RESOURCE_TYPE, "MIDI"), "set_midi", "get_midi");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tempo_scale", PROPERTY_HINT_RANGE, "0.01,10.0,0.01"), "set_tempo_scale", "get_tempo_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "transpose", PROPERTY_HINT_RANGE, "-10,10,1"), "set_transpose", "get_transpose");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "has_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loop_offset"), "set_loop_offset", "get_loop_offset");
}