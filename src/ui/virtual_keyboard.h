#pragma once

#ifdef _GDEXTENSION
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/templates/hash_map.hpp>
using namespace godot;
#else
#include "scene/gui/control.h"
#include "core/templates/hash_map.h"
#endif


class VirtualKeyboard : public Control {
	GDCLASS(VirtualKeyboard, Control);

public:
	enum KeyState {
		KEY_STATE_NORMAL,
		KEY_STATE_PRESSED,
		KEY_STATE_HOVER,
	};

private:
	int octave_start = 3;
	int octave_count = 2;
	bool disable_input = false;

	// -1 means no key pressed
	int pressed_note = -1;
	int hovered_note = -1;

	HashMap<int, Color> key_color_overrides;

	struct ThemeCache {
		Color white_key_color = Color(0.95, 0.95, 0.95);
		Color white_key_pressed_color = Color(0.75, 0.82, 0.92);
		Color white_key_hover_color = Color(0.88, 0.88, 0.92);
		Color black_key_color = Color(0.15, 0.15, 0.15);
		Color black_key_pressed_color = Color(0.35, 0.42, 0.55);
		Color black_key_hover_color = Color(0.25, 0.25, 0.3);
		Color separator_color = Color(0.6, 0.6, 0.6);
		int white_key_width = 24;
		int black_key_width_ratio = 60; // percentage of white key width
		int black_key_height_ratio = 60; // percentage of total height
		int separator_width = 1;
	} theme_cache;

	static bool _is_black_key(int p_note_in_octave);
	static int _white_key_index(int p_note_in_octave);
	int _get_white_key_count() const;
	int _note_at_position(const Vector2 &p_pos) const;
	Rect2 _get_white_key_rect(int p_white_index) const;
	Rect2 _get_black_key_rect(int p_white_index, int p_note_in_octave) const;

	void _press_note(int p_note);
	void _release_note();

protected:
	void _notification(int p_what);
#ifndef _GDEXTENSION
	virtual void _update_theme_item_cache() override;
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
#endif
	static void _bind_methods();

public:
#ifdef _GDEXTENSION
	virtual void _gui_input(const Ref<InputEvent> &p_event) override;
#endif
	void set_octave_start(int p_octave);
	int get_octave_start() const;

	void set_octave_count(int p_count);
	int get_octave_count() const;

	void set_disable_input(bool p_disable);
	bool get_disable_input() const;

	int get_pressed_note() const;

	void set_key_color_override(int p_note, const Color &p_color);
	void remove_key_color_override(int p_note);
	void clear_key_color_overrides();
	bool has_key_color_override(int p_note) const;
	Color get_key_color_override(int p_note) const;

#ifdef _GDEXTENSION
	virtual Vector2 _get_minimum_size() const override;
#else
	virtual Size2 get_minimum_size() const override;
#endif

	VirtualKeyboard();
	~VirtualKeyboard();
};

VARIANT_ENUM_CAST(VirtualKeyboard::KeyState);