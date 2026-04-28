#include "Common/Utilities.hpp"
#include "Features/Misc/Misc.hpp"

namespace {
	

	void RemovePerkConditions() {

		if (RE::TESDataHandler* tes = RE::TESDataHandler::GetSingleton()) {
			for (RE::TESForm* form : tes->GetFormArray(RE::FormType::Perk)) {
				if (RE::BGSPerk* perk = skyrim_cast<RE::BGSPerk*>(form)) {
					perk->perkConditions.head = nullptr;
				}
			}
		}
	}

	void SetNodeViSible(RE::NiAVObject* node) {

		if (!node->name.empty()) {
			if (node->name.contains("Ovl")) {
				node->GetFlags().set(RE::NiAVObject::Flag::kIgnoreFade);

				if (RE::BSGeometry* geom = node->AsGeometry()) {
					RE::NiPointer<RE::NiProperty> effect = geom->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect];
					RE::BSLightingShaderProperty* lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect.get());
					if (lightingShader) {
						lightingShader->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kNoFade);
					}
				}
			}
		}
	}

	void SetNodeVisibleRecursive(RE::NiAVObject* a_object) {
		if (!a_object) {
			return;
		}

		SetNodeViSible(a_object);

		const auto node = a_object->AsNode();
		if (!node) {
			return;
		}

		for (auto& child : node->GetChildren()) {
			SetNodeVisibleRecursive(child.get());
		}
	}

	inline void FlashUntilFocused(HWND hwnd) {
		FLASHWINFO info{};
		info.cbSize = sizeof(info);
		info.hwnd = hwnd;
		info.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
		info.uCount = 0;
		info.dwTimeout = 0;
		FlashWindowEx(&info);
	}
}


namespace BU::Features {

	/*void Misc::OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {

	}

	void Misc::OnSerdeSave(SKSE::SerializationInterface* a_this) {

	}*/

	void Misc::OnSKSEDataLoaded() {
		RemovePerkConditions();

		/*
		for (RE::TESForm* form : tes->GetFormArray(RE::FormType::Climate)) {
			if (RE::TESClimate* climate = skyrim_cast<RE::TESClimate*>(form)) {
				climate->timing.sunrise.begin = 0;
				climate->timing.sunrise.end = 255;
				climate->timing.sunset.begin = 0;
				climate->timing.sunset.end = 255;
			}
		}
		*/

	}

	void Misc::OnUpdate() {
		//Works
		if (RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton()) {
			if (player->Is3DLoaded()) {
				SetNodeVisibleRecursive(player->GetCurrent3D());
			}
		}
	}

	void Misc::OnActorSet3D(RE::Actor* a_actor, RE::NiAVObject* a_object) {
		//Doesn't work, maybe too early?
		/*if (a_actor) {
			SetNodeVisibleRecursive(a_object);
		}*/
	}

	void Misc::OnMenuChange(const RE::MenuOpenCloseEvent* a_event) {
		if (a_event) {
			if (a_event->opening && a_event->menuName == RE::MainMenu::MENU_NAME) {
				FlashUntilFocused(reinterpret_cast<HWND>(RE::BSGraphics::Renderer::GetCurrentRenderWindow()->hWnd));
			}
		}
	}

	/*void Misc::Draw() {
		
	}*/
}
