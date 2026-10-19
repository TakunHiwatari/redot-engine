/**************************************************************************/
/*  test_rdlog.h                                                          */
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
 * @file test_rdlog.h
 *
 * Unit tests for the RDLog module: formatting rules, rendering, escaping,
 * structs, objects, call-site prefixes and runtime configuration.
 */

#include "../rdlog.h"
#include "../rdlog_call_site.h"
#include "../rdlog_color_theme.h"
#include "../rdlog_config.h"
#include "../rdlog_formatter.h"

#include "core/variant/struct.h"
#include "core/variant/struct_info.h"
#include "scene/2d/node_2d.h"
#include "scene/main/node.h"

#include "tests/test_macros.h"

namespace TestRDLog {

static String format_plain(const Variant &p_value, const RDLogConfig &p_config = RDLogConfig(), const RDLogFormatContext &p_ctx = RDLogFormatContext(), const HashMap<String, Callable> &p_overrides = HashMap<String, Callable>()) {
	RDLogTokenStream tokens;
	RDLogFormatter formatter(p_config, p_overrides, tokens);
	formatter.format(p_value, p_ctx);
	return rdlog_render(tokens, nullptr);
}

static String format_rich(const Variant &p_value, const RDLogPalette &p_palette) {
	const RDLogConfig config;
	const HashMap<String, Callable> overrides;
	RDLogTokenStream tokens;
	RDLogFormatter formatter(config, overrides, tokens);
	formatter.format(p_value, RDLogFormatContext());
	return rdlog_render(tokens, &p_palette);
}

static Ref<StructInfo> make_point_info() {
	StructInfoBuilder builder;
	builder.set_logical_type_id("Point");
	StructInfo::Field x;
	x.name = "x";
	x.type = Variant::INT;
	x.is_typed = true;
	x.default_value = 1;
	builder.add_field(x);
	StructInfo::Field y;
	y.name = "y";
	y.type = Variant::INT;
	y.is_typed = true;
	y.default_value = 2;
	builder.add_field(y);
	return builder.build();
}

static Variant point_override(const Variant &p_value) {
	return "P!";
}

static Variant node_override(const Variant &p_value) {
	return "a node";
}

static Variant failing_to_string(const Variant &p_value) {
	return Dictionary();
}

TEST_CASE("[RDLog] Config defaults") {
	const RDLogConfig config;
	CHECK(config.level == 1);
	CHECK(config.disable_colors == false);
	CHECK(config.force_termsafe_colors == false);
	CHECK(config.use_newlines == false);
	CHECK(config.newline_max_depth == -1);
	CHECK(config.warn_todo == true);
	CHECK(config.show_timestamps == false);
	CHECK(config.timestamp_type == 3);
	CHECK(config.timestamp_format == "{hour}:{minute}:{second}");
	CHECK(config.max_array_size == 20);
	CHECK(config.max_dictionary_size == 20);
	CHECK(config.max_depth == 32);
	CHECK(config.dictionary_skip_keys.size() == 1);
	CHECK(config.dictionary_skip_keys[0] == "layer_0/tile_data");
	CHECK(config.color_theme_path.is_empty());
	CHECK(config.overridden == 0);
}

TEST_CASE("[RDLog] Scalars") {
	CHECK(format_plain(1) == "1");
	CHECK(format_plain(1.5) == "1.5");
	CHECK(format_plain(true) == "true");
	CHECK(format_plain(Variant()) == "<null>");
	CHECK(format_plain(String()) == "\"\"");
	CHECK(format_plain("abc") == "abc");
	CHECK(format_plain(StringName("sn")) == "&\"sn\"");
	CHECK(format_plain(NodePath("np")) == "^\"np\"");
	CHECK(format_plain(Color(1, 0, 0)) == "ff0000");
}

TEST_CASE("[RDLog] Vectors") {
	CHECK(format_plain(Vector2(1, 2)) == "(1.0,2.0)");
	CHECK(format_plain(Vector3i(1, 2, 3)) == "(1,2,3)");
	CHECK(format_plain(Vector4(1, 2, 3, 4)) == "(1.0,2.0,3.0,4.0)");
}

TEST_CASE("[RDLog] Arrays") {
	CHECK(format_plain(Array()) == "[]");
	CHECK(format_plain(Array({ 1, 2, 3 })) == "[ 1, 2, 3 ]");
	CHECK(format_plain(Array({ "a", Array({ 1 }) })) == "[ a, [ 1 ] ]");
}

TEST_CASE("[RDLog] Array truncation keeps max_array_size elements") {
	Array big;
	for (int i = 0; i < 30; i++) {
		big.push_back(i);
	}
	const String out = format_plain(big);
	CHECK(out.begins_with("[ 0, 1, 2,"));
	CHECK(out.contains(", 19, ... (10 more) ]"));
	CHECK_FALSE(out.contains("20"));
}

TEST_CASE("[RDLog] Dictionary separators do not depend on key values") {
	Dictionary numbers;
	numbers[0] = "a";
	numbers[1] = "b";
	CHECK(format_plain(numbers) == "{ \"0\": a, \"1\": b }");

	Dictionary falsy_last;
	falsy_last["x"] = 2;
	falsy_last[""] = 1;
	CHECK(format_plain(falsy_last) == "{ \"x\": 2, \"\": 1 }");

	CHECK(format_plain(Dictionary()) == "{}");
}

TEST_CASE("[RDLog] Dictionary skip keys and truncation") {
	Dictionary tiles;
	tiles["layer_0/tile_data"] = PackedInt32Array({ 1, 2, 3 });
	CHECK(format_plain(tiles) == "{ \"layer_0/tile_data\": ... }");

	Dictionary big;
	for (int i = 0; i < 30; i++) {
		big[i] = i;
	}
	CHECK(format_plain(big).contains("... (10 more) }"));
}

TEST_CASE("[RDLog] Packed arrays are walked") {
	CHECK(format_plain(PackedInt32Array({ 1, 2 })) == "[ 1, 2 ]");
	CHECK(format_plain(PackedStringArray({ "a", "b" })) == "[ a, b ]");
	CHECK(format_plain(PackedVector2Array({ Vector2(1, 2) })) == "[ (1.0,2.0) ]");
	CHECK(format_plain(PackedByteArray()) == "[]");
}

TEST_CASE("[RDLog] Self-referencing containers terminate") {
	Array array;
	array.push_back(1);
	array.push_back(array);
	CHECK(format_plain(array) == "[ 1, [...] ]");
	array.clear(); // Break the reference cycle.

	Dictionary dict;
	dict["self"] = dict;
	CHECK(format_plain(dict) == "{ \"self\": {...} }");
	dict.clear();
}

TEST_CASE("[RDLog] Depth limit") {
	Array root;
	Array current = root;
	for (int i = 0; i < 100; i++) {
		Array next;
		current.push_back(next);
		current = next;
	}
	RDLogConfig config;
	config.max_depth = 8;
	const String out = format_plain(root, config);
	CHECK(out.contains("[...]"));
	CHECK(out.count("[ ") == 8);
}

TEST_CASE("[RDLog] Newline layout") {
	Dictionary dict;
	dict["a"] = 1;
	dict["b"] = Array({ 1, 2 });
	RDLogConfig config;
	const RDLogFormatContext ctx = RDLogFormatContext::make_expanded(config, -1);
	CHECK(format_plain(dict, config, ctx) == "{\n\t\"a\": 1,\n\t\"b\": [\n\t\t1,\n\t\t2\n\t]\n}");

	// prn: only the outermost container is expanded.
	const RDLogFormatContext prn = RDLogFormatContext::make_expanded(config, 1);
	CHECK(format_plain(dict, config, prn) == "{\n\t\"a\": 1,\n\t\"b\": [ 1, 2 ]\n}");
}

TEST_CASE("[RDLog] Context options") {
	RDLogConfig config;
	config.use_newlines = true;
	Dictionary options;
	options["newline_max_depth"] = 0;
	const RDLogFormatContext ctx = RDLogFormatContext::make(config, options);
	CHECK_FALSE(ctx.newlines);
}

TEST_CASE("[RDLog] Every Variant type formats to something") {
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		Variant value;
		Callable::CallError ce;
		Variant::construct(Variant::Type(i), value, nullptr, 0, ce);
		INFO(Variant::get_type_name(Variant::Type(i)));
		CHECK_FALSE(format_plain(value).is_empty());
	}
}

TEST_CASE("[RDLog] Colours and rainbow delimiters") {
	const RDLogPalette palette = RDLogColorTheme::create_dark()->build_palette();
	CHECK(format_rich(1, palette) == "[color=6495ed]1[/color]");

	const String nested = format_rich(Array({ Array({ 1 }) }), palette);
	CHECK(nested.begins_with("[color=dc143c][ [/color][color=6495ed][ "));
	CHECK(nested.ends_with("[color=dc143c]][/color]"));
}

TEST_CASE("[RDLog] Background colours alternate by depth") {
	Ref<RDLogColorTheme> theme = RDLogColorTheme::create_dark();
	theme->set_bg_colors({ Color(0, 0, 0), Color(1, 1, 1) });
	const String out = format_rich(Array({ 1 }), theme->build_palette());
	CHECK(out.contains("[bgcolor=000000]"));
	CHECK(out.contains("[bgcolor=ffffff]"));
}

TEST_CASE("[RDLog] Transparent theme colours are left uncoloured") {
	Ref<RDLogColorTheme> theme = RDLogColorTheme::create_dark();
	theme->set_theme_color(RDLogColorTheme::THEME_COLOR_INT, Color(0, 0, 0, 0));
	CHECK(format_rich(1, theme->build_palette()) == "1");
}

TEST_CASE("[RDLog] Termsafe palette uses named colours") {
	const RDLogPalette palette = RDLogPalette::termsafe();
	CHECK(format_rich(1, palette) == "[color=green]1[/color]");
	CHECK(palette.rainbow.size() == 5);
}

TEST_CASE("[RDLog] BBCode escaping") {
	CHECK(rdlog_escape_bbcode("[b]x[/b]") == "[lb]b]x[lb]/b]");
	CHECK(rdlog_escape_bbcode("[color=red]hi") == "[lb]color=red]hi");
	CHECK(rdlog_escape_bbcode("[url]") == "[lb]url]");
	CHECK(rdlog_escape_bbcode("[1, 2]") == "[1, 2]");
	CHECK(rdlog_escape_bbcode("[DEBUG]") == "[DEBUG]");
	CHECK(rdlog_escape_bbcode("[file:3]: ") == "[file:3]: ");
	CHECK(rdlog_escape_bbcode("trailing [") == "trailing [");

	const RDLogPalette palette = RDLogColorTheme::create_dark()->build_palette();
	CHECK(format_rich("[b]bold?[/b]", palette) == "[color=a9a9a9][lb]b]bold?[lb]/b][/color]");
	CHECK(format_plain("[b]bold?[/b]") == "[b]bold?[/b]");
}

TEST_CASE("[RDLog] Structs print their fields") {
	const Ref<StructInfo> info = make_point_info();
	REQUIRE(info.is_valid());
	const Struct point(info);
	CHECK(format_plain(point) == "Point { x: 1, y: 2 }");

	RDLogConfig typed;
	typed.print_struct_types = true;
	CHECK(format_plain(point, typed) == "Point { x: int = 1, y: int = 2 }");

	CHECK(format_plain(Struct()) == "Struct(null)");
}

TEST_CASE("[RDLog] Nullable and nested struct fields") {
	const Ref<StructInfo> point_info = make_point_info();

	StructInfoBuilder builder;
	builder.set_logical_type_id("Label");
	StructInfo::Field text;
	text.name = "text";
	text.type = Variant::STRING;
	text.is_typed = true;
	text.is_nullable = true;
	builder.add_field(text);
	StructInfo::Field at;
	at.name = "at";
	at.type = Variant::STRUCT;
	at.is_typed = true;
	at.struct_type_id = "Point";
	at.default_value = Struct(point_info);
	builder.add_field(at);
	const Ref<StructInfo> info = builder.build();
	REQUIRE(info.is_valid());

	const Struct label(info);
	CHECK(format_plain(label) == "Label { text: <null>, at: Point { x: 1, y: 2 } }");

	RDLogConfig typed;
	typed.print_struct_types = true;
	CHECK(format_plain(label, typed) == "Label { text: String? = <null>, at: Point = Point { x: int = 1, y: int = 2 } }");
}

TEST_CASE("[RDLog][SceneTree] Type overrides") {
	const Struct point(make_point_info());
	HashMap<String, Callable> overrides;
	overrides["Point"] = callable_mp_static(&point_override);
	CHECK(format_plain(point, RDLogConfig(), RDLogFormatContext(), overrides) == "P!");

	// An override that returns its own type is not applied to its own result.
	HashMap<String, Callable> same_type;
	same_type["Dictionary"] = callable_mp_static(&failing_to_string);
	CHECK(format_plain(Dictionary(), RDLogConfig(), RDLogFormatContext(), same_type) == "{}");

	// Native class overrides apply to subclasses.
	HashMap<String, Callable> by_class;
	by_class["Node"] = callable_mp_static(&node_override);
	Node2D *node = memnew(Node2D);
	CHECK(format_plain(node, RDLogConfig(), RDLogFormatContext(), by_class) == "a node");
	memdelete(node);
}

TEST_CASE("[RDLog] Objects") {
	Node *node = memnew(Node);
	node->set_name("Player");
	CHECK(format_plain(node) == "Node:\"Player\"");
	memdelete(node);

	Object *object = memnew(Object);
	const Variant freed = object;
	memdelete(object);
	CHECK(format_plain(freed).begins_with("invalid instance:"));

	Ref<RefCounted> ref;
	ref.instantiate();
	CHECK(format_plain(ref) == "RefCounted");

	CHECK(format_plain(Variant((Object *)nullptr)) == "<null>");
}

TEST_CASE("[RDLog] Call-site prefixes") {
	RDLogCallSite site;
	site.line = 3;
	RDLogRole role = RDLOG_ROLE_NONE;

	site.file = "res://src/player.gd";
	CHECK(site.get_prefix(role) == "[player:3]: ");
	CHECK(role == RDLOG_ROLE_PREFIX_SRC);

	site.file = "res://addons/thing/helper.gd";
	CHECK(site.get_prefix(role) == "<helper:3>: ");
	CHECK(role == RDLOG_ROLE_PREFIX_ADDONS);

	site.file = "res://test/test_player.gd";
	CHECK(site.get_prefix(role) == "{test_player:3}: ");
	CHECK(role == RDLOG_ROLE_PREFIX_TEST);

	Dictionary frame0;
	frame0["source"] = "res://addons/log/log.gd";
	Dictionary frame1;
	frame1["source"] = "res://src/main.gd";
	frame1["line"] = 12;
	frame1["function"] = "_ready";
	const RDLogCallSite from_stack = RDLogCallSite::from_stack(Array({ frame0, frame1 }));
	CHECK(from_stack.valid);
	CHECK(from_stack.file == "res://src/main.gd");
	CHECK(from_stack.line == 12);
	CHECK_FALSE(RDLogCallSite::from_stack(Array()).valid);
}

TEST_CASE("[RDLog] Singleton runtime configuration") {
	RDLog *rdlog = RDLog::get_singleton();
	REQUIRE(rdlog != nullptr);

	rdlog->set_newline_max_depth(4);
	rdlog->enable_newlines();
	rdlog->reset_newline_max_depth();
	CHECK(rdlog->get_newline_max_depth() == -1);
	CHECK(rdlog->get_use_newlines()); // reset_newline_max_depth() must not touch use_newlines.

	rdlog->set_log_level(RDLog::LEVEL_DEBUG);
	rdlog->rebuild_config(); // Runtime overrides survive a reload from ProjectSettings.
	CHECK(rdlog->get_log_level() == RDLog::LEVEL_DEBUG);

	rdlog->disable_colors();
	CHECK(rdlog->to_pretty(Array({ 1 })) == "[ 1 ]");

	rdlog->clear_runtime_overrides();
	CHECK(rdlog->get_log_level() == RDLog::LEVEL_INFO);
	CHECK_FALSE(rdlog->get_use_newlines());
	CHECK_FALSE(rdlog->get_disable_colors());
}

TEST_CASE("[RDLog] Singleton type overrides accept typeof() keys") {
	RDLog *rdlog = RDLog::get_singleton();
	REQUIRE(rdlog != nullptr);

	Dictionary overrides;
	overrides[int(Variant::VECTOR2)] = callable_mp_static(&point_override);
	rdlog->register_type_overwrites(overrides);
	CHECK(rdlog->to_plain(Vector2(1, 2)) == "P!");
	rdlog->clear_type_overwrites();
	CHECK(rdlog->to_plain(Vector2(1, 2)) == "(1.0,2.0)");
}

} // namespace TestRDLog
