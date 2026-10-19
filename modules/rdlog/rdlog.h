/**************************************************************************/
/*  rdlog.h                                                               */
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
 * @file rdlog.h
 *
 * The RDLog engine singleton: a native port of the log.gd pretty-printing logger.
 */

#include "rdlog_call_site.h"
#include "rdlog_color_theme.h"
#include "rdlog_config.h"
#include "rdlog_formatter.h"

#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/safe_refcount.h"

class RDLog : public Object {
	GDCLASS(RDLog, Object);

public:
	enum Level {
		LEVEL_DEBUG,
		LEVEL_INFO,
		LEVEL_WARN,
		LEVEL_ERROR,
	};

	enum TimestampType {
		TIMESTAMP_UNIX,
		TIMESTAMP_TICKS_MSEC,
		TIMESTAMP_TICKS_USEC,
		TIMESTAMP_HUMAN_12HR,
		TIMESTAMP_HUMAN_24HR,
	};

private:
	enum Kind {
		KIND_PR,
		KIND_PRN,
		KIND_PRNN,
		KIND_PRNNN,
		KIND_DEBUG,
		KIND_INFO,
		KIND_WARN,
		KIND_TODO,
		KIND_ERROR,
	};

	/// Everything a log call needs, copied under the lock so that formatting and
	/// any user hooks it calls run without holding it.
	struct Snapshot {
		RDLogConfig config;
		RDLogPalette palette;
		HashMap<String, Callable> overrides;
		bool colors = true;
	};

	static inline RDLog *singleton = nullptr;

	mutable Mutex mutex;
	RDLogConfig config;
	HashMap<String, Callable> type_overrides;
	Ref<RDLogColorTheme> color_theme; // Null means the built-in dark theme.
	Ref<RDLogColorTheme> default_theme;
	RDLogPalette palette;
	bool palette_dirty = true;
	bool theme_load_pending = true;
	bool platform_disables_colors = false;

	// Read without the lock on every call, so filtered-out calls stay cheap.
	SafeNumeric<int> level_fast;
	SafeFlag warn_todo_fast;

	void _sync_fast_locked();
	void _set_theme_locked(const Ref<RDLogColorTheme> &p_theme);
	void _on_theme_changed();
	void _load_pending_theme();
	Snapshot _snapshot();

	static String _timestamp(const RDLogConfig &p_config);
	static void _append_head(RDLogTokenStream &r_tokens, const RDLogConfig &p_config, const RDLogCallSite &p_site);

	void _print(Kind p_kind, const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	void _pr_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) { _print(KIND_PR, p_args, p_argcount, r_error); }
	void _prn_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) { _print(KIND_PRN, p_args, p_argcount, r_error); }
	void _prnn_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) { _print(KIND_PRNN, p_args, p_argcount, r_error); }
	void _prnnn_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) { _print(KIND_PRNNN, p_args, p_argcount, r_error); }
	void _debug_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) { _print(KIND_DEBUG, p_args, p_argcount, r_error); }
	void _info_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) { _print(KIND_INFO, p_args, p_argcount, r_error); }
	void _warn_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) { _print(KIND_WARN, p_args, p_argcount, r_error); }
	void _todo_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) { _print(KIND_TODO, p_args, p_argcount, r_error); }
	void _error_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) { _print(KIND_ERROR, p_args, p_argcount, r_error); }

protected:
	static void _bind_methods();

public:
	static RDLog *get_singleton() { return singleton; }

	void blank();

	String to_pretty(const Variant &p_value, const Dictionary &p_options = Dictionary());
	String to_plain(const Variant &p_value, const Dictionary &p_options = Dictionary());
	String to_printable(const Array &p_messages, const Dictionary &p_options = Dictionary());

	void disable_colors();
	void enable_colors();
	void disable_newlines();
	void enable_newlines();
	void disable_warn_todo();
	void enable_warn_todo();
	void set_newline_max_depth(int p_depth);
	void reset_newline_max_depth();
	void set_log_level(Level p_level);
	Level get_log_level() const;
	void show_timestamps();
	void hide_timestamps();
	void use_timestamp_type(TimestampType p_type);
	void use_timestamp_format(const String &p_format);
	void set_colors_termsafe();
	void set_colors_pretty();
	void set_color_theme(const Ref<RDLogColorTheme> &p_theme);
	Ref<RDLogColorTheme> get_color_theme();

	int get_max_array_size() const;
	PackedStringArray get_dictionary_skip_keys() const;
	bool get_disable_colors() const;
	bool get_use_newlines() const;
	int get_newline_max_depth() const;
	bool get_warn_todo() const;
	bool get_show_timestamps() const;
	TimestampType get_timestamp_type() const;
	String get_timestamp_format() const;

	void rebuild_config();
	void clear_runtime_overrides();

	void register_type_overwrite(const String &p_key, const Callable &p_handler);
	void register_type_overwrites(const Dictionary &p_overrides);
	void clear_type_overwrites();

	RDLog();
	~RDLog();
};

VARIANT_ENUM_CAST(RDLog::Level);
VARIANT_ENUM_CAST(RDLog::TimestampType);
