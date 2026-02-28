#include "virtual_keyboard.h"

#include "scene/theme/theme_db.h"

/*
	piano key layout within one octave (C=0 through B=11):
	white keys : C(0), D(2), E(4), F(5), G(7), A(9), B(11)
	black keys : C#(1), D#(3), F#(6), G#(8), A#(10)
*/

bool VirtualKeyboard::_is_black_key(int p_note_in_octave) {
	switch (p_note_in_octave) {
		case 1:
		case 3:
		case 6:
		case 8:
		case 10:
			return true;
		default:
			return false;
	}
}

int VirtualKeyboard::_white_key_index(int p_note_in_octave) {
	static const int map[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
	return map[CLAMP(p_note_in_octave, 0, 11)];
}

int VirtualKeyboard::_get_white_key_count() const {
	return octave_count * 7;
}

Rect2 VirtualKeyboard::_get_white_key_rect(int p_white_index) const {
	float kw = (float)theme_cache.white_key_width;
	float h = get_size().y;
	return Rect2(p_white_index * kw, 0, kw, h);
}

Rect2 VirtualKeyboard::_get_black_key_rect(int p_white_index, int p_note_in_octave) const {
	float kw = (float)theme_cache.white_key_width;
	float bw = kw * theme_cache.black_key_width_ratio / 100.0f;
	float bh = get_size().y * theme_cache.black_key_height_ratio / 100.0f;

	float offset = 0.0f;
	switch (p_note_in_octave) {
		case 1 : // C#
			offset = kw - bw * 0.6f;
			break;
		case 3 : // D#
			offset = kw - bw * 0.4f;
			break;
		case 6 : // F#
			offset = kw - bw * 0.6f;
			break;
		case 8 : // G#
			offset = kw - bw * 0.5f;
			break;
		case 10 : // A#
			offset = kw - bw * 0.4f;
			break;
	}

	float x = p_white_index * kw + offset;
	return Rect2(x, 0, bw, bh);
}

int VirtualKeyboard::_note_at_position(const Vector2 &p_pos) const {
	float kw = (float)theme_cache.white_key_width;
	float total_w = _get_white_key_count() * kw;
	float h = get_size().y;

	if (p_pos.x < 0 || p_pos.x >= total_w || p_pos.y < 0 || p_pos.y >= h) {
		return -1;
	}

	// black key (only in upper part of the keyboard)
	float bh = h * theme_cache.black_key_height_ratio / 100.0f;
	if (p_pos.y < bh) {
		for (int oct = 0; oct < octave_count; oct++) {
			static const int black_notes[5] = { 1, 3, 6, 8, 10 };
			for (int i = 0; i < 5; i++) {
				int note_in_oct = black_notes[i];
				int white_idx = oct * 7 + _white_key_index(note_in_oct);
				Rect2 r = _get_black_key_rect(white_idx, note_in_oct);
				if (r.has_point(p_pos)) {
					int midi_note = (octave_start + oct) * 12 + note_in_oct;
					if (midi_note >= 0 && midi_note <= 127) {
						return midi_note;
					}
				}
			}
		}
	}

	// white key
	int white_idx = (int)(p_pos.x / kw);
	if (white_idx < 0 || white_idx >= _get_white_key_count()) {
		return -1;
	}

	int oct = white_idx / 7;
	int wk = white_idx % 7;
	static const int white_to_note[7] = { 0, 2, 4, 5, 7, 9, 11 };
	int midi_note = (octave_start + oct) * 12 + white_to_note[wk];
	if (midi_note >= 0 && midi_note <= 127) {
		return midi_note;
	}
	return -1;
}

void VirtualKeyboard::_press_note(int p_note) {
	if (pressed_note == p_note) {
		return;
	}
	if (pressed_note >= 0) {
		int old = pressed_note;
		pressed_note = -1;
		emit_signal(SNAME("note_off"), old);
	}
	if (p_note >= 0) {
		pressed_note = p_note;
		emit_signal(SNAME("note_on"), p_note);
	}
	queue_redraw();
}

void VirtualKeyboard::_release_note() {
	if (pressed_note >= 0) {
		int old = pressed_note;
		pressed_note = -1;
		emit_signal(SNAME("note_off"), old);
		queue_redraw();
	}
}

void VirtualKeyboard::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (disable_input) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->is_pressed()) {
			int note = _note_at_position(mb->get_position());
			_press_note(note);
			accept_event();
		} else {
			_release_note();
			accept_event();
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		int note = _note_at_position(mm->get_position());
		if (hovered_note != note) {
			hovered_note = note;
			queue_redraw();
		}
		if (pressed_note >= 0) {
			// this allows dragging to other keys while holding the mouse button
			if (note != pressed_note) {
				_press_note(note);
			}
			accept_event();
		}
	}
}

void VirtualKeyboard::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW : {
			float kw = (float)theme_cache.white_key_width;
			float h = get_size().y;
			int white_count = _get_white_key_count();
			float sep = (float)theme_cache.separator_width;

			// white keys
			for (int oct = 0; oct < octave_count; oct++) {
				static const int white_notes[7] = { 0, 2, 4, 5, 7, 9, 11 };
				for (int wk = 0; wk < 7; wk++) {
					int white_idx = oct * 7 + wk;
					int midi_note = (octave_start + oct) * 12 + white_notes[wk];
					Rect2 rect = _get_white_key_rect(white_idx);

					Color color = theme_cache.white_key_color;
				if (key_color_overrides.has(midi_note)) {
					color = key_color_overrides[midi_note];
				} else if (midi_note == pressed_note) {
						color = theme_cache.white_key_pressed_color;
					} else if (midi_note == hovered_note) {
						color = theme_cache.white_key_hover_color;
					}

					draw_rect(rect, color);
				}
			}

			// separators between white keys
			if (sep > 0.0f) {
				for (int i = 1; i < white_count; i++) {
					float x = i * kw;
					draw_line(Vector2(x, 0), Vector2(x, h), theme_cache.separator_color, sep);
				}
			}

			// black keys
			for (int oct = 0; oct < octave_count; oct++) {
				static const int black_notes[5] = { 1, 3, 6, 8, 10 };
				for (int i = 0; i < 5; i++) {
					int note_in_oct = black_notes[i];
					int white_idx = oct * 7 + _white_key_index(note_in_oct);
					int midi_note = (octave_start + oct) * 12 + note_in_oct;
					Rect2 rect = _get_black_key_rect(white_idx, note_in_oct);

					Color color = theme_cache.black_key_color;
					if (key_color_overrides.has(midi_note)) {
						color = key_color_overrides[midi_note];
					} else if (midi_note == pressed_note) {
						color = theme_cache.black_key_pressed_color;
					} else if (midi_note == hovered_note) {
						color = theme_cache.black_key_hover_color;
					}

					draw_rect(rect, color);
				}
			}

			// outline around the whole keyboard (needed ?)
			if (sep > 0.0f) {
				float total_w = white_count * kw;
				draw_rect(Rect2(0, 0, total_w, h), theme_cache.separator_color, false, sep);
			}
		} break;

		case NOTIFICATION_MOUSE_EXIT : {
			if (hovered_note >= 0) {
				hovered_note = -1;
				queue_redraw();
			}
		} break;

		case NOTIFICATION_THEME_CHANGED : {
			update_minimum_size();
			queue_redraw();
		} break;
	}
}

void VirtualKeyboard::_update_theme_item_cache() {
	Control::_update_theme_item_cache();

	if (!has_theme_color(SNAME("white_key_color"))) {
		theme_cache.white_key_color = Color(0.95, 0.95, 0.95);
	}
	if (!has_theme_color(SNAME("white_key_pressed_color"))) {
		theme_cache.white_key_pressed_color = Color(0.75, 0.82, 0.92);
	}
	if (!has_theme_color(SNAME("white_key_hover_color"))) {
		theme_cache.white_key_hover_color = Color(0.88, 0.88, 0.92);
	}
	if (!has_theme_color(SNAME("black_key_color"))) {
		theme_cache.black_key_color = Color(0.15, 0.15, 0.15);
	}
	if (!has_theme_color(SNAME("black_key_pressed_color"))) {
		theme_cache.black_key_pressed_color = Color(0.35, 0.42, 0.55);
	}
	if (!has_theme_color(SNAME("black_key_hover_color"))) {
		theme_cache.black_key_hover_color = Color(0.25, 0.25, 0.3);
	}
	if (!has_theme_color(SNAME("separator_color"))) {
		theme_cache.separator_color = Color(0.6, 0.6, 0.6);
	}
	if (!has_theme_constant(SNAME("white_key_width"))) {
		theme_cache.white_key_width = 24;
	}
	if (!has_theme_constant(SNAME("black_key_width_ratio"))) {
		theme_cache.black_key_width_ratio = 60;
	}
	if (!has_theme_constant(SNAME("black_key_height_ratio"))) {
		theme_cache.black_key_height_ratio = 60;
	}
	if (!has_theme_constant(SNAME("separator_width"))) {
		theme_cache.separator_width = 1;
	}
}

Size2 VirtualKeyboard::get_minimum_size() const {
	return Size2(_get_white_key_count() * (float)theme_cache.white_key_width, 0.0);
}

void VirtualKeyboard::set_octave_start(int p_octave) {
	p_octave = CLAMP(p_octave, 0, 9);
	if (octave_start == p_octave) {
		return;
	}
	octave_start = p_octave;
	_release_note();
	hovered_note = -1;
	update_minimum_size();
	queue_redraw();
}

int VirtualKeyboard::get_octave_start() const {
	return octave_start;
}

void VirtualKeyboard::set_octave_count(int p_count) {
	p_count = CLAMP(p_count, 1, 10);
	if (octave_count == p_count) {
		return;
	}
	octave_count = p_count;
	_release_note();
	hovered_note = -1;
	update_minimum_size();
	queue_redraw();
}

int VirtualKeyboard::get_octave_count() const {
	return octave_count;
}

void VirtualKeyboard::set_disable_input(bool p_disable) {
	if (disable_input == p_disable) {
		return;
	}
	disable_input = p_disable;
	if (disable_input) {
		_release_note();
		hovered_note = -1;
		queue_redraw();
	}
}

bool VirtualKeyboard::get_disable_input() const {
	return disable_input;
}

int VirtualKeyboard::get_pressed_note() const {
	return pressed_note;
}

void VirtualKeyboard::set_key_color_override(int p_note, const Color &p_color) {
	ERR_FAIL_COND(p_note < 0 || p_note > 127);
	key_color_overrides[p_note] = p_color;
	queue_redraw();
}

void VirtualKeyboard::remove_key_color_override(int p_note) {
	if (key_color_overrides.erase(p_note)) {
		queue_redraw();
	}
}

void VirtualKeyboard::clear_key_color_overrides() {
	if (key_color_overrides.is_empty()) {
		return;
	}
	key_color_overrides.clear();
	queue_redraw();
}

bool VirtualKeyboard::has_key_color_override(int p_note) const {
	return key_color_overrides.has(p_note);
}

Color VirtualKeyboard::get_key_color_override(int p_note) const {
	if (key_color_overrides.has(p_note)) {
		return key_color_overrides[p_note];
	}
	return Color();
}

void VirtualKeyboard::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_octave_start", "octave"), &VirtualKeyboard::set_octave_start);
	ClassDB::bind_method(D_METHOD("get_octave_start"), &VirtualKeyboard::get_octave_start);

	ClassDB::bind_method(D_METHOD("set_octave_count", "count"), &VirtualKeyboard::set_octave_count);
	ClassDB::bind_method(D_METHOD("get_octave_count"), &VirtualKeyboard::get_octave_count);

	ClassDB::bind_method(D_METHOD("set_disable_input", "disable"), &VirtualKeyboard::set_disable_input);
	ClassDB::bind_method(D_METHOD("get_disable_input"), &VirtualKeyboard::get_disable_input);

	ClassDB::bind_method(D_METHOD("get_pressed_note"), &VirtualKeyboard::get_pressed_note);

	ClassDB::bind_method(D_METHOD("set_key_color_override", "note", "color"), &VirtualKeyboard::set_key_color_override);
	ClassDB::bind_method(D_METHOD("remove_key_color_override", "note"), &VirtualKeyboard::remove_key_color_override);
	ClassDB::bind_method(D_METHOD("clear_key_color_overrides"), &VirtualKeyboard::clear_key_color_overrides);
	ClassDB::bind_method(D_METHOD("has_key_color_override", "note"), &VirtualKeyboard::has_key_color_override);
	ClassDB::bind_method(D_METHOD("get_key_color_override", "note"), &VirtualKeyboard::get_key_color_override);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "octave_start", PROPERTY_HINT_RANGE, "0,9,1"), "set_octave_start", "get_octave_start");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "octave_count", PROPERTY_HINT_RANGE, "1,10,1"), "set_octave_count", "get_octave_count");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disable_input"), "set_disable_input", "get_disable_input");

	ADD_SIGNAL(MethodInfo("note_on", PropertyInfo(Variant::INT, "note")));
	ADD_SIGNAL(MethodInfo("note_off", PropertyInfo(Variant::INT, "note")));

	BIND_ENUM_CONSTANT(KEY_STATE_NORMAL);
	BIND_ENUM_CONSTANT(KEY_STATE_PRESSED);
	BIND_ENUM_CONSTANT(KEY_STATE_HOVER);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, VirtualKeyboard, white_key_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, VirtualKeyboard, white_key_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, VirtualKeyboard, white_key_hover_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, VirtualKeyboard, black_key_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, VirtualKeyboard, black_key_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, VirtualKeyboard, black_key_hover_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, VirtualKeyboard, separator_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, VirtualKeyboard, white_key_width);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, VirtualKeyboard, black_key_width_ratio);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, VirtualKeyboard, black_key_height_ratio);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, VirtualKeyboard, separator_width);
}

VirtualKeyboard::VirtualKeyboard() {
}

VirtualKeyboard::~VirtualKeyboard() {
}
