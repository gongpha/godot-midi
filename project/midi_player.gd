extends Control
class_name MIDIPlayer

@onready var player : AudioStreamPlayer = %player

@onready var tracks_box : VBoxContainer = %tracks_box

const MIDITRACK := preload("res://track.tscn")

@onready var edit_mid : LineEdit = %edit_mid
@onready var browse_mid : Button = %browse_mid
@onready var midi_chooser : FileDialog = %midi_chooser

@onready var edit_sf2 : LineEdit = %edit_sf2
@onready var browse_sf2 : Button = %browse_sf2
@onready var sf2_chooser : FileDialog = %sf2_chooser

@onready var play_pause_button : Button = %play_pause_button
@onready var stop_button : Button = %stop_button
@onready var seek_bar : Slider = %seek_bar
@onready var tempo_scale : Slider = %tempo_scale
@onready var tempo_scale_spinbox : SpinBox = %tempo_scale_spinbox
@onready var bpm : SpinBox = %bpm

enum PlayState { STOPPED, PLAYING, PAUSED }
var _state := PlayState.STOPPED

var _midi_res : MIDI
var _sf2_res : SoundFont2
var _playback : AudioStreamPlaybackMIDISF2
var _midi_stream : AudioStreamMIDI
var _seeking := false

var _channel_tracks : Dictionary = {}

func _ready() -> void :
	browse_mid.pressed.connect(func() -> void : midi_chooser.popup_centered(Vector2i(800, 500)))
	browse_sf2.pressed.connect(func() -> void : sf2_chooser.popup_centered(Vector2i(800, 500)))
	midi_chooser.file_selected.connect(_on_midi_file_selected)
	sf2_chooser.file_selected.connect(_on_sf2_file_selected)

	play_pause_button.pressed.connect(_on_play_pause)
	stop_button.pressed.connect(_on_stop)
	tempo_scale.value_changed.connect(_on_tempo_scale_changed)
	
	tempo_scale.share(tempo_scale_spinbox)

	seek_bar.drag_started.connect(func() -> void : _seeking = true)
	seek_bar.drag_ended.connect(_on_seek_drag_ended)

	var sf_stream : AudioStreamSoundfontPlayer = player.stream
	if sf_stream and sf_stream.soundfont :
		_sf2_res = sf_stream.soundfont

	_update_button_label()

func _process(_delta : float) -> void :
	if _state == PlayState.PLAYING and _playback and not _seeking :
		var pos := player.get_playback_position()
		var length := _midi_stream.get_length() if _midi_stream else 0.0
		if length > 0.0 :
			seek_bar.max_value = length
			seek_bar.value = pos

	if _state == PlayState.PLAYING and not player.playing :
		_clear_all_highlights()
		_state = PlayState.STOPPED
		_update_button_label()

func _on_midi_file_selected(path : String) -> void :
	var fa := FileAccess.open(path, FileAccess.READ)
	if not fa :
		push_error("Cannot open MIDI file : " + path)
		return
	var data := fa.get_buffer(fa.get_length())
	fa.close()
	var midi := MIDI.load_from_buffer(data)
	if not midi :
		push_error("Failed to parse MIDI : " + path)
		return
	_midi_res = midi
	edit_mid.text = path
	_stop()
	_try_setup_stream()

func _on_sf2_file_selected(path : String) -> void :
	var fa := FileAccess.open(path, FileAccess.READ)
	if not fa :
		push_error("Cannot open SF2 file : " + path)
		return
	var data := fa.get_buffer(fa.get_length())
	fa.close()
	var sf2 := SoundFont2.load_from_buffer(data)
	if not sf2 :
		push_error("Failed to parse SF2 : " + path)
		return
	_sf2_res = sf2
	edit_sf2.text = path
	_stop()
	_try_setup_stream()

func _try_setup_stream() -> void :
	if not _midi_res or not _sf2_res :
		return

	_midi_stream = AudioStreamMIDI.new()
	_midi_stream.midi = _midi_res
	_midi_stream.soundfont = _sf2_res
	_midi_stream.tempo_scale = tempo_scale.value

	player.stream = _midi_stream
	player.play()
	_playback = player.get_stream_playback() as AudioStreamPlaybackMIDISF2
	player.stop()

	if _playback :
		_setup_tracks()

func _setup_tracks() -> void :
	for child in tracks_box.get_children() :
		child.queue_free()
	_channel_tracks.clear()

	var channels : Array[Dictionary] = _playback.get_midi_channel_list()
	for ch_info : Dictionary in channels :
		var track_node : MIDITrack = MIDITRACK.instantiate()
		tracks_box.add_child(track_node)
		track_node.setup(ch_info, _playback, _sf2_res)
		_channel_tracks[ch_info["channel"]] = track_node

	_connect_playback_signal()

	seek_bar.value = 0.0
	seek_bar.max_value = _midi_stream.get_length() if _midi_stream else 1.0

func _connect_playback_signal() -> void :
	if _playback and not _playback.applied_midi_message.is_connected(_on_midi_message) :
		_playback.applied_midi_message.connect(_on_midi_message)

func _reacquire_playback() -> void :
	var new_pb := player.get_stream_playback() as AudioStreamPlaybackMIDISF2
	if not new_pb :
		return
	_playback = new_pb
	_connect_playback_signal()
	for track : MIDITrack in _channel_tracks.values() :
		track.update_playback(_playback)

func _on_play_pause() -> void :
	match _state :
		PlayState.STOPPED :
			_play()
		PlayState.PLAYING :
			_pause()
		PlayState.PAUSED :
			_resume()

func _on_stop() -> void :
	_stop()

func _play() -> void :
	if not _midi_stream :
		return
	bpm.value = 120
	player.play()
	_playback = player.get_stream_playback() as AudioStreamPlaybackMIDISF2
	if _playback :
		_setup_tracks()
	_state = PlayState.PLAYING
	_update_button_label()

func _pause() -> void :
	player.stream_paused = true
	_state = PlayState.PAUSED
	_update_button_label()

func _resume() -> void :
	player.stream_paused = false
	_state = PlayState.PLAYING
	_update_button_label()

func _stop() -> void :
	player.stop()
	player.stream_paused = false
	_clear_all_highlights()
	_state = PlayState.STOPPED
	seek_bar.value = 0.0
	_update_button_label()

func _update_button_label() -> void :
	match _state :
		PlayState.STOPPED :
			play_pause_button.text = "PLAY"
		PlayState.PLAYING :
			play_pause_button.text = "PAUSE"
		PlayState.PAUSED :
			play_pause_button.text = "RESUME"

func _on_seek_drag_ended(_value_changed : bool) -> void :
	_seeking = false
	var was_paused := (_state == PlayState.PAUSED)
	if player.playing or was_paused :
		_clear_all_highlights()
		if was_paused :
			player.stream_paused = false
		player.seek(seek_bar.value)
		_reacquire_playback()
		if was_paused :
			player.stream_paused = true

func _on_tempo_scale_changed(val : float) -> void :
	if _midi_stream :
		_midi_stream.tempo_scale = val

func _clear_all_highlights() -> void :
	for track : MIDITrack in _channel_tracks.values() :
		track.clear_highlights()

func _on_midi_message(type : int, channel : int, param1 : int, param2 : int) -> void :
	if type == AudioStreamPlaybackMIDISF2.MESSAGE_SET_TEMPO :
		if param1 > 0 :
			bpm.value = param1
		return

	var track : MIDITrack = _channel_tracks.get(channel)
	if not track :
		return

	# tip : NOTE_ON with velocity 0 is equivalent to NOTE_OFF
	if type == AudioStreamPlaybackMIDISF2.MESSAGE_NOTE_ON and param2 > 0 :
		track.highlight_note(param1)
	elif (
		type == AudioStreamPlaybackMIDISF2.MESSAGE_NOTE_OFF or
		(type == AudioStreamPlaybackMIDISF2.MESSAGE_NOTE_ON and param2 == 0)
	) :
		track.unhighlight_note(param1)
	elif type == AudioStreamPlaybackMIDISF2.MESSAGE_CONTROL_CHANGE :
		track.apply_midi_cc(param1, param2)
	elif type == AudioStreamPlaybackMIDISF2.MESSAGE_PITCH_BEND :
		track.apply_pitch_bend(param1)
	elif type == AudioStreamPlaybackMIDISF2.MESSAGE_PROGRAM_CHANGE :
		track.apply_program_change(param1)
