/**************************************************************************/
/*  rdlog_editor_plugin.cpp                                               */
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
 * @file rdlog_editor_plugin.cpp
 *
 * Editor toolbar selector for RDLog's minimum log level.
 */

#include "rdlog_editor_plugin.h"

#include "../rdlog.h"
#include "../rdlog_config.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "editor/gui/editor_toaster.h"
#include "scene/gui/option_button.h"
#include "scene/scene_string_names.h"

void RDLogEditorPlugin::_update_icons() {
	static const char *icons[] = { "Debug", "NodeInfo", "NodeWarning", "StatusError" };
	for (int i = 0; i < level_selector->get_item_count(); i++) {
		level_selector->set_item_icon(i, level_selector->get_editor_theme_icon(icons[i]));
	}
}

void RDLogEditorPlugin::_sync_from_settings() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	level_selector->set_visible(ps->get_setting(RDLogConfig::SETTING_SHOW_LOG_LEVEL_SELECTOR, false));
	// select() does not emit item_selected, so this cannot loop back into a save.
	level_selector->select(CLAMP(int(ps->get_setting(RDLogConfig::SETTING_LEVEL, RDLogConfig::DEFAULT_LEVEL)), 0, RDLog::LEVEL_ERROR));
}

void RDLogEditorPlugin::_level_selected(int p_index) {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	ps->set_setting(RDLogConfig::SETTING_LEVEL, p_index);
	ps->save();
	// Reload rather than RDLog::set_log_level(), which would pin the level as a
	// runtime override and stop later Project Settings edits from applying.
	if (RDLog::get_singleton()) {
		RDLog::get_singleton()->rebuild_config();
	}
}

void RDLogEditorPlugin::_notify_if_addon_installed() {
	if (FileAccess::exists("res://addons/log/log.gd")) {
		EditorToaster::get_singleton()->popup_str(
				TTR("The Log.gd addon is installed. RDLog provides the same logger natively: replace Log. with RDLog. in your scripts, then remove res://addons/log/."),
				EditorToaster::SEVERITY_INFO);
	}
}

void RDLogEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			ProjectSettings::get_singleton()->connect(SNAME("settings_changed"), callable_mp(this, &RDLogEditorPlugin::_sync_from_settings));
			_sync_from_settings();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			ProjectSettings::get_singleton()->disconnect(SNAME("settings_changed"), callable_mp(this, &RDLogEditorPlugin::_sync_from_settings));
		} break;
		case NOTIFICATION_READY: {
			callable_mp(this, &RDLogEditorPlugin::_notify_if_addon_installed).call_deferred();
		} break;
	}
}

RDLogEditorPlugin::RDLogEditorPlugin() {
	level_selector = memnew(OptionButton);
	level_selector->set_flat(true);
	level_selector->set_tooltip_text(TTR("Minimum level printed by RDLog.debug(), info() and warn()."));
	level_selector->add_item("DEBUG", RDLog::LEVEL_DEBUG);
	level_selector->add_item("INFO", RDLog::LEVEL_INFO);
	level_selector->add_item("WARN", RDLog::LEVEL_WARN);
	level_selector->add_item("ERROR", RDLog::LEVEL_ERROR);
	level_selector->set_visible(false);
	level_selector->connect(SceneStringName(item_selected), callable_mp(this, &RDLogEditorPlugin::_level_selected));
	level_selector->connect(SceneStringName(theme_changed), callable_mp(this, &RDLogEditorPlugin::_update_icons));
	add_control_to_container(CONTAINER_TOOLBAR, level_selector);
}
