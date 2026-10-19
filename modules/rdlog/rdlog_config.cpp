/**************************************************************************/
/*  rdlog_config.cpp                                                      */
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
 * @file rdlog_config.cpp
 *
 * Declaration, migration and loading of RDLog's project settings.
 */

#include "rdlog_config.h"

#include "core/config/project_settings.h"

const char *RDLogConfig::SETTING_LEVEL = "debug/rdlog/level";
const char *RDLogConfig::SETTING_COLOR_THEME = "debug/rdlog/color_theme";
const char *RDLogConfig::SETTING_SHOW_LOG_LEVEL_SELECTOR = "debug/rdlog/show_log_level_selector";

static const char *SETTING_DISABLE_COLORS = "debug/rdlog/disable_colors";
static const char *SETTING_FORCE_TERMSAFE_COLORS = "debug/rdlog/force_termsafe_colors";
static const char *SETTING_MAX_ARRAY_SIZE = "debug/rdlog/max_array_size";
static const char *SETTING_MAX_DICTIONARY_SIZE = "debug/rdlog/max_dictionary_size";
static const char *SETTING_MAX_DEPTH = "debug/rdlog/max_depth";
static const char *SETTING_DICTIONARY_SKIP_KEYS = "debug/rdlog/dictionary_skip_keys";
static const char *SETTING_USE_NEWLINES = "debug/rdlog/use_newlines";
static const char *SETTING_NEWLINE_MAX_DEPTH = "debug/rdlog/newline_max_depth";
static const char *SETTING_WARN_TODO = "debug/rdlog/warn_todo";
static const char *SETTING_SHOW_TIMESTAMPS = "debug/rdlog/show_timestamps";
static const char *SETTING_TIMESTAMP_TYPE = "debug/rdlog/timestamp_type";
static const char *SETTING_TIMESTAMP_FORMAT = "debug/rdlog/timestamp_format";
static const char *SETTING_PRINT_STRUCT_TYPES = "debug/rdlog/print_struct_types";

void RDLogConfig::declare_project_settings() {
	const RDLogConfig defaults;

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, SETTING_LEVEL, PROPERTY_HINT_ENUM, "Debug,Info,Warn,Error"), defaults.level);
	// Stored as a path (normally a uid:// path) rather than a Resource: project.godot is
	// parsed before resource loaders and module classes exist, so a Resource value there
	// fails to load and aborts parsing of every setting after it.
	GLOBAL_DEF(PropertyInfo(Variant::STRING, SETTING_COLOR_THEME, PROPERTY_HINT_FILE, "*.tres,*.res"), defaults.color_theme_path);
	GLOBAL_DEF(SETTING_DISABLE_COLORS, defaults.disable_colors);
	GLOBAL_DEF(SETTING_FORCE_TERMSAFE_COLORS, defaults.force_termsafe_colors);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_MAX_ARRAY_SIZE, PROPERTY_HINT_RANGE, "1,1000,1,or_greater"), defaults.max_array_size);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_MAX_DICTIONARY_SIZE, PROPERTY_HINT_RANGE, "1,1000,1,or_greater"), defaults.max_dictionary_size);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_MAX_DEPTH, PROPERTY_HINT_RANGE, "1,256,1"), defaults.max_depth);
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, SETTING_DICTIONARY_SKIP_KEYS), defaults.dictionary_skip_keys);
	GLOBAL_DEF(SETTING_USE_NEWLINES, defaults.use_newlines);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_NEWLINE_MAX_DEPTH, PROPERTY_HINT_RANGE, "-1,64,1"), defaults.newline_max_depth);
	GLOBAL_DEF(SETTING_WARN_TODO, defaults.warn_todo);
	GLOBAL_DEF(SETTING_SHOW_LOG_LEVEL_SELECTOR, false);
	GLOBAL_DEF(SETTING_SHOW_TIMESTAMPS, defaults.show_timestamps);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_TIMESTAMP_TYPE, PROPERTY_HINT_ENUM, "Unix,Ticks Msec,Ticks Usec,Human 12hr,Human 24hr"), defaults.timestamp_type);
	GLOBAL_DEF(SETTING_TIMESTAMP_FORMAT, defaults.timestamp_format);
	GLOBAL_DEF(SETTING_PRINT_STRUCT_TYPES, defaults.print_struct_types);
}

PackedStringArray RDLogConfig::migrate_legacy_settings() {
	struct Legacy {
		const char *old_key;
		const char *new_key;
	};
	static const Legacy legacy[] = {
		{ "log_gd/config/log_level", SETTING_LEVEL },
		{ "log_gd/config/color_resource_path", SETTING_COLOR_THEME },
		{ "log_gd/config/disable_colors", SETTING_DISABLE_COLORS },
		{ "log_gd/config/force_termsafe_colors", SETTING_FORCE_TERMSAFE_COLORS },
		{ "log_gd/config/max_array_size", SETTING_MAX_ARRAY_SIZE },
		{ "log_gd/config/dictionary_skip_keys", SETTING_DICTIONARY_SKIP_KEYS },
		{ "log_gd/config/use_newlines", SETTING_USE_NEWLINES },
		{ "log_gd/config/newline_max_depth", SETTING_NEWLINE_MAX_DEPTH },
		{ "log_gd/config/warn_todo", SETTING_WARN_TODO },
		{ "log_gd/config/show_log_level_selector", SETTING_SHOW_LOG_LEVEL_SELECTOR },
		{ "log_gd/config/show_timestamps", SETTING_SHOW_TIMESTAMPS },
		{ "log_gd/config/timestamp_type", SETTING_TIMESTAMP_TYPE },
		{ "log_gd/config/human_readable_timestamp_format", SETTING_TIMESTAMP_FORMAT },
	};

	PackedStringArray migrated;
	ProjectSettings *ps = ProjectSettings::get_singleton();
	ERR_FAIL_NULL_V(ps, migrated);

	for (const Legacy &entry : legacy) {
		if (!ps->has_setting(entry.old_key)) {
			continue;
		}
		const Variant old_value = ps->get_setting(entry.old_key);
		if (String(entry.new_key) == SETTING_COLOR_THEME) {
			// The addon's own themes are GDScript resources; they are not RDLogColorThemes.
			const String path = old_value;
			if (path.is_empty() || path.begins_with("res://addons/log/")) {
				continue;
			}
		}
		if (ps->get_setting(entry.new_key) != ps->property_get_revert(entry.new_key)) {
			continue; // Already configured for RDLog; don't overwrite.
		}
		ps->set_setting(entry.new_key, old_value);
		migrated.push_back(entry.old_key);
	}

	if (!migrated.is_empty()) {
		print_verbose(vformat("RDLog: copied legacy log.gd settings: %s.", String(", ").join(migrated)));
	}
	return migrated;
}

void RDLogConfig::load_from_project_settings() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	ERR_FAIL_NULL(ps);

	auto get = [&](Field p_field, const char *p_key, auto &r_value) {
		if (is_overridden(p_field) || !ps->has_setting(p_key)) {
			return;
		}
		using T = std::remove_reference_t<decltype(r_value)>;
		r_value = static_cast<T>(ps->get_setting_with_override(p_key));
	};

	get(FIELD_LEVEL, SETTING_LEVEL, level);
	get(FIELD_COLOR_THEME, SETTING_COLOR_THEME, color_theme_path);
	get(FIELD_DISABLE_COLORS, SETTING_DISABLE_COLORS, disable_colors);
	get(FIELD_FORCE_TERMSAFE_COLORS, SETTING_FORCE_TERMSAFE_COLORS, force_termsafe_colors);
	get(FIELD_MAX_ARRAY_SIZE, SETTING_MAX_ARRAY_SIZE, max_array_size);
	get(FIELD_MAX_DICTIONARY_SIZE, SETTING_MAX_DICTIONARY_SIZE, max_dictionary_size);
	get(FIELD_MAX_DEPTH, SETTING_MAX_DEPTH, max_depth);
	get(FIELD_DICTIONARY_SKIP_KEYS, SETTING_DICTIONARY_SKIP_KEYS, dictionary_skip_keys);
	get(FIELD_USE_NEWLINES, SETTING_USE_NEWLINES, use_newlines);
	get(FIELD_NEWLINE_MAX_DEPTH, SETTING_NEWLINE_MAX_DEPTH, newline_max_depth);
	get(FIELD_WARN_TODO, SETTING_WARN_TODO, warn_todo);
	get(FIELD_SHOW_TIMESTAMPS, SETTING_SHOW_TIMESTAMPS, show_timestamps);
	get(FIELD_TIMESTAMP_TYPE, SETTING_TIMESTAMP_TYPE, timestamp_type);
	get(FIELD_TIMESTAMP_FORMAT, SETTING_TIMESTAMP_FORMAT, timestamp_format);
	get(FIELD_PRINT_STRUCT_TYPES, SETTING_PRINT_STRUCT_TYPES, print_struct_types);

	max_array_size = MAX(1, max_array_size);
	max_dictionary_size = MAX(1, max_dictionary_size);
	max_depth = MAX(1, max_depth);
}
