#include "pch.h"
#include "plugin.h"

BAKKESMOD_PLUGIN(LLMChatPlugin, "LLM chat responder for Rocket League", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void LLMChatPlugin::onLoad()
{
	_globalCvarManager = cvarManager;
}

void LLMChatPlugin::RenderSettings()
{
	ImGui::TextUnformatted("LLM Chat Plugin settings will appear here.");
}

void LLMChatPlugin::RenderWindow()
{
	ImGui::TextUnformatted("LLM Chat Plugin window.");
}
