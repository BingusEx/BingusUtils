#include "Common/Utilities.hpp"

#include "Features/ManaTanks/Manatanks.hpp"
#include "Common/SKEE/SKEE.hpp"

namespace {
	
	float GetMaxAV(RE::Actor* actor, RE::ActorValue av) {
		auto baseValue = actor->AsActorValueOwner()->GetBaseActorValue(av);
		auto permMod = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIERS::kPermanent, av);
		auto tempMod = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIERS::kTemporary, av);
		return baseValue + permMod + tempMod;
	}

	float GetAV(RE::Actor* actor, RE::ActorValue av) {
		float max_av = GetMaxAV(actor, av);
		auto damageMod = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIERS::kDamage, av);
		return max_av + damageMod;
	}

	float GetPercentageAV(RE::Actor* actor, RE::ActorValue av) {
		return GetAV(actor, av) / GetMaxAV(actor, av);
	}

}

namespace BU::Features {

	void ManaTanks::OnSerdeSave(SKSE::SerializationInterface* a_this) {
		{
			std::unique_lock lock(_Lock);
			ActorData.Save(a_this);
			MorphData.Save(a_this);
			EPS.Save(a_this);
		}
	}

	void ManaTanks::OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {

		{
			std::unique_lock lock(_Lock);
			ActorData.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
			MorphData.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
			EPS.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		}

	}

	void ManaTanks::OnActorReset(RE::Actor* a_actor) {
		RemoveActor(a_actor);
	}

	void ManaTanks::OnActorUpdate(RE::Actor* a_actor) {
		if (SKEE::Morphs::Loaded()) {

			std::unique_lock<std::mutex>lock(_Lock);

			if (ActorData.value.contains(a_actor->formID)) {
				auto& data = ActorData.value.at(a_actor->formID);

				switch (static_cast<Mode>(data.CurMode)) {
					case kNone : 
					{
						if (data.ReqMode != data.CurMode) {
							InvalidateData(a_actor);
							data.CurMode = data.ReqMode;
						}

					} return;

					case kMT:
					{
						float AV = GetPercentageAV(a_actor, RE::ActorValue::kMagicka);
						const float Multiplier = GetMaxAV(a_actor, RE::ActorValue::kMagicka) / data.Reference * AV * data.ScaleMult;
						if (std::abs(data.LastKnownValue - Multiplier) > EPS.value) {
							data.LastKnownValue = Multiplier;
							for (auto& morph : MorphData.value) {
								SKEE::Morphs::Set(a_actor, morph.morphName.data(), morph.scale * Multiplier, MorphKey.data(), false);
							}
							SKEE::Morphs::Apply(a_actor);
						}

					} return;

					case kAbs:
					{
						if (std::abs(data.LastKnownValue - data.AbsScale) > EPS.value) {
							data.LastKnownValue = data.AbsScale;
							for (auto& morph : MorphData.value) {
								SKEE::Morphs::Set(a_actor, morph.morphName.data(), morph.scale * data.AbsScale, MorphKey.data(), false);
							}
							SKEE::Morphs::Apply(a_actor);
						}

					} return;
				}
			}
		}
	}

	void ManaTanks::OnSerdePostLoad() {
		if (MorphData.value.empty()) {
			MorphData.value = DefaultEntries;
		}
	}

	void ManaTanks::RemoveActor(RE::Actor* a_actor) {

		{
			std::unique_lock lock(_Lock);
			ActorData.value.erase(a_actor->formID);
		}

		SKEE::Morphs::Clear(a_actor, MorphKey.data());
		SKEE::Morphs::Apply(a_actor);
	}

	void ManaTanks::InvalidateData(RE::Actor* a_actor) {
		if (SKEE::Morphs::Loaded()) {
			if (ActorData.value.contains(a_actor->formID)) {
				auto& data = ActorData.value.at(a_actor->formID);
				SKEE::Morphs::Clear(a_actor, MorphKey.data());
				SKEE::Morphs::Apply(a_actor);
				data.LastKnownValue = -1.f;
			}
		}
	}

	void ManaTanks::Draw() {

		// ---- Add actor section ----
		if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto actorList = Utils::GetAllLoadedActors();

			static RE::ActorHandle s_selected{};
			const char* preview = "<Select Actor>";
			RE::FormID previewID = 0;

			if (auto sel = s_selected.get()) {
				preview = sel->GetDisplayFullName();
				previewID = sel->GetFormID();
			}

			const std::string previewStr =
				(previewID != 0)
				? fmt::format("{} [0x{:08X}]", preview, previewID)
				: std::string(preview);

			if (ImGui::BeginCombo("##AddActor", previewStr.c_str())) {
				for (RE::Actor* actor : actorList) {
					if (!actor) {
						continue;
					}

					const auto name = actor->GetDisplayFullName();
					const auto id = actor->GetFormID();
					const std::string item = fmt::format("{} [0x{:08X}]", name ? name : "<Unnamed>", id);

					const bool is_selected = (s_selected.get().get() == actor);
					if (ImGui::Selectable(item.c_str(), is_selected)) {
						s_selected = actor->GetHandle();
					}
					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();

			if (ImGui::Button("Add New Actor")) {
				if (RE::Actor* actor = s_selected.get().get()) {
					auto [it, inserted] = ActorData.value.emplace(actor->formID, Data{});
					// on any ActorData change, invalidate by forcing update
					it->second.LastKnownValue = -1.f;
				}
			}
			ImGui::InputFloat("EPS", &EPS.value, 0.001f, 0.01f, "%.5f");
		}

		if (ImGui::CollapsingHeader("Edit Actor", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto ModeLabel = [](uint8_t m) -> const char* {
				switch (m) {
					case 0: return "None";
					case 1: return "Mana Tanks";
					case 2: return "Absolute Scale Mode";
					default: return "Unknown";
				}
			};

			static RE::FormID s_selectedID = 0;

			if (ActorData.value.empty()) {
				ImGui::TextUnformatted("No actors added.");
				return;
			}

			// ---- preview label ----
			std::string preview = "<Select Actor>";
			if (s_selectedID != 0) {
				if (auto* form = RE::TESForm::LookupByID(s_selectedID)) {
					if (auto* actor = form->As<RE::Actor>()) {
						const char* name = actor->GetDisplayFullName();
						preview = fmt::format("{} [0x{:08X}]", name ? name : "<Unnamed>", s_selectedID);
					}
					else {
						preview = fmt::format("<Not An Actor> [0x{:08X}]", s_selectedID);
					}
				}
				else {
					preview = fmt::format("<Missing> [0x{:08X}]", s_selectedID);
				}
			}

			// ---- combo ----
			if (ImGui::BeginCombo("Actor##Edit", preview.c_str())) {
				for (const auto& key : ActorData.value | std::views::keys) {
					RE::Actor* actor = nullptr;
					if (auto* form = RE::TESForm::LookupByID(key)) {
						actor = form->As<RE::Actor>();
					}

					const char* name = (actor && actor->GetDisplayFullName()) ? actor->GetDisplayFullName() : "<Unnamed>";
					const std::string label = fmt::format("{} [0x{:08X}]", name, key);

					const bool is_selected = (s_selectedID == key);
					if (ImGui::Selectable(label.c_str(), is_selected)) {
						s_selectedID = key;
					}
					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (s_selectedID != 0) {

				ImGui::SameLine();

				auto it = ActorData.value.find(s_selectedID);
				if (it == ActorData.value.end()) {
					ImGui::TextUnformatted("Selected actor no longer exists in ActorData.");
					return;
				}

				Data& data = it->second;

				RE::Actor* actor = nullptr;
				if (auto* form = RE::TESForm::LookupByID(s_selectedID)) {
					actor = form->As<RE::Actor>();
				}

				if (ImGui::Button("Remove##Edit")) {
					const RE::FormID removeID = s_selectedID;

					ActorData.value.erase(removeID);

					// pick a new selection
					s_selectedID = 0;
					if (!ActorData.value.empty()) {
						s_selectedID = ActorData.value.begin()->first;
					}

					if (actor) {
						SKEE::Morphs::Clear(actor, MorphKey.data());
						SKEE::Morphs::Apply(actor);
					}
					return;
				}

				bool actorDataChanged = false;

				// ---- Mode ----
				{
					if (ImGui::BeginCombo("Mode##Edit", ModeLabel(data.CurMode))) {
						for (int m = 0; m <= 2; ++m) {
							const auto mm = static_cast<uint8_t>(m);
							const bool sel = (data.CurMode == mm);
							if (ImGui::Selectable(ModeLabel(mm), sel)) {
								data.CurMode = mm;
								actorDataChanged = true;
							}
							if (sel) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

				}

				ImGui::Separator();

				// ---- Magicka Reference ----
				{
					int ref = static_cast<int>(data.Reference);
					if (ImGui::InputInt("Magicka Reference##Edit", &ref, 1, 10)) {
						ref = std::clamp(ref, 0, 0xFFFF);
						const auto v = static_cast<uint16_t>(ref);
						if (v != data.Reference) {
							data.Reference = v;
							actorDataChanged = true;
						}
					}
				}

				// ---- Multipliers ----
				if (ImGui::InputFloat("Multiplier##Edit", &data.ScaleMult, 0.01f, 0.1f, "%.3f")) {
					actorDataChanged = true;
				}

				if (ImGui::InputFloat("Absolute Mode Mult.##Edit", &data.AbsScale, 0.01f, 0.1f, "%.3f")) {
					actorDataChanged = true;
				}

				// ---- invalidate on change ----
				if (actorDataChanged) {
					data.LastKnownValue = -1.f;
				}
			}
		}

		if (ImGui::CollapsingHeader("Morph Entries", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto& list = MorphData.value;

			static std::string newName;
			static float newValue = 0.0f;

			bool morphChanged = false;

			ImGui::SetNextItemWidth(350.0f);
			ImGui::InputText(
				"##NewMorphName",
				newName.data(),
				newName.capacity() + 1,
				ImGuiInputTextFlags_CallbackResize,
				[](ImGuiInputTextCallbackData* data) -> int {
					if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
						auto* s = static_cast<std::string*>(data->UserData);
						s->resize(static_cast<size_t>(data->BufTextLen));
						data->Buf = s->data();
					}
					return 0;
				},
				&newName);

			ImGui::SameLine();
			ImGui::SetNextItemWidth(220.f);
			if (ImGui::InputFloat("##NewMorphVal", &newValue, 0.01f, 0.1f, "%.3f")) {
				// not a data change until actually added
			}

			ImGui::SameLine();
			if (ImGui::Button("Add##NewMorph")) {
				if (!newName.empty()) {
					list.push_back(MorphEntry{ newName, newValue });
					newName.clear();
					newValue = 0.0f;
					morphChanged = true;
				}
			}

			ImGui::Separator();

			std::optional<std::size_t> eraseIndex;

			for (std::size_t i = 0; i < list.size(); ++i) {
				auto& e = list[i];

				ImGui::PushID(static_cast<int>(i));

				// Name edit
				{
					auto cb = [](ImGuiInputTextCallbackData* data) -> int {
						if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
							auto* s = static_cast<std::string*>(data->UserData);
							s->resize(static_cast<size_t>(data->BufTextLen));
							data->Buf = s->data();
						}
						return 0;
					};

					ImGui::SetNextItemWidth(350.0f);
					if (ImGui::InputText("##Name", e.morphName.data(), e.morphName.capacity() + 1,
						ImGuiInputTextFlags_CallbackResize, cb, &e.morphName)) {
						morphChanged = true;
					}
				}

				ImGui::SameLine();
				ImGui::SetNextItemWidth(220.0f);
				if (ImGui::InputFloat("##Value", &e.scale, 0.01f, 0.1f, "%.3f")) {
					morphChanged = true;
				}

				ImGui::SameLine();
				if (ImGui::Button("Remove")) {
					eraseIndex = i;
				}

				ImGui::PopID();
			}

			if (eraseIndex) {
				list.erase(list.begin() + static_cast<std::ptrdiff_t>(*eraseIndex));
				morphChanged = true;
			}

			if (ImGui::Button("Reset to Defaults")) {
				list = DefaultEntries;
				morphChanged = true;
			}

			// ---- if MorphData changes call InvalidateData(actor) ----
			if (morphChanged) {
				for (const auto& id : ActorData.value | std::views::keys) {
					if (auto* form = RE::TESForm::LookupByID(id)) {
						if (auto* actor = form->As<RE::Actor>()) {
							InvalidateData(actor);
						}
					}
				}
			}
		}
	}
}
