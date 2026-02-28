extends VBoxContainer
class_name MIDITrack

@onready var track_name : Label = %track_name
@onready var bank_selector : OptionButton = %bank
@onready var mute_btn : CheckButton = %mute
@onready var solo_btn : CheckButton = %solo
@onready var transpose_edit : SpinBox = %transpose
@onready var vol_slider : Slider = %vol_slider
@onready var pan_slider : Slider = %pan_slider
@onready var pitch_slider : Slider = %pitch_slider
@onready var vk : VirtualKeyboard = %vk

var channel : int = -1
var playback : AudioStreamPlaybackMIDISF2
var _preset_map : Dictionary
var _highlight_color := Color(0.2, 0.6, 1.0, 0.8)
var _updating_from_midi := false

func setup(ch_info : Dictionary, pb : AudioStreamPlaybackMIDISF2, sf2 : SoundFont2) -> void :
	channel = ch_info["channel"]
	playback = pb

	var ch_label := "CH %d" % channel
	if ch_info.get("is_drum", false) :
		ch_label += " [DRUM]"
	var preset : String = ch_info.get("preset_name", "")
	if preset != "" :
		ch_label += " - " + preset
	track_name.text = ch_label

	bank_selector.clear()
	var bank_num : int = 128 if ch_info.get("is_drum", false) else 0
	_preset_map = sf2.get_preset_list(bank_num)
	var sorted_keys : Array = _preset_map.keys()
	sorted_keys.sort()
	var current_program : int = ch_info.get("program", 0)
	
	var default_label := "Default"
	if _preset_map.has(current_program) :
		default_label += " - %s" % _preset_map[current_program]
	bank_selector.add_item(default_label, -1)
	for i in sorted_keys.size() :
		var prog : int = sorted_keys[i]
		bank_selector.add_item("%d : %s" % [prog, _preset_map[prog]], prog)
	bank_selector.selected = 0

	vol_slider.value = playback.get_channel_volume(channel)
	pan_slider.value = 0.0

	mute_btn.toggled.connect(_on_mute_toggled)
	solo_btn.toggled.connect(_on_solo_toggled)
	transpose_edit.value_changed.connect(_on_transpose_changed)
	vol_slider.value_changed.connect(_on_vol_changed)
	pan_slider.value_changed.connect(_on_pan_changed)
	pitch_slider.value_changed.connect(_on_pitch_changed)
	bank_selector.item_selected.connect(_on_bank_selected)

func _on_mute_toggled(on : bool) -> void :
	if playback :
		playback.set_channel_muted(channel, on)

func _on_solo_toggled(on : bool) -> void :
	if playback :
		playback.set_channel_solo(channel, on)

func _on_transpose_changed(val : float) -> void :
	if playback :
		clear_highlights()
		playback.set_channel_transpose(channel, int(val))

func _on_vol_changed(val : float) -> void :
	if playback and not _updating_from_midi :
		playback.set_channel_volume(channel, val)

func _on_pan_changed(val : float) -> void :
	if playback and not _updating_from_midi :
		var midi_val := clampi(int((val + 1.0) * 0.5 * 127.0), 0, 127)
		playback.push_midi_message(
			AudioStreamPlaybackMIDISF2.MESSAGE_CONTROL_CHANGE,
			channel,
			AudioStreamPlaybackMIDISF2.CONTROLLER_PAN_MSB,
			midi_val
		)

func _on_pitch_changed(val : float) -> void :
	if playback and not _updating_from_midi :
		var midi_val := clampi(int((val + 1.0) * 0.5 * 16383.0), 0, 16383)
		playback.push_midi_message(
			AudioStreamPlaybackMIDISF2.MESSAGE_PITCH_BEND,
			channel,
			midi_val
		)

func _on_bank_selected(idx : int) -> void :
	if playback :
		var prog : int = bank_selector.get_item_id(idx)
		playback.set_channel_program_override(channel, prog)
		
		var actual_prog := prog if prog >= 0 else playback.get_channel_preset_number(channel)
		if actual_prog >= 0 :
			playback.push_midi_message(
				AudioStreamPlaybackMIDISF2.MESSAGE_PROGRAM_CHANGE,
				channel,
				actual_prog
			)


func highlight_note(note : int) -> void :
	vk.set_key_color_override(note, _highlight_color)

func unhighlight_note(note : int) -> void :
	vk.remove_key_color_override(note)

func clear_highlights() -> void :
	vk.clear_key_color_overrides()

func apply_midi_cc(controller : int, value : int) -> void :
	_updating_from_midi = true
	if controller == AudioStreamPlaybackMIDISF2.CONTROLLER_VOLUME_MSB :
		vol_slider.value = value / 127.0
	elif controller == AudioStreamPlaybackMIDISF2.CONTROLLER_PAN_MSB :
		pan_slider.value = (value / 127.0) * 2.0 - 1.0
	_updating_from_midi = false

func apply_pitch_bend(value : int) -> void :
	_updating_from_midi = true
	pitch_slider.value = (value / 16383.0) * 2.0 - 1.0
	_updating_from_midi = false

func apply_program_change(program : int) -> void :
	if bank_selector.get_item_id(0) == -1 :
		var label := "Default"
		if _preset_map.has(program) :
			label += " - %s" % _preset_map[program]
		bank_selector.set_item_text(0, label)

func update_playback(pb : AudioStreamPlaybackMIDISF2) -> void :
	playback = pb
	if not playback :
		return
	playback.set_channel_muted(channel, mute_btn.button_pressed)
	playback.set_channel_solo(channel, solo_btn.button_pressed)
	playback.set_channel_transpose(channel, int(transpose_edit.value))
	playback.set_channel_volume(channel, vol_slider.value)
	var midi_pan := clampi(int((pan_slider.value + 1.0) * 0.5 * 127.0), 0, 127)
	playback.push_midi_message(
		AudioStreamPlaybackMIDISF2.MESSAGE_CONTROL_CHANGE,
		channel,
		AudioStreamPlaybackMIDISF2.CONTROLLER_PAN_MSB,
		midi_pan
	)
	var midi_pitch := clampi(int((pitch_slider.value + 1.0) * 0.5 * 16383.0), 0, 16383)
	playback.push_midi_message(
		AudioStreamPlaybackMIDISF2.MESSAGE_PITCH_BEND,
		channel,
		midi_pitch
	)
	var prog : int = bank_selector.get_item_id(bank_selector.selected)
	playback.set_channel_program_override(channel, prog)
