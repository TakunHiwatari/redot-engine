/**************************************************************************/
/*  rdlog_color_theme.h                                                   */
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

#pragma once

/**
 * @file rdlog_color_theme.h
 *
 * Colour roles used by the RDLog formatter, the resolved palette handed to the
 * renderer, and the user-facing RDLogColorTheme resource.
 */

#include "core/io/resource.h"

/// What a piece of formatted output represents. The renderer looks the role up
/// in an RDLogPalette to pick a colour.
enum RDLogRole : uint8_t {
	RDLOG_ROLE_NONE, // Whitespace and layout; never coloured.
	RDLOG_ROLE_NIL,
	RDLOG_ROLE_BOOL,
	RDLOG_ROLE_INT,
	RDLOG_ROLE_FLOAT,
	RDLOG_ROLE_STRING,
	RDLOG_ROLE_STRING_NAME,
	RDLOG_ROLE_NODE_PATH,
	RDLOG_ROLE_VECTOR, // Vector components.
	RDLOG_ROLE_RECT, // Rects, transforms, planes, quaternions, AABBs, bases, projections.
	RDLOG_ROLE_COLOR,
	RDLOG_ROLE_RID,
	RDLOG_ROLE_OBJECT,
	RDLOG_ROLE_CLASS_NAME,
	RDLOG_ROLE_CALLABLE,
	RDLOG_ROLE_SIGNAL,
	RDLOG_ROLE_STRUCT,
	RDLOG_ROLE_COMMA,
	RDLOG_ROLE_COLON,
	RDLOG_ROLE_AMPERSAND,
	RDLOG_ROLE_CARET,
	RDLOG_ROLE_PREFIX_SRC,
	RDLOG_ROLE_PREFIX_ADDONS,
	RDLOG_ROLE_PREFIX_TEST,
	RDLOG_ROLE_LEVEL_DEBUG,
	RDLOG_ROLE_LEVEL_INFO,
	RDLOG_ROLE_LEVEL_WARN,
	RDLOG_ROLE_LEVEL_ERROR,
	RDLOG_ROLE_TIMESTAMP,
	RDLOG_ROLE_ELLIPSIS,
	// Rainbow roles: coloured by nesting depth from RDLogPalette::rainbow.
	RDLOG_ROLE_DELIMITER,
	RDLOG_ROLE_DICT_KEY,
	RDLOG_ROLE_MAX,
};

/// A theme resolved to BBCode colour strings. An empty string means "no colour".
struct RDLogPalette {
	String roles[RDLOG_ROLE_MAX];
	Vector<String> rainbow;
	Vector<String> backgrounds;

	_FORCE_INLINE_ const String &get(RDLogRole p_role, int p_depth) const {
		if (p_role >= RDLOG_ROLE_DELIMITER && !rainbow.is_empty()) {
			return rainbow[p_depth % rainbow.size()];
		}
		return roles[p_role];
	}

	/// Named colours that most terminals render (port of log.gd's COLORS_TERM_SAFE).
	static RDLogPalette termsafe();
};

class RDLogColorTheme : public Resource {
	GDCLASS(RDLogColorTheme, Resource);

public:
	// One entry per exported colour property. The first group keeps the property
	// names of log.gd's LogColorTheme so existing themes convert by changing their type.
	enum ThemeColor {
		THEME_COLOR_COMMA,
		THEME_COLOR_AMPERSAND,
		THEME_COLOR_PIPE,
		THEME_COLOR_CARET,
		THEME_COLOR_SRC_PREFIX,
		THEME_COLOR_ADDONS_PREFIX,
		THEME_COLOR_TEST_PREFIX,
		THEME_COLOR_NIL,
		THEME_COLOR_BOOL,
		THEME_COLOR_INT,
		THEME_COLOR_FLOAT,
		THEME_COLOR_VECTORS,
		THEME_COLOR_RECTS,
		THEME_COLOR_CLASS_NAME,
		THEME_COLOR_STRING,
		THEME_COLOR_STRING_NAME,
		THEME_COLOR_NODE_PATH,
		THEME_COLOR_TYPE_COLOR,
		THEME_COLOR_RID,
		THEME_COLOR_OBJECT,
		THEME_COLOR_CALLABLE,
		THEME_COLOR_SIGNAL,
		THEME_COLOR_ARRAY,
		THEME_COLOR_DICTIONARY,
		THEME_COLOR_PACKED_ARRAY,
		THEME_COLOR_TYPE_MAX,
		// Added by RDLog.
		THEME_COLOR_STRUCT,
		THEME_COLOR_LEVEL_DEBUG,
		THEME_COLOR_LEVEL_INFO,
		THEME_COLOR_LEVEL_WARN,
		THEME_COLOR_LEVEL_ERROR,
		THEME_COLOR_TIMESTAMP,
		THEME_COLOR_ELLIPSIS,
		THEME_COLOR_MAX,
	};

private:
	Color colors[THEME_COLOR_MAX];
	PackedColorArray colors_rainbow_delims;
	PackedColorArray bg_colors;

	void _apply_dark();
	void _apply_light();

protected:
	static void _bind_methods();

public:
	void set_theme_color(ThemeColor p_index, const Color &p_color);
	Color get_theme_color(ThemeColor p_index) const;

	void set_colors_rainbow_delims(const PackedColorArray &p_colors);
	PackedColorArray get_colors_rainbow_delims() const;

	void set_bg_colors(const PackedColorArray &p_colors);
	PackedColorArray get_bg_colors() const;

	RDLogPalette build_palette() const;

	static Ref<RDLogColorTheme> create_dark();
	static Ref<RDLogColorTheme> create_light();

	RDLogColorTheme();
};

VARIANT_ENUM_CAST(RDLogColorTheme::ThemeColor);
