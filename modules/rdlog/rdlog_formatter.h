/**************************************************************************/
/*  rdlog_formatter.h                                                     */
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
 * @file rdlog_formatter.h
 *
 * Turns Variants into a stream of coloured tokens, and renders token streams
 * as BBCode (for print_rich / RichTextLabel) or plain text.
 */

#include "rdlog_color_theme.h"
#include "rdlog_config.h"

#include "core/object/object_id.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "core/variant/callable.h"

struct RDLogToken {
	String text;
	RDLogRole role = RDLOG_ROLE_NONE;
	uint16_t depth = 0;
};

typedef LocalVector<RDLogToken> RDLogTokenStream;

/// Per-call formatting state. Plain value type: copied, never shared, so recursion
/// needs no allocation.
struct RDLogFormatContext {
	int depth = 0; // Container nesting depth; selects rainbow colours.
	int indent = 0; // Indentation level when laying out on several lines.
	int newline_max_depth = -1; // Containers at this depth or deeper stay on one line; -1 = no limit.
	bool newlines = false;

	/// Builds the starting context from the config, applying the optional
	/// script-facing option keys (`newlines`, `newline_max_depth`,
	/// `newline_depth`, `indent_level`).
	static RDLogFormatContext make(const RDLogConfig &p_config, const Dictionary &p_options = Dictionary());

	/// Same as make(), but forces multi-line layout down to `p_newline_max_depth`
	/// (used by prn / prnn / prnnn).
	static RDLogFormatContext make_expanded(const RDLogConfig &p_config, int p_newline_max_depth);
};

class RDLogFormatter {
	const RDLogConfig &config;
	const HashMap<String, Callable> &overrides;
	RDLogTokenStream &out;

	HashSet<const void *> visiting_containers;
	HashSet<ObjectID> visiting_objects;
	int hook_depth = 0;

	bool _uses_newlines(const RDLogFormatContext &p_ctx, int p_count) const;
	RDLogFormatContext _child(const RDLogFormatContext &p_ctx, bool p_newlines) const;
	void _separator(const RDLogFormatContext &p_ctx, bool p_newlines, int p_index);
	void _close(const RDLogFormatContext &p_ctx, bool p_newlines, int p_count, const String &p_text);
	void _more(const RDLogFormatContext &p_ctx, bool p_newlines, int p_shown, int p_remaining);

	bool _try_override(const Variant &p_value, const String &p_key, const RDLogFormatContext &p_ctx);
	bool _call_hook(const Callable &p_callable, const Variant &p_value, const RDLogFormatContext &p_ctx);

	void _format_vector(const Variant &p_value, int p_components, const RDLogFormatContext &p_ctx);
	void _format_array(const Array &p_array, const RDLogFormatContext &p_ctx);
	void _format_dictionary(const Dictionary &p_dict, const RDLogFormatContext &p_ctx);
	void _format_struct(const Variant &p_value, const RDLogFormatContext &p_ctx);
	void _format_object(const Variant &p_value, const RDLogFormatContext &p_ctx);
	void _format_object_builtin(Object *p_object, const RDLogFormatContext &p_ctx);

	template <typename T>
	void _format_packed(const T &p_array, const RDLogFormatContext &p_ctx);

	void _format(const Variant &p_value, const RDLogFormatContext &p_ctx, bool p_allow_override);

public:
	void emit(RDLogRole p_role, const String &p_text, int p_depth = 0);
	void format(const Variant &p_value, const RDLogFormatContext &p_ctx);

	RDLogFormatter(const RDLogConfig &p_config, const HashMap<String, Callable> &p_overrides, RDLogTokenStream &r_out) :
			config(p_config), overrides(p_overrides), out(r_out) {}
};

/// Renders tokens. With a palette, produces BBCode with colour tags and escapes
/// any user text that would otherwise be read as a BBCode tag. Without one,
/// produces plain text.
String rdlog_render(const RDLogTokenStream &p_tokens, const RDLogPalette *p_palette);

/// Escapes `[` as `[lb]` wherever it starts a tag that print_rich or
/// RichTextLabel would interpret. Other brackets are left alone so that
/// output such as `[1, 2]` stays readable in a terminal.
String rdlog_escape_bbcode(const String &p_text);
