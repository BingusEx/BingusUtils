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
			if (node->name.contains("[Ovl")) {
				node->GetFlags().set(RE::NiAVObject::Flag::kAlwaysDraw);
				node->GetFlags().set(RE::NiAVObject::Flag::kIgnoreFade);
				node->GetFlags().set(RE::NiAVObject::Flag::kHighDetail);

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
}


namespace BU::Features {

	void Misc::OnSerdeSave(SKSE::SerializationInterface* a_this) {}

	void Misc::OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {}

	void Misc::OnActorReset(RE::Actor* a_actor) {}

	void Misc::OnActorUpdate(RE::Actor* a_actor) {}

	void Misc::OnSerdePostLoad() {}

	void Misc::OnSKSEDataLoaded() {
		RemovePerkConditions();
	}

	void Misc::OnActorLoad3D(RE::Actor* a_actor) {
		if (a_actor) {
			if (a_actor->Is3DLoaded()) {
				//SKSE::GetTaskInterface()->AddTask([&] {
					RE::BSVisit::TraverseScenegraphObjects(a_actor->Get3D(), [&](RE::NiAVObject* a_object) {
						SetNodeViSible(a_object);
						return RE::BSVisit::BSVisitControl::kContinue;
					});
				//});
			}
		}
	}
}
