/**************************************************************************/
/*  rdlog_formatter.cpp                                                   */
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
 * @file rdlog_formatter.cpp
 *
 * Variant → token formatting rules and the BBCode / plain renderers.
 */

#include "rdlog_formatter.h"

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/object/script_language.h"
#include "core/string/string_builder.h"
#include "core/variant/struct.h"
#include "core/variant/struct_info.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

static String _tabs(int p_count) {
	return String::chr('\t').repeat(MAX(0, p_count));
}

/* RDLogFormatContext */

RDLogFormatContext RDLogFormatContext::make(const RDLogConfig &p_config, const Dictionary &p_options) {
	RDLogFormatContext ctx;
	ctx.newlines = p_options.get("newlines", p_config.use_newlines);
	ctx.newline_max_depth = p_options.get("newline_max_depth", p_config.newline_max_depth);
	ctx.depth = MAX(0, int(p_options.get("newline_depth", 0)));
	ctx.indent = MAX(0, int(p_options.get("indent_level", 0)));
	if (!ctx.newlines) {
		ctx.newline_max_depth = 0;
	} else if (ctx.newline_max_depth == 0) {
		ctx.newlines = false;
	}
	return ctx;
}

RDLogFormatContext RDLogFormatContext::make_expanded(const RDLogConfig &p_config, int p_newline_max_depth) {
	RDLogFormatContext ctx;
	ctx.newlines = true;
	ctx.newline_max_depth = p_newline_max_depth;
	return ctx;
}

/* RDLogFormatter: layout helpers */

void RDLogFormatter::emit(RDLogRole p_role, const String &p_text, int p_depth) {
	RDLogToken token;
	token.text = p_text;
	token.role = p_role;
	token.depth = uint16_t(CLAMP(p_depth, 0, UINT16_MAX));
	out.push_back(token);
}

bool RDLogFormatter::_uses_newlines(const RDLogFormatContext &p_ctx, int p_count) const {
	if (!p_ctx.newlines || p_count < 2) {
		return false;
	}
	return p_ctx.newline_max_depth < 0 || p_ctx.depth < p_ctx.newline_max_depth;
}

RDLogFormatContext RDLogFormatter::_child(const RDLogFormatContext &p_ctx, bool p_newlines) const {
	RDLogFormatContext child = p_ctx;
	child.depth++;
	if (p_newlines) {
		child.indent++;
	}
	return child;
}

void RDLogFormatter::_separator(const RDLogFormatContext &p_ctx, bool p_newlines, int p_index) {
	if (p_index > 0) {
		emit(RDLOG_ROLE_COMMA, ",", p_ctx.depth);
	}
	if (p_newlines) {
		emit(RDLOG_ROLE_NONE, "\n" + _tabs(p_ctx.indent + 1), p_ctx.depth);
	} else {
		emit(RDLOG_ROLE_NONE, " ", p_ctx.depth);
	}
}

void RDLogFormatter::_close(const RDLogFormatContext &p_ctx, bool p_newlines, int p_count, const String &p_text) {
	if (p_count > 0) {
		emit(RDLOG_ROLE_NONE, p_newlines ? "\n" + _tabs(p_ctx.indent) : String(" "), p_ctx.depth);
	}
	emit(RDLOG_ROLE_DELIMITER, p_text, p_ctx.depth);
}

void RDLogFormatter::_more(const RDLogFormatContext &p_ctx, bool p_newlines, int p_shown, int p_remaining) {
	_separator(p_ctx, p_newlines, p_shown);
	emit(RDLOG_ROLE_ELLIPSIS, vformat("... (%d more)", p_remaining), p_ctx.depth);
}

/* RDLogFormatter: hooks */

bool RDLogFormatter::_try_override(const Variant &p_value, const String &p_key, const RDLogFormatContext &p_ctx) {
	if (hook_depth >= config.max_depth) {
		return false;
	}
	const Callable *handler = overrides.getptr(p_key);
	if (!handler) {
		return false;
	}
	return _call_override(p_key, *handler, p_value, p_ctx);
}

bool RDLogFormatter::_call_override(const String &p_key, const Callable &p_callable, const Variant &p_value, const RDLogFormatContext &p_ctx) {
	const Variant *args[1] = { &p_value };
	Variant result;
	Callable::CallError ce;
	p_callable.callp(args, 1, result, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		failures.push_back(vformat(R"(RDLog: the type override for "%s" failed (%s); printing the value without it.)", p_key, Variant::get_callable_error_text(p_callable, args, 1, ce)));
		return false;
	}
	hook_depth++;
	// The result is not offered to overrides again at this level, so an override
	// that returns a value of its own type cannot recurse forever.
	_format(result, p_ctx, false);
	hook_depth--;
	return true;
}

/* RDLogFormatter: per-type rules */

void RDLogFormatter::_format_vector(const Variant &p_value, int p_components, const RDLogFormatContext &p_ctx) {
	emit(RDLOG_ROLE_DELIMITER, "(", p_ctx.depth);
	for (int i = 0; i < p_components; i++) {
		if (i > 0) {
			emit(RDLOG_ROLE_COMMA, ",", p_ctx.depth);
		}
		bool valid = false;
		const Variant component = p_value.get(i, &valid);
		emit(RDLOG_ROLE_VECTOR, component.stringify(), p_ctx.depth);
	}
	emit(RDLOG_ROLE_DELIMITER, ")", p_ctx.depth);
}

void RDLogFormatter::_format_array(const Array &p_array, const RDLogFormatContext &p_ctx) {
	const void *id = p_array.id();
	if (visiting_containers.has(id) || p_ctx.depth >= config.max_depth) {
		emit(RDLOG_ROLE_DELIMITER, "[...]", p_ctx.depth);
		return;
	}
	visiting_containers.insert(id);

	const int count = p_array.size();
	const int shown = MIN(count, config.max_array_size);
	const bool newlines = _uses_newlines(p_ctx, shown + (count > shown ? 1 : 0));
	const RDLogFormatContext child = _child(p_ctx, newlines);

	emit(RDLOG_ROLE_DELIMITER, "[", p_ctx.depth);
	for (int i = 0; i < shown; i++) {
		_separator(p_ctx, newlines, i);
		_format(p_array[i], child, true);
	}
	if (count > shown) {
		_more(p_ctx, newlines, shown, count - shown);
	}
	_close(p_ctx, newlines, count, "]");

	visiting_containers.erase(id);
}

template <typename T>
void RDLogFormatter::_format_packed(const T &p_array, const RDLogFormatContext &p_ctx) {
	if (p_ctx.depth >= config.max_depth) {
		emit(RDLOG_ROLE_DELIMITER, "[...]", p_ctx.depth);
		return;
	}
	const int count = p_array.size();
	const int shown = MIN(count, config.max_array_size);
	const bool newlines = _uses_newlines(p_ctx, shown + (count > shown ? 1 : 0));
	const RDLogFormatContext child = _child(p_ctx, newlines);

	emit(RDLOG_ROLE_DELIMITER, "[", p_ctx.depth);
	for (int i = 0; i < shown; i++) {
		_separator(p_ctx, newlines, i);
		_format(Variant(p_array[i]), child, true);
	}
	if (count > shown) {
		_more(p_ctx, newlines, shown, count - shown);
	}
	_close(p_ctx, newlines, count, "]");
}

void RDLogFormatter::_format_dictionary(const Dictionary &p_dict, const RDLogFormatContext &p_ctx) {
	const void *id = p_dict.id();
	if (visiting_containers.has(id) || p_ctx.depth >= config.max_depth) {
		emit(RDLOG_ROLE_DELIMITER, "{...}", p_ctx.depth);
		return;
	}
	visiting_containers.insert(id);

	const int count = p_dict.size();
	const int shown = MIN(count, config.max_dictionary_size);
	const bool newlines = _uses_newlines(p_ctx, shown + (count > shown ? 1 : 0));
	const RDLogFormatContext child = _child(p_ctx, newlines);

	emit(RDLOG_ROLE_DELIMITER, "{", p_ctx.depth);
	int index = 0;
	for (const KeyValue<Variant, Variant> &kv : p_dict) {
		if (index >= shown) {
			break;
		}
		_separator(p_ctx, newlines, index);
		const String key = kv.key.stringify();
		emit(RDLOG_ROLE_DICT_KEY, "\"" + key + "\"", p_ctx.depth);
		emit(RDLOG_ROLE_COLON, ": ", p_ctx.depth);
		if (config.dictionary_skip_keys.has(key)) {
			emit(RDLOG_ROLE_ELLIPSIS, "...", child.depth);
		} else {
			_format(kv.value, child, true);
		}
		index++;
	}
	if (count > shown) {
		_more(p_ctx, newlines, shown, count - shown);
	}
	_close(p_ctx, newlines, count, "}");

	visiting_containers.erase(id);
}

static String _struct_field_type(const Ref<StructInfo> &p_info, int p_index) {
	if (!p_info->is_field_typed(p_index)) {
		return "Variant";
	}
	String type;
	switch (p_info->get_field_type(p_index)) {
		case Variant::OBJECT: {
			const StringName class_name = p_info->get_field_class_name(p_index);
			type = class_name.is_empty() ? String("Object") : String(class_name);
		} break;
		case Variant::STRUCT: {
			type = p_info->get_field_struct_type_id(p_index);
		} break;
		default: {
			type = Variant::get_type_name(p_info->get_field_type(p_index));
		} break;
	}
	if (p_info->is_field_nullable(p_index)) {
		type += "?";
	}
	return type;
}

void RDLogFormatter::_format_struct(const Variant &p_value, const RDLogFormatContext &p_ctx) {
	const Struct value = p_value;
	if (value.is_null()) {
		emit(RDLOG_ROLE_STRUCT, "Struct(null)", p_ctx.depth);
		return;
	}
	emit(RDLOG_ROLE_STRUCT, value.get_type_id(), p_ctx.depth);
	emit(RDLOG_ROLE_NONE, " ", p_ctx.depth);
	if (p_ctx.depth >= config.max_depth) {
		emit(RDLOG_ROLE_DELIMITER, "{...}", p_ctx.depth);
		return;
	}

	const Ref<StructInfo> info = value.get_info();
	const int count = value.get_field_count();
	const int shown = MIN(count, config.max_dictionary_size);
	const bool newlines = _uses_newlines(p_ctx, shown + (count > shown ? 1 : 0));
	const RDLogFormatContext child = _child(p_ctx, newlines);

	emit(RDLOG_ROLE_DELIMITER, "{", p_ctx.depth);
	for (int i = 0; i < shown; i++) {
		_separator(p_ctx, newlines, i);
		emit(RDLOG_ROLE_DICT_KEY, info->get_field_name(i), p_ctx.depth);
		emit(RDLOG_ROLE_COLON, ": ", p_ctx.depth);
		if (config.print_struct_types) {
			emit(RDLOG_ROLE_CLASS_NAME, _struct_field_type(info, i), p_ctx.depth);
			emit(RDLOG_ROLE_COLON, " = ", p_ctx.depth);
		}
		_format(value.get_member(i), child, true);
	}
	if (count > shown) {
		_more(p_ctx, newlines, shown, count - shown);
	}
	_close(p_ctx, newlines, count, "}");
}

void RDLogFormatter::_format_object(const Variant &p_value, const RDLogFormatContext &p_ctx) {
	Object *object = p_value.get_validated_object();
	if (!object) {
		if (p_value.is_null()) {
			emit(RDLOG_ROLE_NIL, "<null>", p_ctx.depth);
		} else {
			emit(RDLOG_ROLE_OBJECT, "invalid instance: " + p_value.stringify(), p_ctx.depth);
		}
		return;
	}

	const ObjectID id = object->get_instance_id();

	// A node in the scene tree can only be read from the thread that processes it. From
	// any other thread, print only what is safe to read: its class and instance ID. Hooks
	// are skipped too, since they would read the node.
	const Node *node = Object::cast_to<Node>(object);
	if (node && !node->is_accessible_from_caller_thread()) {
		emit(RDLOG_ROLE_CLASS_NAME, object->get_class(), p_ctx.depth);
		emit(RDLOG_ROLE_OBJECT, "#" + itos(int64_t(id)), p_ctx.depth);
		return;
	}

	if (visiting_objects.has(id)) {
		emit(RDLOG_ROLE_OBJECT, "<cycle " + object->get_class() + ">", p_ctx.depth);
		return;
	}
	if (hook_depth >= config.max_depth) {
		// Hooks kept returning new objects; same marker style as "[...]" for containers.
		emit(RDLOG_ROLE_OBJECT, "<" + object->get_class() + "...>", p_ctx.depth);
		return;
	}

	// 1. Registered overrides: script global classes up the script chain, then native classes.
	const Callable *handler = nullptr;
	String handler_key;
	if (!overrides.is_empty()) {
		Ref<Script> script = object->get_script();
		while (!handler && script.is_valid()) {
			const StringName global_name = script->get_global_name();
			if (!global_name.is_empty()) {
				handler = overrides.getptr(global_name);
				handler_key = global_name;
			}
			script = script->get_base_script();
		}
		for (StringName cls = object->get_class_name(); !handler && cls != StringName(); cls = ClassDB::get_parent_class(cls)) {
			handler = overrides.getptr(cls);
			handler_key = cls;
		}
	}

	visiting_objects.insert(id);
	bool handled = false;
	if (handler) {
		handled = _call_override(handler_key, *handler, p_value, p_ctx);
	} else if (object->has_method(SNAME("to_pretty"))) {
		// 2. The object's own to_pretty() (also what a `LogPrintable`-style trait provides).
		Callable::CallError ce;
		const Variant result = object->callp(SNAME("to_pretty"), nullptr, 0, ce);
		if (ce.error == Callable::CallError::CALL_OK) {
			hook_depth++;
			_format(result, p_ctx, true);
			hook_depth--;
			handled = true;
		} else {
			failures.push_back(vformat("RDLog: %s.to_pretty() failed (%s); printing the object without it.", object->get_class(),
					Variant::get_call_error_text(object, SNAME("to_pretty"), nullptr, 0, ce)));
		}
	}
	visiting_objects.erase(id);

	if (!handled) {
		_format_object_builtin(object, p_ctx);
	}
}

void RDLogFormatter::_format_object_builtin(Object *p_object, const RDLogFormatContext &p_ctx) {
	const Ref<Script> script = p_object->get_script();
	String script_file;
	String script_class;
	if (script.is_valid()) {
		script_file = script->get_path().get_file();
		script_class = script->get_global_name();
	}

	if (PackedScene *scene = Object::cast_to<PackedScene>(p_object)) {
		if (!scene->get_path().is_empty()) {
			emit(RDLOG_ROLE_CLASS_NAME, "PackedScene:", p_ctx.depth);
			emit(RDLOG_ROLE_STRING, scene->get_path().get_file(), p_ctx.depth);
			return;
		}
	}

	if (Resource *resource = Object::cast_to<Resource>(p_object)) {
		if (!script_file.is_empty()) {
			emit(RDLOG_ROLE_CLASS_NAME, script_file, p_ctx.depth);
		} else if (!resource->get_path().is_empty()) {
			emit(RDLOG_ROLE_CLASS_NAME, "Resource:", p_ctx.depth);
			emit(RDLOG_ROLE_STRING, resource->get_path().get_file(), p_ctx.depth);
		} else {
			emit(RDLOG_ROLE_OBJECT, Variant(p_object).stringify(), p_ctx.depth);
		}
		return;
	}

	if (Node *node = Object::cast_to<Node>(p_object)) {
		String cls = script_class;
		if (cls.is_empty()) {
			cls = script_file.is_empty() ? p_object->get_class() : script_file;
		}
		emit(RDLOG_ROLE_CLASS_NAME, cls, p_ctx.depth);
		emit(RDLOG_ROLE_COLON, ":", p_ctx.depth);
		emit(RDLOG_ROLE_STRING, "\"" + String(node->get_name()) + "\"", p_ctx.depth);
		if (node->is_inside_tree()) {
			emit(RDLOG_ROLE_NONE, " @ ", p_ctx.depth);
			emit(RDLOG_ROLE_NODE_PATH, String(node->get_path()), p_ctx.depth);
		}
		return;
	}

	if (Object::cast_to<RefCounted>(p_object)) {
		emit(RDLOG_ROLE_CLASS_NAME, script_file.is_empty() ? p_object->get_class() : script_file, p_ctx.depth);
		return;
	}

	emit(RDLOG_ROLE_OBJECT, Variant(p_object).stringify(), p_ctx.depth);
}

/* RDLogFormatter: dispatch */

void RDLogFormatter::format(const Variant &p_value, const RDLogFormatContext &p_ctx) {
	_format(p_value, p_ctx, true);
}

void RDLogFormatter::_format(const Variant &p_value, const RDLogFormatContext &p_ctx, bool p_allow_override) {
	const Variant::Type type = p_value.get_type();

	// Objects resolve their overrides by class in _format_object().
	if (p_allow_override && type != Variant::OBJECT && !overrides.is_empty()) {
		if (type == Variant::STRUCT) {
			const Struct value = p_value;
			if (!value.is_null() && _try_override(p_value, value.get_type_id(), p_ctx)) {
				return;
			}
		}
		if (_try_override(p_value, Variant::get_type_name(type), p_ctx)) {
			return;
		}
	}

	switch (type) {
		case Variant::NIL:
			emit(RDLOG_ROLE_NIL, "<null>", p_ctx.depth);
			break;
		case Variant::BOOL:
			emit(RDLOG_ROLE_BOOL, p_value.stringify(), p_ctx.depth);
			break;
		case Variant::INT:
			emit(RDLOG_ROLE_INT, p_value.stringify(), p_ctx.depth);
			break;
		case Variant::FLOAT:
			emit(RDLOG_ROLE_FLOAT, p_value.stringify(), p_ctx.depth);
			break;
		case Variant::STRING: {
			const String text = p_value;
			emit(RDLOG_ROLE_STRING, text.is_empty() ? String("\"\"") : text, p_ctx.depth);
		} break;
		case Variant::STRING_NAME:
			emit(RDLOG_ROLE_AMPERSAND, "&", p_ctx.depth);
			emit(RDLOG_ROLE_STRING_NAME, "\"" + p_value.stringify() + "\"", p_ctx.depth);
			break;
		case Variant::NODE_PATH:
			emit(RDLOG_ROLE_CARET, "^", p_ctx.depth);
			emit(RDLOG_ROLE_NODE_PATH, "\"" + p_value.stringify() + "\"", p_ctx.depth);
			break;
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
			_format_vector(p_value, 2, p_ctx);
			break;
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
			_format_vector(p_value, 3, p_ctx);
			break;
		case Variant::VECTOR4:
		case Variant::VECTOR4I:
			_format_vector(p_value, 4, p_ctx);
			break;
		case Variant::RECT2:
		case Variant::RECT2I:
		case Variant::TRANSFORM2D:
		case Variant::PLANE:
		case Variant::QUATERNION:
		case Variant::AABB:
		case Variant::BASIS:
		case Variant::TRANSFORM3D:
		case Variant::PROJECTION:
			emit(RDLOG_ROLE_RECT, p_value.stringify(), p_ctx.depth);
			break;
		case Variant::COLOR:
			emit(RDLOG_ROLE_COLOR, Color(p_value).to_html(false), p_ctx.depth);
			break;
		case Variant::RID:
			emit(RDLOG_ROLE_RID, p_value.stringify(), p_ctx.depth);
			break;
		case Variant::OBJECT:
			_format_object(p_value, p_ctx);
			break;
		case Variant::CALLABLE:
			emit(RDLOG_ROLE_CALLABLE, p_value.stringify(), p_ctx.depth);
			break;
		case Variant::SIGNAL:
			emit(RDLOG_ROLE_SIGNAL, p_value.stringify(), p_ctx.depth);
			break;
		case Variant::DICTIONARY:
			_format_dictionary(p_value, p_ctx);
			break;
		case Variant::ARRAY:
			_format_array(p_value, p_ctx);
			break;
		case Variant::PACKED_BYTE_ARRAY:
			_format_packed(PackedByteArray(p_value), p_ctx);
			break;
		case Variant::PACKED_INT32_ARRAY:
			_format_packed(PackedInt32Array(p_value), p_ctx);
			break;
		case Variant::PACKED_INT64_ARRAY:
			_format_packed(PackedInt64Array(p_value), p_ctx);
			break;
		case Variant::PACKED_FLOAT32_ARRAY:
			_format_packed(PackedFloat32Array(p_value), p_ctx);
			break;
		case Variant::PACKED_FLOAT64_ARRAY:
			_format_packed(PackedFloat64Array(p_value), p_ctx);
			break;
		case Variant::PACKED_STRING_ARRAY:
			_format_packed(PackedStringArray(p_value), p_ctx);
			break;
		case Variant::PACKED_VECTOR2_ARRAY:
			_format_packed(PackedVector2Array(p_value), p_ctx);
			break;
		case Variant::PACKED_VECTOR3_ARRAY:
			_format_packed(PackedVector3Array(p_value), p_ctx);
			break;
		case Variant::PACKED_COLOR_ARRAY:
			_format_packed(PackedColorArray(p_value), p_ctx);
			break;
		case Variant::PACKED_VECTOR4_ARRAY:
			_format_packed(PackedVector4Array(p_value), p_ctx);
			break;
		case Variant::STRUCT:
			_format_struct(p_value, p_ctx);
			break;
		case Variant::VARIANT_MAX:
			// Not a real type; listed so -Wswitch flags any new Variant type here.
			break;
	}
}

/* Rendering */

// Tags understood by RichTextLabel or by print_rich's terminal converter.
static const char *bbcode_tags[] = {
	"b", "i", "u", "s", "code", "indent", "url", "hint", "color", "bgcolor", "fgcolor",
	"center", "left", "right", "fill", "p", "br", "hr", "font", "font_size", "opentype_features",
	"otf", "lang", "outline_size", "outline_color", "img", "table", "cell", "ul", "ol", "dropcap",
	"wave", "shake", "tornado", "rainbow", "pulse", "fade", "char", "lb", "rb",
	"lrm", "rlm", "lre", "rle", "lro", "rlo", "pdf", "alm", "lri", "rli", "fsi", "pdi", "zwj", "zwnj", "wj", "shy",
};

static bool _is_bbcode_tag(const String &p_name) {
	for (const char *tag : bbcode_tags) {
		if (p_name == tag) {
			return true;
		}
	}
	return false;
}

String rdlog_escape_bbcode(const String &p_text) {
	if (p_text.find_char('[') < 0) {
		return p_text;
	}
	const int length = p_text.length();
	StringBuilder sb;
	int copied = 0;
	for (int i = 0; i < length; i++) {
		if (p_text[i] != '[') {
			continue;
		}
		int start = i + 1;
		if (start < length && p_text[start] == '/') {
			start++;
		}
		int end = start;
		while (end < length && (is_ascii_alphanumeric_char(p_text[end]) || p_text[end] == '_')) {
			end++;
		}
		if (end == start || end >= length) {
			continue;
		}
		const char32_t after = p_text[end];
		if ((after == ']' || after == '=' || after == ' ') && _is_bbcode_tag(p_text.substr(start, end - start))) {
			sb.append(p_text.substr(copied, i - copied));
			sb.append("[lb]");
			copied = i + 1;
		}
	}
	if (copied == 0) {
		return p_text;
	}
	sb.append(p_text.substr(copied));
	return sb.as_string();
}

String rdlog_render(const RDLogTokenStream &p_tokens, const RDLogPalette *p_palette) {
	StringBuilder sb;
	if (!p_palette) {
		for (const RDLogToken &token : p_tokens) {
			sb.append(token.text);
		}
		return sb.as_string();
	}

	const Vector<String> &backgrounds = p_palette->backgrounds;
	String current_fg;
	String current_bg;
	bool open = false;

	auto close_tags = [&]() {
		if (!current_fg.is_empty()) {
			sb.append("[/color]");
		}
		if (!current_bg.is_empty()) {
			sb.append("[/bgcolor]");
		}
		open = false;
	};

	for (const RDLogToken &token : p_tokens) {
		const String bg = backgrounds.is_empty() ? String() : backgrounds[token.depth % backgrounds.size()];
		// Whitespace looks the same in any colour, so it keeps the current one
		// instead of closing and reopening tags.
		const String fg = token.role == RDLOG_ROLE_NONE ? current_fg : p_palette->get(token.role, token.depth);

		if (!open || fg != current_fg || bg != current_bg) {
			if (open) {
				close_tags();
			}
			current_fg = fg;
			current_bg = bg;
			if (!current_bg.is_empty()) {
				sb.append("[bgcolor=" + current_bg + "]");
			}
			if (!current_fg.is_empty()) {
				sb.append("[color=" + current_fg + "]");
			}
			open = true;
		}
		sb.append(token.role == RDLOG_ROLE_NONE ? token.text : rdlog_escape_bbcode(token.text));
	}
	if (open) {
		close_tags();
	}
	return sb.as_string();
}
