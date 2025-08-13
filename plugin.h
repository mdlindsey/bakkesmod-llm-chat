#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class LLMChatPlugin : public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase, public PluginWindowBase
{
	// Configuration state
	std::shared_ptr<bool> enabled;

	// CVar wrappers
	CVarWrapper cvar_api_key;
	CVarWrapper cvar_system_prompt;
	CVarWrapper cvar_model;
	CVarWrapper cvar_history_len;
	CVarWrapper cvar_max_chars;

public:
	// Boilerplate
	void onLoad() override;

	// UI
	void RenderSettings() override;
	void RenderWindow() override;
};
