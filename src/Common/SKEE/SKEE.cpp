#include "SKEE.hpp"
#include "SKEEInterface.hpp"

namespace BU::SKEE {

	using namespace SKEEIntfc;

	//--------------------------------------------------------------------------------
	// Overlays
	//--------------------------------------------------------------------------------

	float Overlays::GetNodeOverrideFloat(RE::TESObjectREFR* a_ref, bool a_female, const char* a_node, int16_t a_key, uint8_t a_index) {
		Variant result;
		if (OverrideInterface->GetNodeOverride(a_ref, a_female, a_node, a_key, a_index, result)) {
			return result.Float();
		}
		return 0.f;
	}

	int32_t Overlays::GetNodeOverrideInt(RE::TESObjectREFR* a_ref, bool a_female, const char* a_node, int16_t a_key, uint8_t a_index) {
		Variant result;
		if (OverrideInterface->GetNodeOverride(a_ref, a_female, a_node, a_key, a_index, result)) {
			return result.Int();
		}
		return 0;
	}

	RE::BSFixedString Overlays::GetNodeOverrideString(RE::TESObjectREFR* a_ref, bool a_female, const char* a_node, int16_t a_key, uint8_t a_index) {
		Variant result;
		if (OverrideInterface->GetNodeOverride(a_ref, a_female, a_node, a_key, a_index, result)) {
			return result.String();
		}
		return "";
	}

	void Overlays::ClearBodyPart(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart) {

		if (!a_Actor) {
			logger::error("Invalid Actor");
			return;
		}

		for (int16_t i = 0; i < a_bodyPart.Count; i++) {
			RemoveSingle(a_Actor, a_bodyPart, i);
		}

	}

	void Overlays::RemoveSingle(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart, uint16_t a_idx) {

		if (!a_Actor) {
			logger::error("Invalid Actor");
			return;
		}

		const std::string NodeName = fmt::format(fmt::runtime(a_bodyPart.FmtName), a_idx);

		const bool Fem = static_cast<bool>(a_Actor->GetActorBase()->GetSex());
		OverrideInterface->RemoveAllNodeOverridesByNode(a_Actor, Fem, NodeName.c_str());

		Variant VariantStr("textures\\actors\\character\\overlays\\default.dds");
		OverrideInterface->RemoveNodeOverride(a_Actor, Fem, NodeName.c_str(), static_cast<uint16_t>(Layers::kShaderTexture), 0);
		OverrideInterface->AddNodeOverride(a_Actor, static_cast<bool>(a_Actor->GetActorBase()->GetSex()), NodeName.c_str(), static_cast<uint16_t>(Layers::kShaderTexture), 0, VariantStr);

		Variant VariantF(0.0f);
		OverrideInterface->RemoveNodeOverride(a_Actor, Fem, NodeName.c_str(), static_cast<uint16_t>(Layers::kShaderAlpha), -1);
		OverrideInterface->AddNodeOverride(a_Actor, static_cast<bool>(a_Actor->GetActorBase()->GetSex()), NodeName.c_str(), static_cast<uint16_t>(Layers::kShaderAlpha), -1, VariantF);

		Variant VariantI(0);
		OverrideInterface->RemoveNodeOverride(a_Actor, Fem, NodeName.c_str(), static_cast<uint16_t>(Layers::kShaderTintColor), -1);
		OverrideInterface->AddNodeOverride(a_Actor, static_cast<bool>(a_Actor->GetActorBase()->GetSex()), NodeName.c_str(), static_cast<uint16_t>(Layers::kShaderTintColor), -1, VariantI);

		logger::trace("Cleared bodypart: {} On Ovl {}", a_Actor->GetDisplayFullName(), NodeName);

	}

	void Overlays::ClearAll(RE::Actor* a_Actor) {

		if (!a_Actor) {
			logger::error("Invalid Actor");
			return;
		}

		ClearBodyPart(a_Actor, FaceInfo);
		ClearBodyPart(a_Actor, BodyInfo);
		ClearBodyPart(a_Actor, HandsInfo);
		ClearBodyPart(a_Actor, FeetInfo);

		OverlayInterface->RevertOverlays(a_Actor, true);
		OverlayInterface->EraseOverlays(a_Actor);
		OverlayInterface->RemoveOverlays(a_Actor);

		if (const auto& Actor3D = a_Actor->Get3D(false)) {
			OverrideInterface->SetNodeProperties(a_Actor, false);
			OverrideInterface->SetSkinProperties(a_Actor, false);
			OverrideInterface->ApplyNodeOverrides(a_Actor, Actor3D, false);
			a_Actor->Update3DModel();
			a_Actor->DoReset3D(true);
		}

		logger::trace("All overlays removed on: {}", a_Actor->GetDisplayFullName());

	}

	int16_t Overlays::GetNumOfUsedSlots(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart) {
		if (!a_Actor) {
			logger::error("Invalid Actor");
			return 0;
		}

		int16_t NumOfOccupiedSlots = 0;

		for (int16_t i = 0; i < a_bodyPart.Count; i++) {
			const bool Fem = static_cast<bool>(a_Actor->GetActorBase()->GetSex());
			const std::string NodeName = fmt::format(fmt::runtime(a_bodyPart.FmtName), i);
			const std::string NodeDataTextureStr = GetNodeOverrideString(a_Actor, Fem, NodeName.c_str(), static_cast<int16_t>(Layers::kShaderTexture), 0).c_str();

			const bool IsUnused = NodeDataTextureStr.empty() || 
				Util::Text::ContainsInvariantStr(NodeDataTextureStr, R"(\default.dds)") ||
				Util::Text::ContainsInvariantStr(NodeDataTextureStr, R"(\blank.dds)");

			if (IsUnused) {
				continue;
			}

			NumOfOccupiedSlots++;
		}
		return NumOfOccupiedSlots;
	}

	void Overlays::SetSingle(RE::Actor* a_Actor, const Overlay& a_overlay) {

		if (!a_Actor) {
			logger::error("Invalid Actor");
			return;
		}

		std::string targetFmt;

		//Check if overlay index is valid for the bodypart
		{
			using Loc = IOverlayInterface::OverlayLocation;

			int16_t count = -1;

			switch (static_cast<Loc>(a_overlay.BodyPart)) {
				case Loc::Face: { count = FaceInfo.Count;  targetFmt = FaceInfo.FmtName; break; }
				case Loc::Hand: { count = HandsInfo.Count; targetFmt = HandsInfo.FmtName; break; }
				case Loc::Body: { count = BodyInfo.Count;  targetFmt = BodyInfo.FmtName; break; }
				case Loc::Feet: { count = FeetInfo.Count;  targetFmt = FeetInfo.FmtName; break; }
			}

			if (a_overlay.Index > count) {
				logger::error("Invalid Idx");
				return;
			}
		}

		if (targetFmt.empty()) {
			logger::error("Invalid Target");
			return;
		}

		std::string NodeName = fmt::format(fmt::runtime(targetFmt), a_overlay.Index);
		const bool Fem = static_cast<bool>(a_Actor->GetActorBase()->GetSex());

		Variant VariantStr(a_overlay.TexturePath.c_str());
		Variant VariantF(a_overlay.DiffuseAlpha);
		Variant VariantI(static_cast<int32_t>(a_overlay.DiffuseColor));

		OverrideInterface->AddNodeOverride(a_Actor, Fem, NodeName.c_str(), static_cast<uint16_t>(Layers::kShaderTexture),    0, VariantStr);
		OverrideInterface->AddNodeOverride(a_Actor, Fem, NodeName.c_str(), static_cast<uint16_t>(Layers::kShaderAlpha),     -1, VariantF);
		OverrideInterface->AddNodeOverride(a_Actor, Fem, NodeName.c_str(), static_cast<uint16_t>(Layers::kShaderTintColor), -1, VariantI);

		logger::trace("Appied {} to {} on BodyPart {}", a_overlay.TexturePath, a_Actor->GetDisplayFullName(), NodeName);

	}

	std::list<uint16_t> Overlays::GetUsedSlotIndices(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart) {
		if (!a_Actor) {
			logger::error("Invalid Actor");
			return {};
		}

		std::list<uint16_t> OccupiedSlots = {};

		for (int16_t i = 0; i < a_bodyPart.Count; i++) {
			const bool Fem = static_cast<bool>(a_Actor->GetActorBase()->GetSex());
			const std::string NodeName = fmt::format(fmt::runtime(a_bodyPart.FmtName), i);
			const std::string NodeDataTextureStr = GetNodeOverrideString(a_Actor, Fem, NodeName.c_str(), static_cast<int16_t>(Layers::kShaderTexture), 0).c_str();

			const bool IsUnused = NodeDataTextureStr.empty() ||
				Util::Text::ContainsInvariantStr(NodeDataTextureStr, R"(\default.dds)") ||
				Util::Text::ContainsInvariantStr(NodeDataTextureStr, R"(\blank.dds)");

			if (!IsUnused) {
				OccupiedSlots.push_back(i);
			}

		}
		return OccupiedSlots;
	}


	bool Overlays::BuildOverlayAtIdx(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart, uint16_t a_idx, Overlay* a_outOverlayObject) {
		if (!a_Actor) {
			logger::error("Invalid Actor");
			return false;
		}

		if (!a_outOverlayObject) {
			logger::error("Invalid OverlayObject");
			return false;
		}

		if (a_idx > a_bodyPart.Count) {
			logger::error("Idx Out of range");
			return false;
		}

		bool Fem = static_cast<bool>(a_Actor->GetActorBase()->GetSex());
		const std::string NodeName = fmt::format(fmt::runtime(a_bodyPart.FmtName), a_idx);

		a_outOverlayObject->TexturePath = GetNodeOverrideString(a_Actor, Fem, NodeName.c_str(), static_cast<int16_t>(Layers::kShaderTexture), 0);
		a_outOverlayObject->DiffuseAlpha = GetNodeOverrideFloat(a_Actor, Fem, NodeName.c_str(), static_cast<int16_t>(Layers::kShaderAlpha), -1);
		a_outOverlayObject->DiffuseColor = GetNodeOverrideInt(a_Actor, Fem, NodeName.c_str(),   static_cast<int16_t>(Layers::kShaderTintColor), -1);
		a_outOverlayObject->Index = a_idx;
		a_outOverlayObject->BodyPart = static_cast<uint8_t>(a_bodyPart.loc);

		return true;
	}

	void Overlays::Register() {
		InterfaceExchangeMessage msg;

		const auto* const intfc{ SKSE::GetMessagingInterface() };

		intfc->Dispatch(InterfaceExchangeMessage::kMessage_ExchangeInterface, &msg, sizeof(InterfaceExchangeMessage*), "SKEE");

		if (!msg.interfaceMap) {
			logger::error("Couldn't Get SKSE interface map.");
			return;
		}

		OverlayInterface = static_cast<IOverlayInterface*>(msg.interfaceMap->QueryInterface("Overlay"));
		OverrideInterface = static_cast<IOverrideInterface*>(msg.interfaceMap->QueryInterface("OVerride"));

		if (!OverlayInterface) {
			logger::error("Couldn't get SKEE OverlayInterface.");
			return;
		}

		if (!OverrideInterface) {
			logger::error("Couldn't get SKEE OverrideInterface.");
			return;
		}

		logger::info("SKEE OverlayInterface Version {}", OverlayInterface->GetVersion());
		logger::info("SKEE OverrideInterface Version {}", OverrideInterface->GetVersion());


		//Get Slot Count
		FaceInfo.Count = static_cast<int16_t>(OverlayInterface->GetOverlayCount(IOverlayInterface::OverlayType::Normal, IOverlayInterface::OverlayLocation::Face));
		HandsInfo.Count = static_cast<int16_t>(OverlayInterface->GetOverlayCount(IOverlayInterface::OverlayType::Normal, IOverlayInterface::OverlayLocation::Hand));
		BodyInfo.Count = static_cast<int16_t>(OverlayInterface->GetOverlayCount(IOverlayInterface::OverlayType::Normal, IOverlayInterface::OverlayLocation::Body));
		FeetInfo.Count = static_cast<int16_t>(OverlayInterface->GetOverlayCount(IOverlayInterface::OverlayType::Normal, IOverlayInterface::OverlayLocation::Feet));

		logger::info("RM Says There are {} Head, {} Hand, {} Body and {} Feet Ovl Slots", FaceInfo.Count, HandsInfo.Count, BodyInfo.Count, FeetInfo.Count);
	}

	void Overlays::OnSKSEDataLoaded() {
		Overlays::Register();
	}

	bool Overlays::Loaded() {
		return OverlayInterface != nullptr && OverrideInterface != nullptr;
	}

	//--------------------------------------------------------------------------------
	// Bodymorph
	//--------------------------------------------------------------------------------

	void Morphs::OnSKSEDataLoaded() {
		Morphs::Register();
	}

	void Morphs::Register() {

		logger::info("Registering SKEE BodymorphInterface API");

		InterfaceExchangeMessage msg;

		const auto* const intfc{ SKSE::GetMessagingInterface() };

		intfc->Dispatch(SKEE::InterfaceExchangeMessage::kMessage_ExchangeInterface, &msg, sizeof(SKEE::InterfaceExchangeMessage*), "SKEE");

		if (!msg.interfaceMap) {
			logger::error("Couldn't Get SKSE interface map.");
			return;
		}

		MorphInterFace = static_cast<SKEE::IBodyMorphInterface*>(msg.interfaceMap->QueryInterface("BodyMorph"));

		if (!MorphInterFace) {
			logger::warn("Couldn't get SKEE interface.");
			return;
		}

		logger::info("SKEE BodyMorhInterface Version {}", MorphInterFace->GetVersion());
	}

	void Morphs::Set(RE::Actor* a_actor, const char* a_morphName, const float a_value, const char* a_key, const bool a_immediate) {
		if (!a_actor || !MorphInterFace) return;
		if (!a_actor->Is3DLoaded()) return;

		//logger::info("Setting Bodymorph \"{}\" for actor {} to {} ", a_morphName, a_actor->formID, a_value);
		MorphInterFace->SetMorph(a_actor, a_morphName, a_key, a_value);

		if (a_immediate) Apply(a_actor);
	}

	float Morphs::Get(RE::Actor* a_actor, const char* a_morphName, const char* a_key) {
		if (!a_actor || !MorphInterFace) return 0.0f;
		return MorphInterFace->GetMorph(a_actor, a_morphName, a_key);
	}

	//Warning this will erase all morphs on a character
	void Morphs::ClearAll(RE::Actor* a_actor) {
		if (!a_actor || !MorphInterFace) return;
		MorphInterFace->ClearMorphs(a_actor);
		logger::trace("Cleared all racemenu morphs from actor {}", a_actor->formID);
	}

	//Warning this will erase all morphs done by this mod
	void Morphs::Clear(RE::Actor* a_actor, const char* a_key) {
		if (!a_actor || !MorphInterFace) return;
		MorphInterFace->ClearBodyMorphKeys(a_actor, a_key);
		logger::trace("Cleared all {} morphs from actor {}", a_key, a_actor->formID);
	}

	//Remove a morph
	void Morphs::Clear(RE::Actor* a_actor, const char* a_morphName, const char* a_key) {
		if (!a_actor || !MorphInterFace) return;
		MorphInterFace->ClearMorph(a_actor, a_morphName, a_key);
		logger::trace("Cleared morph \"{}\" from actor {}", a_morphName, a_actor->formID);
	}

	//Instruct racemenu to update this actor
	void Morphs::Apply(RE::Actor* a_actor) {
		if (!a_actor || !MorphInterFace) return;
		if (!a_actor->Is3DLoaded()) return;

		MorphInterFace->ApplyBodyMorphs(a_actor, true);
		MorphInterFace->UpdateModelWeight(a_actor, false);
		logger::trace("Do bodymorph update on actor {}", a_actor->formID);
	}


	bool Morphs::HasKey(RE::Actor* a_actor, const char* a_key) {
		if (!a_actor || !MorphInterFace) return false;
		if (!a_actor->Is3DLoaded()) return false;
		return MorphInterFace->HasBodyMorphKey(a_actor, a_key);
	}

	bool Morphs::Loaded() {
		return MorphInterFace != nullptr;
	}

}

