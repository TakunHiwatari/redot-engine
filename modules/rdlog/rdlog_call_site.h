/**************************************************************************/
/*  rdlog_call_site.h                                                     */
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
 * @file rdlog_call_site.h
 *
 * Finds the script file and line that called into RDLog, and formats the
 * `[file:line]: ` prefix.
 */

#include "rdlog_color_theme.h"

#include "core/string/ustring.h"
#include "core/variant/array.h"

struct RDLogCallSite {
	String file;
	String function;
	int line = 0;
	bool valid = false;

	/// The most recent script frame on the calling thread. Script languages track
	/// their call stack in editor and debug builds, and in release builds only when
	/// debug/settings/gdscript/always_track_call_stacks is enabled; otherwise this
	/// returns an invalid call site.
	static RDLogCallSite capture();

	/// Reads frame 1 of an Array in GDScript `get_stack()` format, as passed to
	/// `RDLog.to_printable(msgs, {stack = get_stack()})`.
	static RDLogCallSite from_stack(const Array &p_stack);

	/// `[file:line]: ` for project code, `<file:line>: ` under `addons/`,
	/// `{file:line}: ` under `test/`.
	String get_prefix(RDLogRole &r_role) const;
};
