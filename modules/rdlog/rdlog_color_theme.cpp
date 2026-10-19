/**************************************************************************/
/*  rdlog_color_theme.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

/**
 * @file rdlog_color_theme.cpp
 *
 * RDLogColorTheme resource and the built-in palettes. The dark and light values
 * are the ones shipped in log.gd's color_theme_dark.tres / color_theme_light.tres.
 */

#include "rdlog_color_theme.h"

#include "core/object/class_db.h"

static const char *theme_color_property_names[RDLogColorTheme::THEME_COLOR_MAX] = {
	"color_comma",
	"color_ampersand",
	"color_pipe",
	"color_carrot",
	"color_src_prefix",
	"color_addons_prefix",
	"color_test_prefix",
	"color_nil",
	"color_bool",
	"color_int",
	"color_float",
	"color_vectors",
	"color_rects",
	"color_class_name",
	"color_string",
	"color_string_name",
	"color_node_path",
	"color_type_color",
	"color_rid",
	"color_object",
	"color_callable",
	"color_signal",
	"color_array",
	"color_dictionary",
	"color_packed_array",
	"color_type_max",
	"color_struct",
	"color_level_debug",
	"color_level_info",
	"color_level_warn",
	"color_level_error",
	"color_timestamp",
	"color_ellipsis",
};

// A fully transparent colour means "leave this role uncoloured".
static String _color_to_bbcode(const Color &p_color) {
	if (p_color.a <= 0.0f) {
		return String();
	}
	return p_color.to_html(false);
}

RDLogPalette RDLogPalette::termsafe() {
	RDLogPalette p;
	const String pink = "pink";
	const String green = "green";
	for (int i = RDLOG_ROLE_NIL; i < RDLOG_ROLE_MAX; i++) {
		p.roles[i] = pink;
	}
	p.roles[RDLOG_ROLE_NONE] = String();
	p.roles[RDLOG_ROLE_INT] = green;
	p.roles[RDLOG_ROLE_FLOAT] = green;
	p.roles[RDLOG_ROLE_VECTOR] = green;
	p.roles[RDLOG_ROLE_CLASS_NAME] = "magenta";
	p.roles[RDLOG_ROLE_STRUCT] = "magenta";
	p.roles[RDLOG_ROLE_COMMA] = "red";
	p.roles[RDLOG_ROLE_AMPERSAND] = "orange";
	p.roles[RDLOG_ROLE_CARET] = "orange";
	p.roles[RDLOG_ROLE_PREFIX_SRC] = "cyan";
	p.roles[RDLOG_ROLE_PREFIX_ADDONS] = "red";
	p.roles[RDLOG_ROLE_PREFIX_TEST] = green;
	p.roles[RDLOG_ROLE_LEVEL_WARN] = "yellow";
	p.roles[RDLOG_ROLE_LEVEL_ERROR] = "red";
	p.roles[RDLOG_ROLE_TIMESTAMP] = String();
	p.rainbow = { "red", "blue", "green", "pink", "orange" };
	return p;
}

void RDLogColorTheme::_apply_dark() {
	const Color coral(1, 0.498039, 0.313726);
	const Color pink(1, 0.752941, 0.796078);
	const Color cornflower(0.392157, 0.584314, 0.929412);
	const Color cadet(0.372549, 0.619608, 0.627451);
	const Color gray(0.662745, 0.662745, 0.662745);

	for (int i = 0; i < THEME_COLOR_MAX; i++) {
		colors[i] = pink;
	}
	colors[THEME_COLOR_SRC_PREFIX] = Color(0.498039, 1, 0.831373);
	colors[THEME_COLOR_ADDONS_PREFIX] = Color(0.803922, 0.521569, 0.247059);
	colors[THEME_COLOR_TEST_PREFIX] = Color(0.678431, 1, 0.184314);
	colors[THEME_COLOR_COMMA] = Color(1, 0.411765, 0.705882);
	colors[THEME_COLOR_AMPERSAND] = coral;
	colors[THEME_COLOR_PIPE] = coral;
	colors[THEME_COLOR_CARET] = coral;
	colors[THEME_COLOR_NIL] = coral;
	colors[THEME_COLOR_INT] = cornflower;
	colors[THEME_COLOR_FLOAT] = cornflower;
	colors[THEME_COLOR_VECTORS] = cornflower;
	colors[THEME_COLOR_RECTS] = cornflower;
	colors[THEME_COLOR_CLASS_NAME] = cadet;
	colors[THEME_COLOR_STRING] = gray;
	colors[THEME_COLOR_STRUCT] = cadet;
	colors[THEME_COLOR_LEVEL_DEBUG] = gray;
	colors[THEME_COLOR_LEVEL_INFO] = gray;
	colors[THEME_COLOR_LEVEL_WARN] = Color(1, 1, 0);
	colors[THEME_COLOR_LEVEL_ERROR] = Color(1, 0, 0);
	colors[THEME_COLOR_TIMESTAMP] = Color(0, 0, 0, 0);
	colors[THEME_COLOR_ELLIPSIS] = gray;

	colors_rainbow_delims = {
		Color(0.862745, 0.0784314, 0.235294),
		cornflower,
		coral,
		pink,
		Color(0.803922, 0.521569, 0.247059),
	};
	bg_colors.clear();
}

void RDLogColorTheme::_apply_light() {
	const Color dark_red(0.545098, 0, 0);
	const Color coral(1, 0.498039, 0.313726);
	const Color purple(0.6, 0.196078, 0.8);
	const Color cornflower(0.392157, 0.584314, 0.929412);
	const Color cadet(0.372549, 0.619608, 0.627451);

	for (int i = 0; i < THEME_COLOR_MAX; i++) {
		colors[i] = dark_red;
	}
	colors[THEME_COLOR_SRC_PREFIX] = Color(0, 0.545098, 0.545098);
	colors[THEME_COLOR_ADDONS_PREFIX] = dark_red;
	colors[THEME_COLOR_TEST_PREFIX] = Color(0, 0.392157, 0);
	colors[THEME_COLOR_COMMA] = Color(0.862745, 0.0784314, 0.235294);
	colors[THEME_COLOR_AMPERSAND] = coral;
	colors[THEME_COLOR_PIPE] = coral;
	colors[THEME_COLOR_CARET] = coral;
	colors[THEME_COLOR_NIL] = coral;
	colors[THEME_COLOR_INT] = purple;
	colors[THEME_COLOR_FLOAT] = purple;
	colors[THEME_COLOR_VECTORS] = cornflower;
	colors[THEME_COLOR_RECTS] = cornflower;
	colors[THEME_COLOR_CLASS_NAME] = cadet;
	colors[THEME_COLOR_STRUCT] = cadet;
	colors[THEME_COLOR_LEVEL_WARN] = Color(0.721569, 0.52549, 0.0431373);
	colors[THEME_COLOR_LEVEL_ERROR] = Color(0.8, 0, 0);
	colors[THEME_COLOR_TIMESTAMP] = Color(0, 0, 0, 0);

	colors_rainbow_delims = {
		Color(0.862745, 0.0784314, 0.235294),
		Color(0, 0, 0.545098),
		Color(0.294118, 0, 0.509804),
		Color(0.501961, 0.501961, 0),
		Color(0.545098, 0.270588, 0.0745098),
	};
	bg_colors.clear();
}

void RDLogColorTheme::set_theme_color(ThemeColor p_index, const Color &p_color) {
	ERR_FAIL_INDEX(p_index, THEME_COLOR_MAX);
	if (colors[p_index] == p_color) {
		return;
	}
	colors[p_index] = p_color;
	emit_changed();
}

Color RDLogColorTheme::get_theme_color(ThemeColor p_index) const {
	ERR_FAIL_INDEX_V(p_index, THEME_COLOR_MAX, Color());
	return colors[p_index];
}

void RDLogColorTheme::set_colors_rainbow_delims(const PackedColorArray &p_colors) {
	colors_rainbow_delims = p_colors;
	emit_changed();
}

PackedColorArray RDLogColorTheme::get_colors_rainbow_delims() const {
	return colors_rainbow_delims;
}

void RDLogColorTheme::set_bg_colors(const PackedColorArray &p_colors) {
	bg_colors = p_colors;
	emit_changed();
}

PackedColorArray RDLogColorTheme::get_bg_colors() const {
	return bg_colors;
}

RDLogPalette RDLogColorTheme::build_palette() const {
	RDLogPalette p;
	auto set = [&](RDLogRole p_role, ThemeColor p_color) {
		p.roles[p_role] = _color_to_bbcode(colors[p_color]);
	};
	set(RDLOG_ROLE_NIL, THEME_COLOR_NIL);
	set(RDLOG_ROLE_BOOL, THEME_COLOR_BOOL);
	set(RDLOG_ROLE_INT, THEME_COLOR_INT);
	set(RDLOG_ROLE_FLOAT, THEME_COLOR_FLOAT);
	set(RDLOG_ROLE_STRING, THEME_COLOR_STRING);
	set(RDLOG_ROLE_STRING_NAME, THEME_COLOR_STRING_NAME);
	set(RDLOG_ROLE_NODE_PATH, THEME_COLOR_NODE_PATH);
	set(RDLOG_ROLE_VECTOR, THEME_COLOR_VECTORS);
	set(RDLOG_ROLE_RECT, THEME_COLOR_RECTS);
	set(RDLOG_ROLE_COLOR, THEME_COLOR_TYPE_COLOR);
	set(RDLOG_ROLE_RID, THEME_COLOR_RID);
	set(RDLOG_ROLE_OBJECT, THEME_COLOR_OBJECT);
	set(RDLOG_ROLE_CLASS_NAME, THEME_COLOR_CLASS_NAME);
	set(RDLOG_ROLE_CALLABLE, THEME_COLOR_CALLABLE);
	set(RDLOG_ROLE_SIGNAL, THEME_COLOR_SIGNAL);
	set(RDLOG_ROLE_STRUCT, THEME_COLOR_STRUCT);
	set(RDLOG_ROLE_COMMA, THEME_COLOR_COMMA);
	set(RDLOG_ROLE_COLON, THEME_COLOR_STRING);
	set(RDLOG_ROLE_AMPERSAND, THEME_COLOR_AMPERSAND);
	set(RDLOG_ROLE_CARET, THEME_COLOR_CARET);
	set(RDLOG_ROLE_PREFIX_SRC, THEME_COLOR_SRC_PREFIX);
	set(RDLOG_ROLE_PREFIX_ADDONS, THEME_COLOR_ADDONS_PREFIX);
	set(RDLOG_ROLE_PREFIX_TEST, THEME_COLOR_TEST_PREFIX);
	set(RDLOG_ROLE_LEVEL_DEBUG, THEME_COLOR_LEVEL_DEBUG);
	set(RDLOG_ROLE_LEVEL_INFO, THEME_COLOR_LEVEL_INFO);
	set(RDLOG_ROLE_LEVEL_WARN, THEME_COLOR_LEVEL_WARN);
	set(RDLOG_ROLE_LEVEL_ERROR, THEME_COLOR_LEVEL_ERROR);
	set(RDLOG_ROLE_TIMESTAMP, THEME_COLOR_TIMESTAMP);
	set(RDLOG_ROLE_ELLIPSIS, THEME_COLOR_ELLIPSIS);
	// Used only when the rainbow is empty.
	set(RDLOG_ROLE_DELIMITER, THEME_COLOR_STRING);
	set(RDLOG_ROLE_DICT_KEY, THEME_COLOR_STRING);

	for (const Color &c : colors_rainbow_delims) {
		p.rainbow.push_back(_color_to_bbcode(c));
	}
	for (const Color &c : bg_colors) {
		p.backgrounds.push_back(_color_to_bbcode(c));
	}
	return p;
}

Ref<RDLogColorTheme> RDLogColorTheme::create_dark() {
	Ref<RDLogColorTheme> theme;
	theme.instantiate();
	return theme;
}

Ref<RDLogColorTheme> RDLogColorTheme::create_light() {
	Ref<RDLogColorTheme> theme;
	theme.instantiate();
	theme->_apply_light();
	return theme;
}

void RDLogColorTheme::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_theme_color", "index", "color"), &RDLogColorTheme::set_theme_color);
	ClassDB::bind_method(D_METHOD("get_theme_color", "index"), &RDLogColorTheme::get_theme_color);
	ClassDB::bind_method(D_METHOD("set_colors_rainbow_delims", "colors"), &RDLogColorTheme::set_colors_rainbow_delims);
	ClassDB::bind_method(D_METHOD("get_colors_rainbow_delims"), &RDLogColorTheme::get_colors_rainbow_delims);
	ClassDB::bind_method(D_METHOD("set_bg_colors", "colors"), &RDLogColorTheme::set_bg_colors);
	ClassDB::bind_method(D_METHOD("get_bg_colors"), &RDLogColorTheme::get_bg_colors);

	ClassDB::bind_static_method("RDLogColorTheme", D_METHOD("create_dark"), &RDLogColorTheme::create_dark);
	ClassDB::bind_static_method("RDLogColorTheme", D_METHOD("create_light"), &RDLogColorTheme::create_light);

	ADD_GROUP("Cycles", "");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_COLOR_ARRAY, "bg_colors"), "set_bg_colors", "get_bg_colors");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_COLOR_ARRAY, "colors_rainbow_delims"), "set_colors_rainbow_delims", "get_colors_rainbow_delims");

	ADD_GROUP("Colors", "");
	for (int i = 0; i < THEME_COLOR_MAX; i++) {
		ADD_PROPERTYI(PropertyInfo(Variant::COLOR, theme_color_property_names[i]), "set_theme_color", "get_theme_color", i);
	}

	BIND_ENUM_CONSTANT(THEME_COLOR_COMMA);
	BIND_ENUM_CONSTANT(THEME_COLOR_AMPERSAND);
	BIND_ENUM_CONSTANT(THEME_COLOR_PIPE);
	BIND_ENUM_CONSTANT(THEME_COLOR_CARET);
	BIND_ENUM_CONSTANT(THEME_COLOR_SRC_PREFIX);
	BIND_ENUM_CONSTANT(THEME_COLOR_ADDONS_PREFIX);
	BIND_ENUM_CONSTANT(THEME_COLOR_TEST_PREFIX);
	BIND_ENUM_CONSTANT(THEME_COLOR_NIL);
	BIND_ENUM_CONSTANT(THEME_COLOR_BOOL);
	BIND_ENUM_CONSTANT(THEME_COLOR_INT);
	BIND_ENUM_CONSTANT(THEME_COLOR_FLOAT);
	BIND_ENUM_CONSTANT(THEME_COLOR_VECTORS);
	BIND_ENUM_CONSTANT(THEME_COLOR_RECTS);
	BIND_ENUM_CONSTANT(THEME_COLOR_CLASS_NAME);
	BIND_ENUM_CONSTANT(THEME_COLOR_STRING);
	BIND_ENUM_CONSTANT(THEME_COLOR_STRING_NAME);
	BIND_ENUM_CONSTANT(THEME_COLOR_NODE_PATH);
	BIND_ENUM_CONSTANT(THEME_COLOR_TYPE_COLOR);
	BIND_ENUM_CONSTANT(THEME_COLOR_RID);
	BIND_ENUM_CONSTANT(THEME_COLOR_OBJECT);
	BIND_ENUM_CONSTANT(THEME_COLOR_CALLABLE);
	BIND_ENUM_CONSTANT(THEME_COLOR_SIGNAL);
	BIND_ENUM_CONSTANT(THEME_COLOR_ARRAY);
	BIND_ENUM_CONSTANT(THEME_COLOR_DICTIONARY);
	BIND_ENUM_CONSTANT(THEME_COLOR_PACKED_ARRAY);
	BIND_ENUM_CONSTANT(THEME_COLOR_TYPE_MAX);
	BIND_ENUM_CONSTANT(THEME_COLOR_STRUCT);
	BIND_ENUM_CONSTANT(THEME_COLOR_LEVEL_DEBUG);
	BIND_ENUM_CONSTANT(THEME_COLOR_LEVEL_INFO);
	BIND_ENUM_CONSTANT(THEME_COLOR_LEVEL_WARN);
	BIND_ENUM_CONSTANT(THEME_COLOR_LEVEL_ERROR);
	BIND_ENUM_CONSTANT(THEME_COLOR_TIMESTAMP);
	BIND_ENUM_CONSTANT(THEME_COLOR_ELLIPSIS);
	BIND_ENUM_CONSTANT(THEME_COLOR_MAX);
}

RDLogColorTheme::RDLogColorTheme() {
	_apply_dark();
}
