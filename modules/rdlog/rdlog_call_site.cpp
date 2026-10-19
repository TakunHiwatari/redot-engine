/**************************************************************************/
/*  rdlog_call_site.cpp                                                   */
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
 * @file rdlog_call_site.cpp
 *
 * Script call-site capture for RDLog prefixes and error locations.
 */

#include "rdlog_call_site.h"

#include "core/object/script_language.h"
#include "core/variant/dictionary.h"

RDLogCallSite RDLogCallSite::capture() {
	RDLogCallSite site;
#ifdef DEBUG_ENABLED
	// RDLog's own methods are native, so frame 0 of the first script language with a
	// non-empty stack is the script line that called it.
	const int count = ScriptServer::get_language_count();
	for (int i = 0; i < count; i++) {
		ScriptLanguage *language = ScriptServer::get_language(i);
		if (!language) {
			continue;
		}
		const Vector<ScriptLanguage::StackInfo> stack = language->debug_get_current_stack_info();
		if (stack.is_empty()) {
			continue;
		}
		site.file = stack[0].file;
		site.function = stack[0].func;
		site.line = stack[0].line;
		site.valid = !site.file.is_empty();
		break;
	}
#endif
	return site;
}

RDLogCallSite RDLogCallSite::from_stack(const Array &p_stack) {
	RDLogCallSite site;
	if (p_stack.size() < 2 || p_stack[1].get_type() != Variant::DICTIONARY) {
		return site;
	}
	const Dictionary frame = p_stack[1];
	site.file = frame.get("source", String()).operator String();
	site.function = frame.get("function", String()).operator String();
	site.line = frame.get("line", 0);
	site.valid = !site.file.is_empty();
	return site;
}

String RDLogCallSite::get_prefix(RDLogRole &r_role) const {
	const String location = file.get_file().get_basename() + ":" + itos(line);
	if (file.match("*/test/*")) {
		r_role = RDLOG_ROLE_PREFIX_TEST;
		return "{" + location + "}: ";
	}
	if (file.match("*/addons/*")) {
		r_role = RDLOG_ROLE_PREFIX_ADDONS;
		return "<" + location + ">: ";
	}
	r_role = RDLOG_ROLE_PREFIX_SRC;
	return "[" + location + "]: ";
}
