#pragma once

#ifdef _GDEXTENSION
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/templates/safe_refcount.hpp>
#include <godot_cpp/classes/mutex.hpp>
using namespace godot;
#else
#include "servers/audio/audio_server.h"
#include "servers/audio/audio_stream.h"
#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include "core/templates/safe_refcount.h"
#endif

#include "soundfont2.h"

struct tsf;

class AudioStreamSoundfontPlayer;

class AudioStreamPlaybackSoundfont : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackSoundfont, AudioStreamPlayback);

private:
	friend class AudioStreamSoundfontPlayer;

#ifdef _GDEXTENSION
	AudioStreamSoundfontPlayer *sf_stream = nullptr;
#else
	Ref<AudioStreamSoundfontPlayer> sf_stream;
#endif

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

#ifdef _GDEXTENSION
	Ref<Mutex> pending_mutex;
#else
	BinaryMutex pending_mutex;
#endif
	LocalVector<PendingCommand> pending_commands;

	void _flush_pending_commands();

protected:
	static void _bind_methods();

public:
#ifdef _GDEXTENSION
	virtual void _start(double p_from_pos = 0.0) override;
	virtual void _stop() override;
	virtual bool _is_playing() const override;

	virtual int32_t _get_loop_count() const override;
	virtual double _get_playback_position() const override;
	virtual void _seek(double p_time) override;

	virtual int32_t _mix(AudioFrame *p_buffer, float p_rate_scale, int32_t p_frames) override;
#else
	virtual void start(double p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;

	virtual int get_loop_count() const override;
	virtual double get_playback_position() const override;
	virtual void seek(double p_time) override;

	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	virtual void tag_used_streams() override;
#endif

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

#ifdef _GDEXTENSION
	virtual Ref<AudioStreamPlayback> _instantiate_playback() const override;
	virtual String _get_stream_name() const override;
	virtual double _get_length() const override;
	virtual bool _is_monophonic() const override;
#else
	virtual Ref<AudioStreamPlayback> instantiate_playback() override;
	virtual String get_stream_name() const override;
	virtual double get_length() const override;
	virtual bool is_monophonic() const override;
#endif

	AudioStreamSoundfontPlayer();
	~AudioStreamSoundfontPlayer();
};
