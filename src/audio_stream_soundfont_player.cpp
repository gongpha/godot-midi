#include "audio_stream_soundfont_player.h"

#ifdef _GDEXTENSION
using namespace godot;
#else
#include "core/error/error_macros.h"
#endif

#include "../thirdparty/tinysoundfont/tsf.h"

#ifdef _GDEXTENSION
#define PENDING_MUTEX_LOCK pending_mutex->lock();
#define PENDING_MUTEX_UNLOCK pending_mutex->unlock();
#else
#define PENDING_MUTEX_LOCK pending_mutex.lock();
#define PENDING_MUTEX_UNLOCK pending_mutex.unlock();
#endif

void AudioStreamPlaybackSoundfont::_flush_pending_commands() {
	PENDING_MUTEX_LOCK
	LocalVector<PendingCommand> local_cmds(pending_commands);
	pending_commands.clear();
	PENDING_MUTEX_UNLOCK

	if (!tsf_instance) {
		return;
	}

	for (const PendingCommand &cmd : local_cmds) {
		switch (cmd.type) {
			case CMD_NOTE_ON : {
				int key = CLAMP(cmd.param1, 0, 127);
				float vel = CLAMP(cmd.fparam, 0.0f, 1.0f);
				tsf_channel_note_on(tsf_instance, cmd.channel, key, vel);
			} break;
			case CMD_NOTE_OFF : {
				int key = CLAMP(cmd.param1, 0, 127);
				tsf_channel_note_off(tsf_instance, cmd.channel, key);
			} break;
			case CMD_NOTE_OFF_ALL : {
				tsf_note_off_all(tsf_instance);
			} break;
			case CMD_SET_PRESET : {
				bool drums = (cmd.param2 != 0);
				tsf_channel_set_presetnumber(tsf_instance, cmd.channel, cmd.param1, drums);
			} break;
			case CMD_CONTROL_CHANGE : {
				tsf_channel_midi_control(tsf_instance, cmd.channel, cmd.param1, cmd.param2);
			} break;
			case CMD_PITCH_BEND : {
				tsf_channel_set_pitchwheel(tsf_instance, cmd.channel, cmd.param1);
			} break;
			case CMD_CHANNEL_PRESSURE : {
				tsf_channel_midi_control(tsf_instance, cmd.channel, 0x07 /* volume MSB */, cmd.param1);
			} break;
			default:
				break;
		}
	}
}

#ifdef _GDEXTENSION
void AudioStreamPlaybackSoundfont::_start(double p_from_pos) {
#else
void AudioStreamPlaybackSoundfont::start(double p_from_pos) {
#endif
	active = true;
	frames_mixed = 0;
}

#ifdef _GDEXTENSION
void AudioStreamPlaybackSoundfont::_stop() {
#else
void AudioStreamPlaybackSoundfont::stop() {
#endif
	active = false;
	if (tsf_instance) {
		tsf_note_off_all(tsf_instance);
	}
}

#ifdef _GDEXTENSION
bool AudioStreamPlaybackSoundfont::_is_playing() const {
#else
bool AudioStreamPlaybackSoundfont::is_playing() const {
#endif
	return active;
}

#ifdef _GDEXTENSION
int32_t AudioStreamPlaybackSoundfont::_get_loop_count() const {
#else
int AudioStreamPlaybackSoundfont::get_loop_count() const {
#endif
	return 0;
}

#ifdef _GDEXTENSION
double AudioStreamPlaybackSoundfont::_get_playback_position() const {
#else
double AudioStreamPlaybackSoundfont::get_playback_position() const {
#endif
	float sample_rate = AudioServer::get_singleton()->get_mix_rate();
	return (double)frames_mixed / (double)sample_rate;
}

#ifdef _GDEXTENSION
void AudioStreamPlaybackSoundfont::_seek(double p_time) {
#else
void AudioStreamPlaybackSoundfont::seek(double p_time) {
#endif
	float sample_rate = AudioServer::get_singleton()->get_mix_rate();
	frames_mixed = (uint32_t)(sample_rate * p_time);
}

#ifdef _GDEXTENSION
int32_t AudioStreamPlaybackSoundfont::_mix(AudioFrame *p_buffer, float p_rate_scale, int32_t p_frames) {
#else
int AudioStreamPlaybackSoundfont::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
#endif
	if (!active || !tsf_instance) {
		for (int i = 0; i < p_frames; i++) {
			p_buffer[i].left = 0.0f;
			p_buffer[i].right = 0.0f;
		}
		return p_frames;
	}

	_flush_pending_commands();

	tsf_render_float(tsf_instance, (float *)p_buffer, p_frames, 0);
	frames_mixed += p_frames;

	return p_frames;
}

#ifdef _GDEXTENSION
#else
void AudioStreamPlaybackSoundfont::tag_used_streams() {
	sf_stream->tag_used(get_playback_position());
}
#endif

void AudioStreamPlaybackSoundfont::note_on(int p_key, float p_velocity, int p_channel) {
	ERR_FAIL_COND(p_key < 0 || p_key > 127);
	PENDING_MUTEX_LOCK
	pending_commands.push_back({ CMD_NOTE_ON, p_channel, p_key, 0, p_velocity });
	PENDING_MUTEX_UNLOCK
}

void AudioStreamPlaybackSoundfont::note_off(int p_key, int p_channel) {
	ERR_FAIL_COND(p_key < 0 || p_key > 127);
	PENDING_MUTEX_LOCK
	pending_commands.push_back({ CMD_NOTE_OFF, p_channel, p_key, 0, 0.0f });
	PENDING_MUTEX_UNLOCK
}

void AudioStreamPlaybackSoundfont::note_off_all() {
	PENDING_MUTEX_LOCK
	pending_commands.push_back({ CMD_NOTE_OFF_ALL, 0, 0, 0, 0.0f });
	PENDING_MUTEX_UNLOCK
}

void AudioStreamPlaybackSoundfont::set_preset(int p_channel, int p_preset_number, bool p_drums) {
	PENDING_MUTEX_LOCK
	pending_commands.push_back({ CMD_SET_PRESET, p_channel, p_preset_number, p_drums ? 1 : 0, 0.0f });
	PENDING_MUTEX_UNLOCK
}

void AudioStreamPlaybackSoundfont::control_change(int p_channel, int p_controller, int p_value) {
	PENDING_MUTEX_LOCK
	pending_commands.push_back({ CMD_CONTROL_CHANGE, p_channel, p_controller, p_value, 0.0f });
	PENDING_MUTEX_UNLOCK
}

void AudioStreamPlaybackSoundfont::pitch_bend(int p_channel, int p_pitch_wheel) {
	PENDING_MUTEX_LOCK
	pending_commands.push_back({ CMD_PITCH_BEND, p_channel, p_pitch_wheel, 0, 0.0f });
	PENDING_MUTEX_UNLOCK
}

void AudioStreamPlaybackSoundfont::channel_pressure(int p_channel, int p_pressure) {
	PENDING_MUTEX_LOCK
	pending_commands.push_back({ CMD_CHANNEL_PRESSURE, p_channel, p_pressure, 0, 0.0f });
	PENDING_MUTEX_UNLOCK
}

AudioStreamPlaybackSoundfont::AudioStreamPlaybackSoundfont() {
#ifdef _GDEXTENSION
	pending_mutex.instantiate();
#endif
}

AudioStreamPlaybackSoundfont::~AudioStreamPlaybackSoundfont() {
	if (tsf_instance) {
		tsf_close(tsf_instance);
		tsf_instance = nullptr;
	}
}

void AudioStreamPlaybackSoundfont::_bind_methods() {
	ClassDB::bind_method(D_METHOD("note_on", "key", "velocity", "channel"), &AudioStreamPlaybackSoundfont::note_on, DEFVAL(1.0f), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("note_off", "key", "channel"), &AudioStreamPlaybackSoundfont::note_off, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("note_off_all"), &AudioStreamPlaybackSoundfont::note_off_all);

	ClassDB::bind_method(D_METHOD("set_preset", "channel", "preset_number", "drums"), &AudioStreamPlaybackSoundfont::set_preset, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("control_change", "channel", "controller", "value"), &AudioStreamPlaybackSoundfont::control_change);
	ClassDB::bind_method(D_METHOD("pitch_bend", "channel", "pitch_wheel"), &AudioStreamPlaybackSoundfont::pitch_bend);
	ClassDB::bind_method(D_METHOD("channel_pressure", "channel", "pressure"), &AudioStreamPlaybackSoundfont::channel_pressure);
}

AudioStreamSoundfontPlayer::AudioStreamSoundfontPlayer() {
}

AudioStreamSoundfontPlayer::~AudioStreamSoundfontPlayer() {
}

void AudioStreamSoundfontPlayer::set_soundfont(const Ref<SoundFont2> &p_soundfont) {
	soundfont = p_soundfont;
}

Ref<SoundFont2> AudioStreamSoundfontPlayer::get_soundfont() const {
	return soundfont;
}

#ifdef _GDEXTENSION
Ref<AudioStreamPlayback> AudioStreamSoundfontPlayer::_instantiate_playback() const {
	ERR_FAIL_COND_V_MSG(soundfont.is_null(), nullptr, "AudioStreamSoundfontPlayer : No SoundFont2 assigned.");
	ERR_FAIL_COND_V_MSG(!soundfont->get_soundfont(), nullptr, "AudioStreamSoundfontPlayer : SoundFont2 has no loaded data.");

	Ref<AudioStreamPlaybackSoundfont> playback;
	playback.instantiate();
	playback->sf_stream = const_cast<AudioStreamSoundfontPlayer *>(this);

	playback->tsf_instance = tsf_copy(soundfont->get_soundfont());
	ERR_FAIL_COND_V_MSG(!playback->tsf_instance, nullptr, "AudioStreamSoundfontPlayer : Failed to copy SoundFont instance.");

	float sample_rate = AudioServer::get_singleton()->get_mix_rate();
	tsf_set_output(playback->tsf_instance, TSF_STEREO_INTERLEAVED, (int)sample_rate, 0.0f);
	tsf_set_max_voices(playback->tsf_instance, 256);

	return playback;
}
#else
Ref<AudioStreamPlayback> AudioStreamSoundfontPlayer::instantiate_playback() {
	ERR_FAIL_COND_V_MSG(soundfont.is_null(), nullptr, "AudioStreamSoundfontPlayer : No SoundFont2 assigned.");
	ERR_FAIL_COND_V_MSG(!soundfont->get_soundfont(), nullptr, "AudioStreamSoundfontPlayer : SoundFont2 has no loaded data.");

	Ref<AudioStreamPlaybackSoundfont> playback;
	playback.instantiate();
	playback->sf_stream = Ref<AudioStreamSoundfontPlayer>(this);

	playback->tsf_instance = tsf_copy(soundfont->get_soundfont());
	ERR_FAIL_COND_V_MSG(!playback->tsf_instance, nullptr, "AudioStreamSoundfontPlayer : Failed to copy SoundFont instance.");

	float sample_rate = AudioServer::get_singleton()->get_mix_rate();
	tsf_set_output(playback->tsf_instance, TSF_STEREO_INTERLEAVED, (int)sample_rate, 0.0f);
	tsf_set_max_voices(playback->tsf_instance, 256);

	return playback;
}
#endif

#ifdef _GDEXTENSION
String AudioStreamSoundfontPlayer::_get_stream_name() const {
#else
String AudioStreamSoundfontPlayer::get_stream_name() const {
#endif
	return "";
}

#ifdef _GDEXTENSION
double AudioStreamSoundfontPlayer::_get_length() const {
#else
double AudioStreamSoundfontPlayer::get_length() const {
#endif
	return 0.0;
}

#ifdef _GDEXTENSION
bool AudioStreamSoundfontPlayer::_is_monophonic() const {
#else
bool AudioStreamSoundfontPlayer::is_monophonic() const {
#endif
	return false;
}

void AudioStreamSoundfontPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_soundfont", "soundfont"), &AudioStreamSoundfontPlayer::set_soundfont);
	ClassDB::bind_method(D_METHOD("get_soundfont"), &AudioStreamSoundfontPlayer::get_soundfont);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "soundfont", PROPERTY_HINT_RESOURCE_TYPE, "SoundFont2"), "set_soundfont", "get_soundfont");
}
