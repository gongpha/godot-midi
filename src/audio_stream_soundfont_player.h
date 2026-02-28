#pragma once

#ifdef _GDEXTENSION
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
using namespace godot;
#else
#include "servers/audio/audio_server.h"
#include "servers/audio/audio_stream.h"
#endif

#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include "core/templates/safe_refcount.h"

#include "soundfont2.h"

struct tsf;

class AudioStreamSoundfontPlayer;

class AudioStreamPlaybackSoundfont : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackSoundfont, AudioStreamPlayback);

private:
	friend class AudioStreamSoundfontPlayer;

	Ref<AudioStreamSoundfontPlayer> sf_stream;

	tsf *tsf_instance = nullptr;
	uint32_t frames_mixed = 0;
	bool active = false;

	enum PendingCommandType {
		CMD_NOTE_ON,
		CMD_NOTE_OFF,
		CMD_NOTE_OFF_ALL,
		CMD_SET_PRESET,
		CMD_CONTROL_CHANGE,
		CMD_PITCH_BEND,
		CMD_CHANNEL_PRESSURE,
	};

	struct PendingCommand {
		PendingCommandType type;
		int channel;
		int param1;
		int param2;
		float fparam;
	};

	BinaryMutex pending_mutex;
	LocalVector<PendingCommand> pending_commands;

	void _flush_pending_commands();

protected:
	static void _bind_methods();

public:
	virtual void start(double p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;

	virtual int get_loop_count() const override;
	virtual double get_playback_position() const override;
	virtual void seek(double p_time) override;

	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	virtual void tag_used_streams() override;

	void note_on(int p_key, float p_velocity = 1.0f, int p_channel = 0);
	void note_off(int p_key, int p_channel = 0);
	void note_off_all();

	void set_preset(int p_channel, int p_preset_number, bool p_drums = false);
	void control_change(int p_channel, int p_controller, int p_value);
	void pitch_bend(int p_channel, int p_pitch_wheel);
	void channel_pressure(int p_channel, int p_pressure);

	AudioStreamPlaybackSoundfont();
	~AudioStreamPlaybackSoundfont();
};

class AudioStreamSoundfontPlayer : public AudioStream {
	GDCLASS(AudioStreamSoundfontPlayer, AudioStream);
#ifndef _GDEXTENSION
	OBJ_SAVE_TYPE(AudioStream);
#endif

	Ref<SoundFont2> soundfont;

	friend class AudioStreamPlaybackSoundfont;

protected:
	static void _bind_methods();

public:
	void set_soundfont(const Ref<SoundFont2> &p_soundfont);
	Ref<SoundFont2> get_soundfont() const;

	virtual Ref<AudioStreamPlayback> instantiate_playback() override;
	virtual String get_stream_name() const override;
	virtual double get_length() const override;
	virtual bool is_monophonic() const override;

	AudioStreamSoundfontPlayer();
	~AudioStreamSoundfontPlayer();
};
