#pragma once

#ifdef _GDEXTENSION
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback.hpp>
#include <godot_cpp/classes/mutex.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/templates/safe_refcount.hpp>
using namespace godot;
#else
#include "servers/audio/audio_server.h"
#include "servers/audio/audio_stream.h"
#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include "core/templates/safe_refcount.h"
#endif



#include "soundfont2.h"
#include "midi.h"

struct tsf;
struct tml_message;

class AudioStreamMIDI;

class AudioStreamPlaybackMIDISF2 : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackMIDISF2, AudioStreamPlayback);

public:
	enum MIDIMessageType {
		MESSAGE_NOTE_OFF = 0x80,
		MESSAGE_NOTE_ON = 0x90,
		MESSAGE_KEY_PRESSURE = 0xA0,
		MESSAGE_CONTROL_CHANGE = 0xB0,
		MESSAGE_PROGRAM_CHANGE = 0xC0,
		MESSAGE_CHANNEL_PRESSURE = 0xD0,
		MESSAGE_PITCH_BEND = 0xE0,
		MESSAGE_SET_TEMPO = 0x51,
	};

	enum MIDIController {
		CONTROLLER_BANK_SELECT_MSB = 0,
		CONTROLLER_MODULATION_WHEEL_MSB = 1,
		CONTROLLER_BREATH_MSB = 2,
		CONTROLLER_FOOT_MSB = 4,
		CONTROLLER_PORTAMENTO_TIME_MSB = 5,
		CONTROLLER_DATA_ENTRY_MSB = 6,
		CONTROLLER_VOLUME_MSB = 7,
		CONTROLLER_BALANCE_MSB = 8,
		CONTROLLER_PAN_MSB = 10,
		CONTROLLER_EXPRESSION_MSB = 11,
		CONTROLLER_EFFECTS1_MSB = 12,
		CONTROLLER_EFFECTS2_MSB = 13,
		CONTROLLER_GPC1_MSB = 16,
		CONTROLLER_GPC2_MSB = 17,
		CONTROLLER_GPC3_MSB = 18,
		CONTROLLER_GPC4_MSB = 19,
		CONTROLLER_BANK_SELECT_LSB = 32,
		CONTROLLER_MODULATION_WHEEL_LSB = 33,
		CONTROLLER_BREATH_LSB = 34,
		CONTROLLER_FOOT_LSB = 36,
		CONTROLLER_PORTAMENTO_TIME_LSB = 37,
		CONTROLLER_DATA_ENTRY_LSB = 38,
		CONTROLLER_VOLUME_LSB = 39,
		CONTROLLER_BALANCE_LSB = 40,
		CONTROLLER_PAN_LSB = 42,
		CONTROLLER_EXPRESSION_LSB = 43,
		CONTROLLER_EFFECTS1_LSB = 44,
		CONTROLLER_EFFECTS2_LSB = 45,
		CONTROLLER_GPC1_LSB = 48,
		CONTROLLER_GPC2_LSB = 49,
		CONTROLLER_GPC3_LSB = 50,
		CONTROLLER_GPC4_LSB = 51,
		CONTROLLER_SUSTAIN = 64,
		CONTROLLER_PORTAMENTO = 65,
		CONTROLLER_SOSTENUTO = 66,
		CONTROLLER_SOFT_PEDAL = 67,
		CONTROLLER_LEGATO = 68,
		CONTROLLER_HOLD2 = 69,
		CONTROLLER_SOUND_CTRL1 = 70,
		CONTROLLER_SOUND_CTRL2 = 71,
		CONTROLLER_SOUND_CTRL3 = 72,
		CONTROLLER_SOUND_CTRL4 = 73,
		CONTROLLER_SOUND_CTRL5 = 74,
		CONTROLLER_SOUND_CTRL6 = 75,
		CONTROLLER_SOUND_CTRL7 = 76,
		CONTROLLER_SOUND_CTRL8 = 77,
		CONTROLLER_SOUND_CTRL9 = 78,
		CONTROLLER_SOUND_CTRL10 = 79,
		CONTROLLER_GPC5 = 80,
		CONTROLLER_GPC6 = 81,
		CONTROLLER_GPC7 = 82,
		CONTROLLER_GPC8 = 83,
		CONTROLLER_PORTAMENTO_CTRL = 84,
		CONTROLLER_FX_REVERB = 91,
		CONTROLLER_FX_TREMOLO = 92,
		CONTROLLER_FX_CHORUS = 93,
		CONTROLLER_FX_CELESTE_DETUNE = 94,
		CONTROLLER_FX_PHASER = 95,
		CONTROLLER_DATA_ENTRY_INCR = 96,
		CONTROLLER_DATA_ENTRY_DECR = 97,
		CONTROLLER_NRPN_LSB = 98,
		CONTROLLER_NRPN_MSB = 99,
		CONTROLLER_RPN_LSB = 100,
		CONTROLLER_RPN_MSB = 101,
		CONTROLLER_ALL_SOUND_OFF = 120,
		CONTROLLER_ALL_CTRL_OFF = 121,
		CONTROLLER_LOCAL_CONTROL = 122,
		CONTROLLER_ALL_NOTES_OFF = 123,
		CONTROLLER_OMNI_OFF = 124,
		CONTROLLER_OMNI_ON = 125,
		CONTROLLER_POLY_OFF = 126,
		CONTROLLER_POLY_ON = 127,
	};

private:
	friend class AudioStreamMIDI;

	Ref<AudioStreamMIDI> midi_stream;

	tsf *tsf_instance = nullptr;
	tml_message *first_msg = nullptr;
	tml_message *current_msg = nullptr;
	double playback_msec = 0.0;
	uint32_t frames_mixed = 0;
	bool active = false;
	bool suppress_signals = false;
	int loops = 0;

	struct PendingMIDIMessage {
		MIDIMessageType type;
		int channel;
		int param1;
		int param2;
	};

	static const int MIDI_CHANNEL_COUNT = 16;

	struct ChannelState {
		SafeFlag muted;
		SafeFlag solo;
		SafeNumeric<int> transpose; // semitones
		SafeNumeric<float> volume; // 0.0 - 1.0, multiplier
		SafeNumeric<int> program_override; // -1 = no override
	};

	ChannelState channel_states[MIDI_CHANNEL_COUNT];
	bool _has_any_solo() const;
	bool _is_channel_audible(int p_channel) const;

#ifdef _GDEXTENSION
	Ref<Mutex> pending_mutex;
#else
	BinaryMutex pending_mutex;
#endif
	LocalVector<PendingMIDIMessage> pending_messages;

	void _flush_pending_messages();
	void _apply_pending_message(const PendingMIDIMessage &p_msg);
	void _process_midi_events(double p_up_to_msec);
	void _apply_midi_message(tml_message *p_msg);

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

	void push_midi_message(MIDIMessageType p_type, int p_channel, int p_param1, int p_param2 = 0);

	void set_channel_muted(int p_channel, bool p_muted);
	bool is_channel_muted(int p_channel) const;

	void set_channel_solo(int p_channel, bool p_solo);
	bool is_channel_solo(int p_channel) const;

	void set_channel_transpose(int p_channel, int p_semitones);
	int get_channel_transpose(int p_channel) const;

	void set_channel_volume(int p_channel, float p_volume);
	float get_channel_volume(int p_channel) const;

	void set_channel_program_override(int p_channel, int p_program);
	int get_channel_program_override(int p_channel) const;

	int get_channel_preset_index(int p_channel) const;
	int get_channel_preset_number(int p_channel) const;
	String get_channel_preset_name(int p_channel) const;
	TypedArray<Dictionary> get_midi_channel_list() const;

	AudioStreamPlaybackMIDISF2();
	~AudioStreamPlaybackMIDISF2();
};

VARIANT_ENUM_CAST(AudioStreamPlaybackMIDISF2::MIDIMessageType);
VARIANT_ENUM_CAST(AudioStreamPlaybackMIDISF2::MIDIController);

class AudioStreamMIDI : public AudioStream {
	GDCLASS(AudioStreamMIDI, AudioStream);
#ifndef _GDEXTENSION
	OBJ_SAVE_TYPE(AudioStream);
#endif

	Ref<SoundFont2> soundfont;
	Ref<MIDI> midi;

	float tempo_scale = 1.0f;
	int transpose = 0;
	bool loop = false;
	double loop_offset = 0.0;

	friend class AudioStreamPlaybackMIDISF2;

protected:
	static void _bind_methods();

public:
	void set_soundfont(const Ref<SoundFont2> &p_soundfont);
	Ref<SoundFont2> get_soundfont() const;

	void set_midi(const Ref<MIDI> &p_midi);
	Ref<MIDI> get_midi() const;

	void set_tempo_scale(float p_scale);
	float get_tempo_scale() const;

	void set_transpose(int p_shift);
	int get_transpose() const;

	void set_loop(bool p_enable);
#ifdef _GDEXTENSION
	virtual bool _has_loop() const override;
#else
	virtual bool has_loop() const override;
#endif

	void set_loop_offset(double p_seconds);
	double get_loop_offset() const;

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

	AudioStreamMIDI();
	~AudioStreamMIDI();
};