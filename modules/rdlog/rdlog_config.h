/**************************************************************************/
/*  rdlog_config.h                                                        */
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
 * @file rdlog_config.h
 *
 * Snapshot of RDLog's project settings, plus the code that declares them,
 * migrates log.gd's old `log_gd/config/...` keys and reloads them on change.
 */

#include "core/string/ustring.h"
#include "core/variant/variant.h"

struct RDLogConfig {
	/// Bit positions in `overridden`. A set bit means the value was changed from
	/// script at runtime and must survive reloads from ProjectSettings.
	enum Field : uint32_t {
		FIELD_LEVEL,
		FIELD_DISABLE_COLORS,
		FIELD_FORCE_TERMSAFE_COLORS,
		FIELD_USE_NEWLINES,
		FIELD_NEWLINE_MAX_DEPTH,
		FIELD_WARN_TODO,
		FIELD_SHOW_TIMESTAMPS,
		FIELD_TIMESTAMP_TYPE,
		FIELD_TIMESTAMP_FORMAT,
		FIELD_MAX_ARRAY_SIZE,
		FIELD_MAX_DICTIONARY_SIZE,
		FIELD_MAX_DEPTH,
		FIELD_DICTIONARY_SKIP_KEYS,
		FIELD_PRINT_STRUCT_TYPES,
		FIELD_COLOR_THEME,
		FIELD_MAX,
	};

	static constexpr int DEFAULT_LEVEL = 1; // RDLog::LEVEL_INFO
	static constexpr int DEFAULT_NEWLINE_MAX_DEPTH = -1;
	static constexpr int DEFAULT_TIMESTAMP_TYPE = 3; // RDLog::TIMESTAMP_HUMAN_12HR
	static constexpr int DEFAULT_MAX_ARRAY_SIZE = 20;
	static constexpr int DEFAULT_MAX_DICTIONARY_SIZE = 20;
	static constexpr int DEFAULT_MAX_DEPTH = 32;

	int level = DEFAULT_LEVEL;
	bool disable_colors = false;
	bool force_termsafe_colors = false;
	bool use_newlines = false;
	int newline_max_depth = DEFAULT_NEWLINE_MAX_DEPTH;
	bool warn_todo = true;
	bool show_timestamps = false;
	int timestamp_type = DEFAULT_TIMESTAMP_TYPE;
	String timestamp_format = "{hour}:{minute}:{second}";
	int max_array_size = DEFAULT_MAX_ARRAY_SIZE;
	int max_dictionary_size = DEFAULT_MAX_DICTIONARY_SIZE;
	int max_depth = DEFAULT_MAX_DEPTH;
	PackedStringArray dictionary_skip_keys = { "layer_0/tile_data" };
	bool print_struct_types = false;
	String color_theme_path; // Empty means the built-in dark theme.

	uint32_t overridden = 0;

	_FORCE_INLINE_ void mark_overridden(Field p_field) { overridden |= (1u << p_field); }
	_FORCE_INLINE_ bool is_overridden(Field p_field) const { return overridden & (1u << p_field); }

	/// Reloads every field from ProjectSettings except overridden ones.
	void load_from_project_settings();

	/// Registers the `debug/rdlog/...` settings with their hints and defaults.
	static void declare_project_settings();

	/// Copies log.gd's `log_gd/config/...` values into `debug/rdlog/...` where the new
	/// key is still at its default. Returns the old keys that were copied.
	static PackedStringArray migrate_legacy_settings();

	static const char *SETTING_LEVEL;
	static const char *SETTING_COLOR_THEME;
	static const char *SETTING_SHOW_LOG_LEVEL_SELECTOR;
};
