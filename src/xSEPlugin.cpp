#include "Util/Logger/Logger.hpp"
#include "Version.hpp"

namespace {

	void InitializeMessaging() {

		if (!SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_Message) {

			switch (a_Message->type) {

				// Called after all plugins have finished running SKSEPluginLoad.
				case SKSE::MessagingInterface::kPostLoad:{
					logger::debug("kPostLoad");
					break;
				}

				// Called after all kPostLoad message handlers have run.
				case SKSE::MessagingInterface::kPostPostLoad:{
					logger::debug("kPostPostLoad");
					break;
				}

				// Called when all game data has been found.
				case SKSE::MessagingInterface::kInputLoaded:{
					logger::debug("kInputLoaded");
					break;
				}

				// All ESM/ESL/ESP plugins have loaded, main menu is now active.
				case SKSE::MessagingInterface::kDataLoaded:{
					logger::debug("kDataLoaded");
					break;
				}

				// Player's selected save game has finished loading.
				case SKSE::MessagingInterface::kPostLoadGame:{
					logger::debug("kPostLoadGame");
					break;
				}

				// Player starts a new game from main menu.
				case SKSE::MessagingInterface::kNewGame:{
					logger::debug("kNewGame");
					break;
				}

				// Player selected a game to load, but it hasn't loaded yet, data will be the name of the loaded save.
				case SKSE::MessagingInterface::kPreLoadGame:{
					logger::debug("kPreLoadGame");
					break;
				}

				// The player has saved a game.
				case SKSE::MessagingInterface::kSaveGame:{
					logger::debug("kSaveGame");
					break;
				}

				// The player deleted a saved game from within the load menu, data will be the save name.
				case SKSE::MessagingInterface::kDeleteGame:{
					logger::debug("kDeleteGame");
					break;
				}

				default:{}
			}
			})) {
			Util::Win32::ReportAndExit("Unable to register message listener.");
		}
	}

	void WaitForDebugger() {
		#ifdef DEBUG
			Util::Win32::ReportInfo("Attach Debugger And Press OK.");
		#endif
	}
}

SKSEPluginLoad(const SKSE::LoadInterface * a_SKSE) {

	WaitForDebugger();

	SKSE::Init(a_SKSE);

	logger::Initialize();
	InitializeMessaging();

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