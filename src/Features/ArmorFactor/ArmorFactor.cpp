#include "Common/Utilities.hpp"

#include "Features/ArmorFactor/ArmorFactor.hpp"
#include "Common/SKEE/SKEE.hpp"
#include "Common/UI/UIUtils.hpp"

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

        // ─── Options ────────────────────────────────────────────────────────────
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

            ImVec2 contentWidth, AddNewActor_textSize;
            ImGui::GetContentRegionAvail(&contentWidth);
            ImGui::CalcTextSize(&AddNewActor_textSize, "Add", nullptr, true, -1.0f);

            // Add Actor ComboBox
            ImGui::SetNextItemWidth(contentWidth.x - AddNewActor_textSize.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
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
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Select a loaded actor to track.");
            }

            ImGui::SameLine();

            const bool canAdd = s_selected.get() && !ActorData.value.contains(s_selected.get()->formID);

            // Add Actor Button
            {
                ImGui::BeginDisabled(!canAdd);
                if (canAdd) ImUtil::ButtonStyle_Green();

                if (ImGui::Button("Add##Options")) {
                    if (RE::Actor* actor = s_selected.get().get()) {
                        auto [it, inserted] = ActorData.value.emplace(actor->formID, Data{});
                        it->second.NeedsUpdate = true;
                    }
                }

                if (canAdd) ImUtil::ButtonStyle_Reset();
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (!s_selected.get()) ImGui::SetTooltip("Select an actor first.");
                else if (!canAdd)      ImGui::SetTooltip("This actor has already been added.");
                else                   ImGui::SetTooltip("Add the selected actor to the armor factor system.");
            }

            // Update "Already Added" text
            AlreadyAddedText = !canAdd && s_selected.get() ? "[Already added]" : "";

            // Regex Filter Input Text
            {
                ImGui::Spacing();
                ImGui::Text("Valid Armors");

                const std::string regexBefore = ValidNameRegex.value;

                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText("##Regex", ValidNameRegex.value.data(), ValidNameRegex.value.capacity() + 1,
                    ImGuiInputTextFlags_CallbackResize,
                    [](ImGuiInputTextCallbackData* d) -> int {
                        if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                            auto* s = static_cast<std::string*>(d->UserData);
                            s->resize(static_cast<size_t>(d->BufTextLen));
                            d->Buf = s->data();
                        }
                        return 0;
                    },
                    &ValidNameRegex.value
                );

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "A regex based filter defining which armors should use the armor factor if equipped.\n"
                        "The default setup checks for a case insensitive keyword in either the EditorID or In-Game Name."
                    );
                }

                if (ValidNameRegex.value != regexBefore) {
                    for (auto& data : ActorData.value | std::views::values) {
                        data.NeedsUpdate = true;
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

        }

        // ─── Edit Actor ─────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Edit Actor", ImGuiTreeNodeFlags_DefaultOpen)) {
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

                            if (ImGui::Checkbox("Enabled##ActorOptions", &data.Enabled)) {
                                data.NeedsUpdate = true;
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Temporarily enable or disable the armor factor effect for this actor.");
                            }

                            ImGui::SetNextItemWidth(220.0f);
                            if (ImGui::InputFloat("Multiplier##ActorOptions", &data.ScaleMult, 0.01f, 0.1f, "%.2f")) {
                                data.NeedsUpdate = true;
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Scale the armor factor effect for this actor.");
                            }
                        }
                    }
                }

				
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

        }

        // ─── Morph Entries ───────────────────────────────────────────────────────
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

            // ── Morph list ──
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
