#include "Util/Logger/Logger.hpp"
#include "Util/Events/EventDispatcher.hpp"
#include "Hooks/Hooks.hpp"
#include "Version.hpp"

#include "Common/UI/UIRegistry.hpp"
#include "Common/SKEE/SKEE.hpp"

#include "Features/ArmorFactor/ArmorFactor.hpp"
#include "Features/ManaTanks/Manatanks.hpp"
#include "Features/Misc/Misc.hpp"
#include "Features/NPCUtils/NPCUtils.hpp"
#include "Features/QuestUtils//QuestUtils.hpp"
#include "Features/OverlayUtils/OverlayUtils.hpp"


SKSEPluginLoad(const SKSE::LoadInterface * a_SKSE) {

	SKSE::Init(a_SKSE);
	logger::Initialize();
	SKSE::GetTrampoline().create(128);
	Hooks::Install();

	BU::EventDispatcher::AddListener(&BU::UI::UIItemRegistry::GetSingleton());
	BU::EventDispatcher::AddListener(&BU::SKEE::Overlays::GetSingleton());
	BU::EventDispatcher::AddListener(&BU::SKEE::Morphs::GetSingleton());

	BU::EventDispatcher::AddListener(&BU::Features::ManaTanks::GetSingleton());
	BU::EventDispatcher::AddListener(&BU::Features::ArmorFactor::GetSingleton());
	BU::EventDispatcher::AddListener(&BU::Features::OverlayTools::GetSingleton());
	BU::EventDispatcher::AddListener(&BU::Features::Misc::GetSingleton());
	BU::EventDispatcher::AddListener(&BU::Features::QuestUtils::GetSingleton());
	BU::EventDispatcher::AddListener(&BU::Features::NPCUtils::GetSingleton());

	BU::EventDispatcher::Init(_byteswap_ulong('BIUT'));

	logger::info("SKSEPluginLoad OK");

	return true;
}

SKSEPluginInfo(
	.Version = Plugin::ModVersion,
	.Name = Plugin::ModName,
	.Author = "BingusEx",
	.StructCompatibility = SKSE::StructCompatibility::Independent,
	.RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary
);