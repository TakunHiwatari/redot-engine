/**************************************************************************/
/*  rdlog.cpp                                                             */
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
 * @file rdlog.cpp
 *
 * The RDLog singleton: level filtering, line assembly, output, configuration
 * and type overrides. Formatting itself lives in rdlog_formatter.cpp.
 */

#include "rdlog.h"

#include "core/config/project_settings.h"
#include "core/core_string_names.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/string/print_string.h"

/* Internal state */

void RDLog::_sync_fast_locked() {
	level_fast.set(config.level);
	warn_todo_fast.set_to(config.warn_todo);
}

void RDLog::_set_theme_locked(const Ref<RDLogColorTheme> &p_theme) {
	const Callable on_changed = callable_mp(this, &RDLog::_on_theme_changed);
	if (color_theme.is_valid() && color_theme->is_connected(CoreStringName(changed), on_changed)) {
		color_theme->disconnect(CoreStringName(changed), on_changed);
	}
	color_theme = p_theme;
	if (color_theme.is_valid()) {
		color_theme->connect(CoreStringName(changed), on_changed);
	}
	palette_dirty = true;
}

void RDLog::_on_theme_changed() {
	MutexLock lock(mutex);
	palette_dirty = true;
}

void RDLog::_load_pending_theme() {
	String path;
	{
		MutexLock lock(mutex);
		if (!theme_load_pending) {
			return;
		}
		theme_load_pending = false;
		path = config.color_theme_path;
	}

	// Loaded outside the lock: loading can run script code that logs.
	Ref<RDLogColorTheme> loaded;
	if (!path.is_empty()) {
		loaded = ResourceLoader::load(path, "RDLogColorTheme");
		if (loaded.is_null()) {
			WARN_PRINT(vformat(R"(RDLog: "%s" is not an RDLogColorTheme; using the built-in dark theme.)", path));
		}
	}

	MutexLock lock(mutex);
	if (!config.is_overridden(RDLogConfig::FIELD_COLOR_THEME) && config.color_theme_path == path) {
		_set_theme_locked(loaded);
	}
}

RDLog::Snapshot RDLog::_snapshot() {
	_load_pending_theme();

	MutexLock lock(mutex);
	if (palette_dirty) {
		if (config.force_termsafe_colors) {
			palette = RDLogPalette::termsafe();
		} else {
			palette = (color_theme.is_valid() ? color_theme : default_theme)->build_palette();
		}
		palette_dirty = false;
	}

	Snapshot snapshot;
	snapshot.config = config;
	snapshot.palette = palette;
	snapshot.overrides = type_overrides;
	snapshot.colors = !config.disable_colors && !platform_disables_colors;
	return snapshot;
}

/* Line assembly and output */

String RDLog::_timestamp(const RDLogConfig &p_config) {
	switch (p_config.timestamp_type) {
		case TIMESTAMP_UNIX:
			return itos(int64_t(Time::get_singleton()->get_unix_time_from_system()));
		case TIMESTAMP_TICKS_MSEC:
			return itos(OS::get_singleton()->get_ticks_msec());
		case TIMESTAMP_TICKS_USEC:
			return itos(OS::get_singleton()->get_ticks_usec());
		case TIMESTAMP_HUMAN_12HR:
		case TIMESTAMP_HUMAN_24HR: {
			const Dictionary time = Time::get_singleton()->get_datetime_dict_from_system();
			const int hour = time["hour"];
			Dictionary values;
			values["year"] = time["year"];
			values["month"] = vformat("%02d", int(time["month"]));
			values["day"] = vformat("%02d", int(time["day"]));
			values["minute"] = vformat("%02d", int(time["minute"]));
			values["second"] = vformat("%02d", int(time["second"]));
			values["dst"] = time["dst"];
			if (p_config.timestamp_type == TIMESTAMP_HUMAN_12HR) {
				values["hour"] = hour % 12 == 0 ? 12 : hour % 12;
				values["meridiem"] = hour < 12 ? "AM" : "PM";
			} else {
				values["hour"] = vformat("%02d", hour);
			}
			return p_config.timestamp_format.format(values);
		}
		default:
			return itos(int64_t(Time::get_singleton()->get_unix_time_from_system()));
	}
}

void RDLog::_append_head(RDLogTokenStream &r_tokens, const RDLogConfig &p_config, const RDLogCallSite &p_site) {
	static const HashMap<String, Callable> no_overrides;
	RDLogFormatter head(p_config, no_overrides, r_tokens);
	if (p_config.show_timestamps) {
		head.emit(RDLOG_ROLE_TIMESTAMP, "[" + _timestamp(p_config) + "]");
		head.emit(RDLOG_ROLE_NONE, " ");
	}
	if (p_site.valid) {
		RDLogRole role = RDLOG_ROLE_PREFIX_SRC;
		const String prefix = p_site.get_prefix(role);
		head.emit(role, prefix);
	}
}

void RDLog::_print(Kind p_kind, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
		return;
	}
	r_error.error = Callable::CallError::CALL_OK;

	// Level gate first: a filtered call does no formatting and takes no lock.
	const int level = level_fast.get();
	const bool warn_todo = warn_todo_fast.is_set();
	switch (p_kind) {
		case KIND_DEBUG:
			if (level > LEVEL_DEBUG) {
				return;
			}
			break;
		case KIND_INFO:
			if (level > LEVEL_INFO) {
				return;
			}
			break;
		case KIND_WARN:
			if (level > LEVEL_WARN) {
				return;
			}
			break;
		case KIND_TODO:
			if (level > (warn_todo ? LEVEL_WARN : LEVEL_INFO)) {
				return;
			}
			break;
		default:
			break;
	}

	const Snapshot snapshot = _snapshot();
	const RDLogCallSite site = RDLogCallSite::capture();

	RDLogFormatContext ctx;
	switch (p_kind) {
		case KIND_PRN:
			ctx = RDLogFormatContext::make_expanded(snapshot.config, 1);
			break;
		case KIND_PRNN:
			ctx = RDLogFormatContext::make_expanded(snapshot.config, 2);
			break;
		case KIND_PRNNN:
			ctx = RDLogFormatContext::make_expanded(snapshot.config, 3);
			break;
		default:
			ctx = RDLogFormatContext::make(snapshot.config);
			break;
	}

	// head: timestamp and call site. tag: level marker, shown only in the rich line.
	// body: the messages (plus [TODO]), shared by the rich line and the warning/error.
	RDLogTokenStream head;
	RDLogTokenStream tag;
	RDLogTokenStream body;
	_append_head(head, snapshot.config, site);

	RDLogFormatter tag_formatter(snapshot.config, snapshot.overrides, tag);
	switch (p_kind) {
		case KIND_DEBUG:
			tag_formatter.emit(RDLOG_ROLE_LEVEL_DEBUG, "[DEBUG]");
			break;
		case KIND_INFO:
			tag_formatter.emit(RDLOG_ROLE_LEVEL_INFO, "[INFO]");
			break;
		case KIND_WARN:
			tag_formatter.emit(RDLOG_ROLE_LEVEL_WARN, "[WARN]");
			break;
		case KIND_TODO:
			if (warn_todo) {
				tag_formatter.emit(RDLOG_ROLE_LEVEL_WARN, "[WARN]");
			}
			break;
		case KIND_ERROR:
			tag_formatter.emit(RDLOG_ROLE_LEVEL_ERROR, "[ERR]");
			break;
		default:
			break;
	}
	if (!tag.is_empty()) {
		tag_formatter.emit(RDLOG_ROLE_NONE, " ");
	}

	RDLogFormatter formatter(snapshot.config, snapshot.overrides, body);
	if (p_kind == KIND_TODO) {
		formatter.emit(RDLOG_ROLE_LEVEL_INFO, "[TODO]");
		formatter.emit(RDLOG_ROLE_NONE, " ");
	}
	for (int i = 0; i < p_argcount; i++) {
		if (i > 0) {
			formatter.emit(RDLOG_ROLE_NONE, " ", ctx.depth);
		}
		formatter.format(*p_args[i], ctx);
	}

	const RDLogPalette *palette_ptr = snapshot.colors ? &snapshot.palette : nullptr;
	const String line = rdlog_render(head, palette_ptr) + rdlog_render(tag, palette_ptr) + rdlog_render(body, palette_ptr);
	if (palette_ptr) {
		print_line_rich(line);
	} else {
		print_line(line);
	}

	const bool push = p_kind == KIND_WARN || p_kind == KIND_ERROR || (p_kind == KIND_TODO && warn_todo);
	if (!push) {
		return;
	}
	// Formatted once above; only rendered again, without colour, for the debugger.
	const String plain = rdlog_render(head, nullptr) + rdlog_render(body, nullptr);
	const ErrorHandlerType type = p_kind == KIND_ERROR ? ERR_HANDLER_ERROR : ERR_HANDLER_WARNING;
	if (site.valid) {
		// Report at the script line, not at this C++ file.
		const CharString function = site.function.utf8();
		const CharString file = site.file.utf8();
		_err_print_error(function.get_data(), file.get_data(), site.line, plain, false, type);
	} else if (type == ERR_HANDLER_ERROR) {
		ERR_PRINT(plain);
	} else {
		WARN_PRINT(plain);
	}
}

void RDLog::blank() {
	print_line("");
}

/* Formatting without printing */

String RDLog::to_pretty(const Variant &p_value, const Dictionary &p_options) {
	const Snapshot snapshot = _snapshot();
	RDLogTokenStream tokens;
	RDLogFormatter formatter(snapshot.config, snapshot.overrides, tokens);
	formatter.format(p_value, RDLogFormatContext::make(snapshot.config, p_options));
	const bool colors = snapshot.colors && !bool(p_options.get("disable_colors", false));
	return rdlog_render(tokens, colors ? &snapshot.palette : nullptr);
}

String RDLog::to_plain(const Variant &p_value, const Dictionary &p_options) {
	const Snapshot snapshot = _snapshot();
	RDLogTokenStream tokens;
	RDLogFormatter formatter(snapshot.config, snapshot.overrides, tokens);
	formatter.format(p_value, RDLogFormatContext::make(snapshot.config, p_options));
	return rdlog_render(tokens, nullptr);
}

String RDLog::to_printable(const Array &p_messages, const Dictionary &p_options) {
	const Snapshot snapshot = _snapshot();
	const bool pretty = p_options.get("pretty", true);
	const bool colors = pretty && snapshot.colors && !bool(p_options.get("disable_colors", false));
	const RDLogCallSite site = p_options.has("stack") ? RDLogCallSite::from_stack(p_options["stack"]) : RDLogCallSite::capture();
	const RDLogFormatContext ctx = RDLogFormatContext::make(snapshot.config, p_options);

	RDLogTokenStream tokens;
	_append_head(tokens, snapshot.config, site);
	RDLogFormatter formatter(snapshot.config, snapshot.overrides, tokens);
	for (int i = 0; i < p_messages.size(); i++) {
		if (i > 0) {
			formatter.emit(RDLOG_ROLE_NONE, " ");
		}
		if (pretty) {
			formatter.format(p_messages[i], ctx);
		} else {
			formatter.emit(RDLOG_ROLE_NONE, p_messages[i].stringify());
		}
	}
	return rdlog_render(tokens, colors ? &snapshot.palette : nullptr);
}

/* Runtime configuration */

#define RDLOG_SET(m_field, m_member, m_value) \
	MutexLock lock(mutex);                    \
	config.m_member = m_value;                \
	config.mark_overridden(RDLogConfig::m_field);

void RDLog::disable_colors() {
	RDLOG_SET(FIELD_DISABLE_COLORS, disable_colors, true);
}

void RDLog::enable_colors() {
	RDLOG_SET(FIELD_DISABLE_COLORS, disable_colors, false);
}

void RDLog::disable_newlines() {
	RDLOG_SET(FIELD_USE_NEWLINES, use_newlines, false);
}

void RDLog::enable_newlines() {
	RDLOG_SET(FIELD_USE_NEWLINES, use_newlines, true);
}

void RDLog::disable_warn_todo() {
	RDLOG_SET(FIELD_WARN_TODO, warn_todo, false);
	_sync_fast_locked();
}

void RDLog::enable_warn_todo() {
	RDLOG_SET(FIELD_WARN_TODO, warn_todo, true);
	_sync_fast_locked();
}

void RDLog::set_newline_max_depth(int p_depth) {
	RDLOG_SET(FIELD_NEWLINE_MAX_DEPTH, newline_max_depth, p_depth);
}

void RDLog::reset_newline_max_depth() {
	RDLOG_SET(FIELD_NEWLINE_MAX_DEPTH, newline_max_depth, RDLogConfig::DEFAULT_NEWLINE_MAX_DEPTH);
}

void RDLog::set_log_level(Level p_level) {
	ERR_FAIL_COND_MSG(p_level < LEVEL_DEBUG || p_level > LEVEL_ERROR,
			vformat("RDLog: %d is not a log level. Use RDLog.LEVEL_DEBUG (0), LEVEL_INFO (1), LEVEL_WARN (2) or LEVEL_ERROR (3); the level is unchanged.", int(p_level)));
	RDLOG_SET(FIELD_LEVEL, level, int(p_level));
	_sync_fast_locked();
}

RDLog::Level RDLog::get_log_level() const {
	return Level(level_fast.get());
}

void RDLog::show_timestamps() {
	RDLOG_SET(FIELD_SHOW_TIMESTAMPS, show_timestamps, true);
}

void RDLog::hide_timestamps() {
	RDLOG_SET(FIELD_SHOW_TIMESTAMPS, show_timestamps, false);
}

void RDLog::use_timestamp_type(TimestampType p_type) {
	ERR_FAIL_COND_MSG(p_type < TIMESTAMP_UNIX || p_type > TIMESTAMP_HUMAN_24HR,
			vformat("RDLog: %d is not a timestamp type. Use one of the RDLog.TIMESTAMP_* constants (0 to 4); the type is unchanged.", int(p_type)));
	RDLOG_SET(FIELD_TIMESTAMP_TYPE, timestamp_type, int(p_type));
}

void RDLog::use_timestamp_format(const String &p_format) {
	RDLOG_SET(FIELD_TIMESTAMP_FORMAT, timestamp_format, p_format);
}

void RDLog::set_colors_termsafe() {
	RDLOG_SET(FIELD_FORCE_TERMSAFE_COLORS, force_termsafe_colors, true);
	palette_dirty = true;
}

void RDLog::set_colors_pretty() {
	RDLOG_SET(FIELD_FORCE_TERMSAFE_COLORS, force_termsafe_colors, false);
	palette_dirty = true;
}

#undef RDLOG_SET

void RDLog::set_color_theme(const Ref<RDLogColorTheme> &p_theme) {
	MutexLock lock(mutex);
	config.mark_overridden(RDLogConfig::FIELD_COLOR_THEME);
	config.force_termsafe_colors = false;
	config.mark_overridden(RDLogConfig::FIELD_FORCE_TERMSAFE_COLORS);
	theme_load_pending = false;
	_set_theme_locked(p_theme);
}

Ref<RDLogColorTheme> RDLog::get_color_theme() {
	_load_pending_theme();
	MutexLock lock(mutex);
	return color_theme.is_valid() ? color_theme : default_theme;
}

int RDLog::get_max_array_size() const {
	MutexLock lock(mutex);
	return config.max_array_size;
}

PackedStringArray RDLog::get_dictionary_skip_keys() const {
	MutexLock lock(mutex);
	return config.dictionary_skip_keys;
}

bool RDLog::get_disable_colors() const {
	MutexLock lock(mutex);
	return config.disable_colors;
}

bool RDLog::get_use_newlines() const {
	MutexLock lock(mutex);
	return config.use_newlines;
}

int RDLog::get_newline_max_depth() const {
	MutexLock lock(mutex);
	return config.newline_max_depth;
}

bool RDLog::get_warn_todo() const {
	return warn_todo_fast.is_set();
}

bool RDLog::get_show_timestamps() const {
	MutexLock lock(mutex);
	return config.show_timestamps;
}

RDLog::TimestampType RDLog::get_timestamp_type() const {
	MutexLock lock(mutex);
	return TimestampType(config.timestamp_type);
}

String RDLog::get_timestamp_format() const {
	MutexLock lock(mutex);
	return config.timestamp_format;
}

void RDLog::rebuild_config() {
	MutexLock lock(mutex);
	const String previous_theme_path = config.color_theme_path;
	config.load_from_project_settings();
	if (!config.is_overridden(RDLogConfig::FIELD_COLOR_THEME) && config.color_theme_path != previous_theme_path) {
		theme_load_pending = true;
	}
	palette_dirty = true;
	_sync_fast_locked();
}

void RDLog::clear_runtime_overrides() {
	{
		MutexLock lock(mutex);
		config.overridden = 0;
		theme_load_pending = true;
	}
	rebuild_config();
}

/* Type overrides */

void RDLog::register_type_overwrite(const String &p_key, const Callable &p_handler) {
	ERR_FAIL_COND_MSG(p_key.is_empty(), "RDLog: type override key must not be empty.");
	ERR_FAIL_COND_MSG(!p_handler.is_valid(), "RDLog: type override handler must be a valid Callable.");
	MutexLock lock(mutex);
	type_overrides[p_key] = p_handler;
}

void RDLog::register_type_overwrites(const Dictionary &p_overrides) {
	MutexLock lock(mutex);
	for (const KeyValue<Variant, Variant> &kv : p_overrides) {
		String key;
		if (kv.key.get_type() == Variant::INT) {
			// typeof() values, as accepted by log.gd.
			const int type = kv.key;
			ERR_CONTINUE_MSG(type < 0 || type >= Variant::VARIANT_MAX, vformat("RDLog: %d is not a Variant type.", type));
			key = Variant::get_type_name(Variant::Type(type));
		} else {
			key = kv.key.stringify();
		}
		ERR_CONTINUE_MSG(kv.value.get_type() != Variant::CALLABLE, vformat(R"(RDLog: the override for "%s" is not a Callable.)", key));
		type_overrides[key] = kv.value;
	}
}

void RDLog::clear_type_overwrites() {
	MutexLock lock(mutex);
	type_overrides.clear();
}

/* Binding */

void RDLog::_bind_methods() {
	struct PrintMethod {
		const char *name;
		void (RDLog::*method)(const Variant **, int, Callable::CallError &);
	};
	static const PrintMethod print_methods[] = {
		{ "pr", &RDLog::_pr_bind },
		{ "log", &RDLog::_pr_bind },
		{ "prn", &RDLog::_prn_bind },
		{ "prnn", &RDLog::_prnn_bind },
		{ "prnnn", &RDLog::_prnnn_bind },
		{ "debug", &RDLog::_debug_bind },
		{ "info", &RDLog::_info_bind },
		{ "warn", &RDLog::_warn_bind },
		{ "todo", &RDLog::_todo_bind },
		{ "err", &RDLog::_error_bind },
		{ "error", &RDLog::_error_bind },
	};
	for (const PrintMethod &pm : print_methods) {
		MethodInfo mi;
		mi.name = pm.name;
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, pm.name, pm.method, mi, varray(), false);
	}
	ClassDB::bind_method(D_METHOD("blank"), &RDLog::blank);

	ClassDB::bind_method(D_METHOD("to_pretty", "value", "options"), &RDLog::to_pretty, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("to_plain", "value", "options"), &RDLog::to_plain, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("to_printable", "messages", "options"), &RDLog::to_printable, DEFVAL(Dictionary()));

	ClassDB::bind_method(D_METHOD("disable_colors"), &RDLog::disable_colors);
	ClassDB::bind_method(D_METHOD("enable_colors"), &RDLog::enable_colors);
	ClassDB::bind_method(D_METHOD("disable_newlines"), &RDLog::disable_newlines);
	ClassDB::bind_method(D_METHOD("enable_newlines"), &RDLog::enable_newlines);
	ClassDB::bind_method(D_METHOD("disable_warn_todo"), &RDLog::disable_warn_todo);
	ClassDB::bind_method(D_METHOD("enable_warn_todo"), &RDLog::enable_warn_todo);
	ClassDB::bind_method(D_METHOD("set_newline_max_depth", "depth"), &RDLog::set_newline_max_depth);
	ClassDB::bind_method(D_METHOD("reset_newline_max_depth"), &RDLog::reset_newline_max_depth);
	ClassDB::bind_method(D_METHOD("set_log_level", "level"), &RDLog::set_log_level);
	ClassDB::bind_method(D_METHOD("get_log_level"), &RDLog::get_log_level);
	ClassDB::bind_method(D_METHOD("show_timestamps"), &RDLog::show_timestamps);
	ClassDB::bind_method(D_METHOD("hide_timestamps"), &RDLog::hide_timestamps);
	ClassDB::bind_method(D_METHOD("use_timestamp_type", "type"), &RDLog::use_timestamp_type);
	ClassDB::bind_method(D_METHOD("use_timestamp_format", "format"), &RDLog::use_timestamp_format);
	ClassDB::bind_method(D_METHOD("set_colors_termsafe"), &RDLog::set_colors_termsafe);
	ClassDB::bind_method(D_METHOD("set_colors_pretty"), &RDLog::set_colors_pretty);
	ClassDB::bind_method(D_METHOD("set_color_theme", "theme"), &RDLog::set_color_theme);
	ClassDB::bind_method(D_METHOD("get_color_theme"), &RDLog::get_color_theme);

	ClassDB::bind_method(D_METHOD("get_max_array_size"), &RDLog::get_max_array_size);
	ClassDB::bind_method(D_METHOD("get_dictionary_skip_keys"), &RDLog::get_dictionary_skip_keys);
	ClassDB::bind_method(D_METHOD("get_disable_colors"), &RDLog::get_disable_colors);
	ClassDB::bind_method(D_METHOD("get_use_newlines"), &RDLog::get_use_newlines);
	ClassDB::bind_method(D_METHOD("get_newline_max_depth"), &RDLog::get_newline_max_depth);
	ClassDB::bind_method(D_METHOD("get_warn_todo"), &RDLog::get_warn_todo);
	ClassDB::bind_method(D_METHOD("get_show_timestamps"), &RDLog::get_show_timestamps);
	ClassDB::bind_method(D_METHOD("get_timestamp_type"), &RDLog::get_timestamp_type);
	ClassDB::bind_method(D_METHOD("get_timestamp_format"), &RDLog::get_timestamp_format);

	ClassDB::bind_method(D_METHOD("rebuild_config"), &RDLog::rebuild_config);
	ClassDB::bind_method(D_METHOD("clear_runtime_overrides"), &RDLog::clear_runtime_overrides);

	ClassDB::bind_method(D_METHOD("register_type_overwrite", "key", "handler"), &RDLog::register_type_overwrite);
	ClassDB::bind_method(D_METHOD("register_type_overwrites", "overrides"), &RDLog::register_type_overwrites);
	ClassDB::bind_method(D_METHOD("clear_type_overwrites"), &RDLog::clear_type_overwrites);

	BIND_ENUM_CONSTANT(LEVEL_DEBUG);
	BIND_ENUM_CONSTANT(LEVEL_INFO);
	BIND_ENUM_CONSTANT(LEVEL_WARN);
	BIND_ENUM_CONSTANT(LEVEL_ERROR);

	BIND_ENUM_CONSTANT(TIMESTAMP_UNIX);
	BIND_ENUM_CONSTANT(TIMESTAMP_TICKS_MSEC);
	BIND_ENUM_CONSTANT(TIMESTAMP_TICKS_USEC);
	BIND_ENUM_CONSTANT(TIMESTAMP_HUMAN_12HR);
	BIND_ENUM_CONSTANT(TIMESTAMP_HUMAN_24HR);
}

RDLog::RDLog() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "RDLog is a singleton; use the RDLog global instead of creating one.");
	singleton = this;

	default_theme = RDLogColorTheme::create_dark();
	platform_disables_colors = OS::get_singleton()->has_feature("ios") || OS::get_singleton()->has_feature("web");

	config.load_from_project_settings();
	_sync_fast_locked();

	ProjectSettings::get_singleton()->connect(SNAME("settings_changed"), callable_mp(this, &RDLog::rebuild_config));
}

RDLog::~RDLog() {
	if (singleton != this) {
		return;
	}
	ProjectSettings *ps = ProjectSettings::get_singleton();
	const Callable on_settings_changed = callable_mp(this, &RDLog::rebuild_config);
	if (ps && ps->is_connected(SNAME("settings_changed"), on_settings_changed)) {
		ps->disconnect(SNAME("settings_changed"), on_settings_changed);
	}
	_set_theme_locked(Ref<RDLogColorTheme>());
	singleton = nullptr;
}
