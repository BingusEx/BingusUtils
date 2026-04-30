#include "Common/Utilities.hpp"

#include "Features/ManaTanks/Manatanks.hpp"
#include "Common/SKEE/SKEE.hpp"
#include "Common/UI/UIUtils.hpp"

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

		// --- Options ------------------------------------------------------------
		if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen)) {

			static std::string AlreadyAddedText = "";
			ImGui::Text("Add Actor");
			ImGui::SameLine();
			ImGui::TextDisabled(AlreadyAddedText.c_str());

			auto actorList = Utils::GetAllLoadedActors(true);

			static RE::ActorHandle s_selected{};

			// Build preview string
			std::string previewStr = "[Select Actor]";
			if (auto sel = s_selected.get()) {
				const char* name = sel->GetDisplayFullName();
				previewStr = fmt::format("{} [0x{:08X}]", name ? name : "[Unnamed]", sel->GetFormID());
			}

			ImVec2 contentWidth, addTextSize;
			ImGui::GetContentRegionAvail(&contentWidth);
			ImGui::CalcTextSize(&addTextSize, "Add", nullptr, true, -1.0f);

			// Add Actor ComboBox
			ImGui::SetNextItemWidth(contentWidth.x - addTextSize.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
			if (ImGui::BeginCombo("##AddActor", previewStr.c_str())) {
				for (RE::Actor* actor : actorList) {
					if (!actor) continue;

					const char* name = actor->GetDisplayFullName();
					const RE::FormID id = actor->GetFormID();
					const std::string item = fmt::format("{} [0x{:08X}]", name ? name : "[Unnamed]", id);
					const bool is_selected = (s_selected.get().get() == actor);

					if (ImGui::Selectable(item.c_str(), is_selected))
						s_selected = actor->GetHandle();

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();

			const bool canAdd = s_selected.get() && !ActorData.value.contains(s_selected.get()->formID);

			// Add Actor Button
			{
				ImGui::BeginDisabled(!canAdd);
				if (canAdd) ImUtil::ButtonStyle_Green();

				if (ImGui::Button("Add")) {
					if (RE::Actor* actor = s_selected.get().get()) {
						auto [it, inserted] = ActorData.value.emplace(actor->formID, Data{});
						it->second.LastKnownValue = -1.f;
					}
				}

				if (canAdd) ImUtil::ButtonStyle_Reset();
				ImGui::EndDisabled();
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				if (!s_selected.get())  ImGui::SetTooltip("Select an actor first.");
				else if (!canAdd)       ImGui::SetTooltip("This actor has already been added.");
				else                    ImGui::SetTooltip("Add the selected actor.");
			}

			// Update "Already Added" text
			AlreadyAddedText = !canAdd && s_selected.get() ? "[Already added]" : "";

			ImGui::Spacing();
			ImGui::InputFloat("EPS", &EPS.value, 0.001f, 0.01f, "%.5f");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Set the epsilon threshold for applying updated morphs\n"
					  "Lower: Smoother updates, higher performace cost.\n"
					  "Higher: Less frequent updates, lower performance cost."
				);
			}

			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::Spacing();

		}


		// --- Edit Actor ---------------------------------------------------------
		if (ImGui::CollapsingHeader("Edit Actor", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto ModeLabel = [](uint8_t m) -> const char* {
				switch (m) {
				case 0:  return "None";
				case 1:  return "Mana Tanks";
				case 2:  return "Absolute Scale Mode";
				default: return "Unknown";
				}
			};

			if (ActorData.value.empty()) {
				ImGui::TextDisabled("No actors added yet. Use the Options section above.");
			}
			else {
				static RE::FormID s_selectedID = 0;

				// Ensure selection is still valid
				if (s_selectedID != 0 && !ActorData.value.contains(s_selectedID))
					s_selectedID = 0;

				// Auto-select first entry if nothing is selected
				if (s_selectedID == 0 && !ActorData.value.empty())
					s_selectedID = ActorData.value.begin()->first;

				// Build preview
				auto BuildActorLabel = [](RE::FormID id) -> std::string {
					if (id == 0) return "[Select Actor]";
					if (auto* form = RE::TESForm::LookupByID(id))
						if (auto* actor = form->As<RE::Actor>()) {
							const char* name = actor->GetDisplayFullName();
							return fmt::format("{} [0x{:08X}]", name ? name : "[Unnamed]", id);
						}
					return fmt::format("[Missing] [0x{:08X}]", id);
				};

				{
					ImVec2 availableWidth, removeTextSize;
					ImGui::GetContentRegionAvail(&availableWidth);
					ImGui::CalcTextSize(&removeTextSize, "Remove", nullptr, true, -1.0f);

					ImGui::SetNextItemWidth(availableWidth.x - removeTextSize.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
					if (ImGui::BeginCombo("##EditActor", BuildActorLabel(s_selectedID).c_str())) {
						for (const auto& key : ActorData.value | std::views::keys) {
							const bool is_selected = (s_selectedID == key);
							if (ImGui::Selectable(BuildActorLabel(key).c_str(), is_selected))
								s_selectedID = key;
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Select a tracked actor to edit.");
					}
				}

				if (s_selectedID != 0) {
					auto it = ActorData.value.find(s_selectedID);
					if (it != ActorData.value.end()) {
						Data& data = it->second;

						RE::Actor* actor = nullptr;
						if (auto* form = RE::TESForm::LookupByID(s_selectedID))
							actor = form->As<RE::Actor>();

						// Actor Remove Button
						{
							ImGui::SameLine();

							ImUtil::ButtonStyle_Red();
							const bool doRemove = ImGui::Button("Remove##ActorOptions");
							ImUtil::ButtonStyle_Reset();

							ImGui::Spacing();

							if (doRemove) {
								ActorData.value.erase(s_selectedID);
								s_selectedID = ActorData.value.empty() ? 0 : ActorData.value.begin()->first;

								if (actor) {
									SKEE::Morphs::Clear(actor, MorphKey.data());
									SKEE::Morphs::Apply(actor);
								}
							}
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
									if (sel) ImGui::SetItemDefaultFocus();
								}
								ImGui::EndCombo();
							}
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("Select the scaling mode for this actor.");
							}
						}

						// ---- Magicka Reference ----
						{
							int ref = data.Reference;
							if (ImGui::InputInt("Magicka Reference##Edit", &ref, 1, 10)) {
								ref = std::clamp(ref, 0, 0xFFFF);
								const auto v = static_cast<uint16_t>(ref);
								if (v != data.Reference) {
									data.Reference = v;
									actorDataChanged = true;
								}
							}
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("The baseline magicka value used as the reference point for 100%% scaling.");
							}
						}

						// ---- Multipliers ----
						{
							ImGui::SetNextItemWidth(220.0f);
							if (ImGui::InputFloat("Multiplier##Edit", &data.ScaleMult, 0.01f, 0.1f, "%.3f"))
								actorDataChanged = true;
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("Scales the resulting mana tanks effect for this actor.\n"
									"1.0 = default strength, 2.0 = double effect, 0.0 = no effect.");
							}
						}

						{
							ImGui::SetNextItemWidth(220.0f);
							if (ImGui::InputFloat("Absolute Mode Mult.##Edit", &data.AbsScale, 0.01f, 0.1f, "%.3f"))
								actorDataChanged = true;
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("Multiplier applied when using Absolute Scale Mode.");
							}
						}

						// ---- invalidate on change ----
						if (actorDataChanged) {
							data.LastKnownValue = -1.f;
						}

						
					}
				}
			}

			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::Spacing();

		}

		// --- Morph Entries -------------------------------------------------------
		if (ImGui::CollapsingHeader("Morphs")) {
			auto& list = MorphData.value;
			bool morphChanged = false;

			// Add new entry
			ImGui::Text("Add New Morph");

			static std::string newName;
			static float newValue = 0.0f;

			ImGui::SetNextItemWidth(350.0f);
			ImGui::InputText("##NewMorphName", newName.data(), newName.capacity() + 1,
				ImGuiInputTextFlags_CallbackResize,
				[](ImGuiInputTextCallbackData* d) -> int {
					if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
						auto* s = static_cast<std::string*>(d->UserData);
						s->resize(static_cast<size_t>(d->BufTextLen));
						d->Buf = s->data();
					}
					return 0;
				},
				&newName
			);

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("The raw name of the NiOverride/RaceMenu morph (as is shown in outfit studio) to be added.");
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(150.0f);
			ImGui::InputFloat("##NewMorphVal", &newValue, 0.01f, 0.1f, "%.3f");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("The strength of the the to be added morph.\n"
					"Negative values are allowed.");
			}

			//Add New Morph Button
			{

				bool empty = newName.empty();
				ImGui::SameLine();
				ImGui::BeginDisabled(empty);
				if (!empty) ImUtil::ButtonStyle_Green();
				if (ImGui::Button("Add##NewMorph")) {
					list.push_back(MorphEntry{ newName, newValue });
					newName.clear();
					newValue = 0.0f;
					morphChanged = true;
				}
				if (!empty) ImUtil::ButtonStyle_Reset();
				ImGui::EndDisabled();
			}

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				if (newName.empty()) ImGui::SetTooltip("Enter a morph name first.");
				else ImGui::SetTooltip("Add this morph to the list.");
			}

			// -- Morph list --
			ImGui::Spacing();
			ImGui::Text("%s", fmt::format("Morphs ({})", list.size()).c_str());

			static auto resizeCb = [](ImGuiInputTextCallbackData* d) -> int {
				if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
					auto* s = static_cast<std::string*>(d->UserData);
					s->resize(static_cast<size_t>(d->BufTextLen));
					d->Buf = s->data();
				}
				return 0;
			};

			std::optional<std::size_t> eraseIndex;

			for (std::size_t i = 0; i < list.size(); ++i) {
				auto& e = list[i];
				ImGui::PushID(static_cast<int>(i));

				// Morph Name Edit
				{
					ImGui::SetNextItemWidth(350.0f);
					if (ImGui::InputText("##Name", e.morphName.data(), e.morphName.capacity() + 1, ImGuiInputTextFlags_CallbackResize, resizeCb, &e.morphName))
						morphChanged = true;
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Raw Morph key name as is shown in outfit studio.");
					}
				}

				// Morph Strength Value edit
				{
					ImGui::SameLine();
					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::InputFloat("##Value", &e.scale, 0.01f, 0.1f, "%.3f"))
						morphChanged = true;
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Adjust the strength multiplier of the morph.\n"
							"Negative values are allowed.");
					}
				}

				// Remove Morph Button
				{
					ImGui::SameLine();
					ImUtil::ButtonStyle_Red();
					if (ImGui::Button("X##Morphs"))
						eraseIndex = i;
					ImUtil::ButtonStyle_Reset();

					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Remove this morph.");
					}
				}
				ImGui::PopID();
			}

			if (eraseIndex) {
				list.erase(list.begin() + static_cast<std::ptrdiff_t>(*eraseIndex));
				morphChanged = true;
			}

			ImGui::Spacing();

			if (ImGui::Button("Reset to Defaults")) {
				list = DefaultEntries;
				morphChanged = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Restore the morph list to its default configuration.");
			}

			// Propagate changes
			if (morphChanged) {
				for (const auto& id : ActorData.value | std::views::keys) {
					if (auto* form = RE::TESForm::LookupByID(id))
						if (auto* actor = form->As<RE::Actor>())
							InvalidateData(actor);
				}
			}
		}
	}
}
