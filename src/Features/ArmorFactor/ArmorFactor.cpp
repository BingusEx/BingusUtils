#include "Common/Utilities.hpp"

#include "Features/ArmorFactor/ArmorFactor.hpp"
#include "Common/SKEE/SKEE.hpp"

namespace BU::Features {

	void ArmorFactor::OnSerdeSave(SKSE::SerializationInterface* a_this) {
		{
			std::unique_lock lock(_Lock);
			ActorData.Save(a_this);
			MorphData.Save(a_this);
			ValidNameRegex.Save(a_this);
		}
	}

	void ArmorFactor::OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {

		{
			std::unique_lock lock(_Lock);

			ActorData.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
			MorphData.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
			ValidNameRegex.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		}
	}

	void ArmorFactor::OnSerdePostLoad() {
		if (MorphData.value.empty()) {
			MorphData.value = DefaultEntries;
		}

		if (ValidNameRegex.value.empty()) {
			ValidNameRegex.value = R"((?i)(?:^|[^A-Za-z0-9])(?:bra|bikini|top|bandage)(?:$|[^A-Za-z0-9]))";
		}
	}

	void ArmorFactor::OnActorReset(RE::Actor* a_actor) {
		RemoveActor(a_actor);
	}

	void ArmorFactor::OnActorUpdate(RE::Actor* a_actor) {
		if (SKEE::Morphs::Loaded()) {
			if (ActorData.value.contains(a_actor->formID)) {
				auto& data = ActorData.value.at(a_actor->formID);
				if (data.NeedsUpdate) {
					if (data.Enabled) {
						const auto inv = a_actor->GetInventory([](RE::TESBoundObject& a_object) {
							return a_object.IsArmor();
						}, false);

						const auto pattern = ValidNameRegex.value; 
						const auto rx = Utils::Regex::Re2Check(pattern);

						for (const auto& [item, invData] : inv) {
							const auto& [count, entry] = invData;
							if (count > 0 && entry->IsWorn()) {
								const auto armor = item->As<RE::TESObjectARMO>();
								if (armor) {
									if (armor->GetFullNameLength() > 0) {
										const char* name = armor->GetFullName();
										const char* edid = armor->GetFormEditorID();
										if (Utils::Regex::SearchSafe(name, *rx) || Utils::Regex::SearchSafe(edid, *rx)) {
											for (const auto& morph : MorphData.value) {
												SKEE::Morphs::Set(a_actor, morph.morphName.c_str(), morph.scale * data.ScaleMult, MorphKey.data(), false);
											}
											SKEE::Morphs::Apply(a_actor);
											data.NeedsUpdate = false;
											return;
										}
									}
								}
							}
						}
					}
					SKEE::Morphs::Clear(a_actor, MorphKey.data());
					SKEE::Morphs::Apply(a_actor);
					data.NeedsUpdate = false;
				}
			}

		}
	}

	void ArmorFactor::OnActorEquip(RE::Actor* a_actor) {
		if (SKEE::Morphs::Loaded()) {
			if (ActorData.value.contains(a_actor->formID)) {
				auto& data = ActorData.value.at(a_actor->formID);
				if (data.Enabled) {
					data.NeedsUpdate = true;
				}
			}
		}
		
	}

	void ArmorFactor::OnActorUnequip(RE::Actor* a_actor) {
		if (SKEE::Morphs::Loaded()) {
			if (ActorData.value.contains(a_actor->formID)) {
				auto& data = ActorData.value.at(a_actor->formID);
				if (data.Enabled) {
					data.NeedsUpdate = true;
				}
			}
		}
	}

	void ArmorFactor::RemoveActor(RE::Actor* a_actor) {

		{
			std::unique_lock lock(_Lock);
			ActorData.value.erase(a_actor->formID);
		}

		SKEE::Morphs::Clear(a_actor, MorphKey.data());
		SKEE::Morphs::Apply(a_actor);
	}

	void ArmorFactor::InvalidateData(RE::Actor* a_actor) {
		if (SKEE::Morphs::Loaded()) {
			if (ActorData.value.contains(a_actor->formID)) {
				auto& data = ActorData.value.at(a_actor->formID);
				SKEE::Morphs::Clear(a_actor, MorphKey.data());
				SKEE::Morphs::Apply(a_actor);
				data.NeedsUpdate = true;
			}
		}
	}

	void ArmorFactor::Draw() {

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
					it->second.NeedsUpdate = true;
				}
			}

			{
				char* regexPreview = ValidNameRegex.value.data();
				ImGui::InputText("Regex", regexPreview, 2048);
				ValidNameRegex.value = regexPreview;
			}

		}

		if (ImGui::CollapsingHeader("Edit Actor", ImGuiTreeNodeFlags_DefaultOpen)) {

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

				// ---- Enable ----
				if (ImGui::Checkbox("Enable##Edit", &data.Enabled)) {
					data.NeedsUpdate = true;
				}

				// ---- Multipliers ----
				if (ImGui::InputFloat("Multiplier##Edit", &data.ScaleMult, 0.01f, 0.1f, "%.3f")) {
					data.NeedsUpdate = true;
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
