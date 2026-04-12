#include "Features/QuestUtils/QuestUtils.hpp"
#include "Common/Utilities.hpp"
#include "Common/UI/UIUtils.hpp"

namespace RE {

	bool TESQuest_SetStage(RE::TESQuest* quest, uint16_t stage) {
		if (!quest) return false;
		REL::Relocation<decltype(TESQuest_SetStage)> func(REL::RelocationID(24482, 25004, NULL));
		return func(quest, stage);
	}
}

namespace {

	struct QuestTargetEntry {
		RE::TESQuestTarget* target{};

		RE::TESObjectREFR* trackingRef{};
		RE::TESObjectREFR* targetRef{};

		RE::FormID trackingFormID{};
		RE::FormID targetFormID{};

		std::string trackingName;
		std::string targetName;
	};

	struct QuestObjectiveEntry {
		RE::BGSQuestObjective* objective{};
		std::vector<QuestTargetEntry> targets;
	};

	std::vector<RE::TESQuest*> questArray;

	bool IsObjectiveDisplayed(const RE::BGSQuestObjective* objective) {
		if (!objective) return false;
		return objective->state.get() >= RE::QUEST_OBJECTIVE_STATE::kDisplayed;
	}

	bool IsObjectiveCompleted(const RE::BGSQuestObjective* objective) {
		if (!objective) return false;
		return objective->state.get() >= RE::QUEST_OBJECTIVE_STATE::kCompleted;
	}

	std::vector<uint16_t> GetQuestStages(RE::TESQuest* quest) {
		std::vector<uint16_t> out;
		if (!quest) return out;

		if (quest->executedStages) {
			for (RE::TESQuestStage& stage : *quest->executedStages) {
				out.push_back(stage.data.index);
			}
		}

		if (quest->waitingStages) {
			for (RE::TESQuestStage* stage : *quest->waitingStages) {
				if (stage) {
					out.push_back(stage->data.index);
				}
			}
		}

		std::ranges::sort(out);

		auto [first, last] = std::ranges::unique(out);
		out.erase(first, last);

		return out;
	}


	std::vector<QuestObjectiveEntry> CollectObjectives(RE::TESQuest* quest) {
		std::vector<QuestObjectiveEntry> out;
		if (!quest) return out;

		out.reserve(quest->objectives.size());

		for (auto* objective : quest->objectives) {
			if (!objective) continue;

			QuestObjectiveEntry objectiveEntry;
			objectiveEntry.objective = objective;

			auto** targets = objective->targets;
			const auto count = objective->numTargets;

			if (targets && count > 0) {
				objectiveEntry.targets.reserve(count);

				for (std::uint32_t i = 0; i < count; ++i) {
					auto* target = targets[i];
					if (!target) continue;

					RE::ObjectRefHandle trackingHandle;
					RE::ObjectRefHandle targetHandle;
					target->GetTrackingRef(trackingHandle, quest);
					target->GetTargetRef(targetHandle, true, quest);

					if (!trackingHandle && !targetHandle) continue;

					QuestTargetEntry targetEntry;
					targetEntry.target = target;

					RE::TESObjectREFR* trackingRaw = trackingHandle ? trackingHandle.get().get() : targetHandle.get().get();
					RE::TESObjectREFR* targetRaw = targetHandle ? targetHandle.get().get() : trackingHandle.get().get();

					targetEntry.trackingRef = trackingRaw;
					targetEntry.trackingFormID = trackingRaw->formID;
					targetEntry.trackingName = BU::Utils::GetRefDisplayName(trackingRaw);

					targetEntry.targetRef = targetRaw;
					targetEntry.targetFormID = targetRaw->formID;
					targetEntry.targetName = BU::Utils::GetRefDisplayName(targetRaw);

					objectiveEntry.targets.push_back(std::move(targetEntry));
				}
			}
			out.push_back(std::move(objectiveEntry));
		}

		return out;
	}
}


namespace BU::Features {

	void QuestUtils::Draw() {

		static const auto& player = RE::PlayerCharacter::GetSingleton();

		if (questArray.empty()) {
			ImGui::TextDisabled("No Quests");
			ImGui::End();
			return;
		}

		static ImGuiTextFilter filter;
		filter.Draw("Filter", 250.0f);

		for (RE::TESQuest* quest : questArray) {
			if (!quest) continue;
			if (!quest->IsRunning() || !quest->IsActive()) continue;

			
			// Header
			{
				const char* const enabled = quest->IsEnabled() ? "Y" : "N";
				const char* const editorID = quest->GetFormEditorID() ? quest->GetFormEditorID() : "";
				const std::string questName = Utils::GetFormName(quest);
				const std::string header = std::format("{} [{}]##{:08X} | E: {}", questName, editorID, quest->GetFormID(), enabled);

				if (!filter.PassFilter(header.c_str())) continue;
				if (!ImGui::CollapsingHeader(header.c_str())) continue;
			}

			std::vector<QuestObjectiveEntry> objectives = CollectObjectives(quest);
			
			const RE::FormID formID = quest->GetFormID();

			//Options
			{
				ImGui::PushID(formID);

				ImGui::Text("Stage Selection");

				static std::uint16_t selectedStage = 0;
				for (uint16_t stage : GetQuestStages(quest)) {
					if (ImGui::SmallButton(std::format("{}", stage).c_str())) {
						selectedStage = stage;
					}
					if (ImGui::IsItemHovered()) {
						for (auto& objective : quest->objectives) {
							std::string objectiveText = objective->displayText.c_str();
							ImGui::SetTooltip(std::format("{}\n{}", objective->index == stage ? "(Current)" : "", objectiveText).c_str());
						}
					}
					ImGui::SameLine();
				}
				ImGui::NewLine();

				{
					ImGui::SetNextItemWidth(100.0f);
					ImGui::InputScalar("##Selection", ImGuiDataType_U16, &selectedStage);
				}

				ImGui::SameLine();

				//Set Stage Button
				{
					ImUtil::ButtonStyle_Green();

					if (ImGui::Button("Set Stage")) {
						RE::TESQuest_SetStage(quest, selectedStage);
					}
					ImUtil::ButtonStyle_Reset();
				}

				ImGui::PopID();
			}

			ImGui::Spacing();

			// Targets
			{
				std::vector<const QuestObjectiveEntry*> pending;
				for (const auto& entry : objectives) {
					if (IsObjectiveDisplayed(entry.objective) &&
						!IsObjectiveCompleted(entry.objective) &&
						!entry.targets.empty()) {
						pending.push_back(&entry);
					}
				}

				if (!pending.empty()) {

					ImGui::Spacing();

					ImGui::Text("Targets");

					ImGui::Indent();

					for (const auto* entry : pending) {
						ImGui::Text("[%u] %s", entry->objective->index, entry->objective->displayText.c_str());

						ImGui::Indent();

						for (const auto& target : entry->targets) {
							const bool hasSeparateTarget = target.trackingFormID != target.targetFormID;

							ImGui::PushID(static_cast<int>(target.trackingFormID));

							// Tracking ref row
							ImGui::TextDisabled(target.trackingName.c_str());
							ImGui::SameLine();
							if (ImGui::SmallButton("Go to##track")) {
								if (target.trackingRef && player)
									player->MoveTo(target.trackingRef);
							}
							ImGui::SameLine();
							if (ImGui::SmallButton("Pull here##track")) {
								if (target.trackingRef && player)
									target.trackingRef->MoveTo(player);
							}

							if (hasSeparateTarget) {
								ImGui::Indent();
								ImGui::Text("%s", target.targetName.c_str());
								ImGui::SameLine();
								if (ImGui::SmallButton("Go to##tgt")) {
									if (target.targetRef && player)
										player->MoveTo(target.targetRef);
								}
								ImGui::SameLine();
								if (ImGui::SmallButton("Pull here##tgt")) {
									if (target.targetRef && player)
										target.targetRef->MoveTo(player);
								}
								ImGui::Unindent();
							}

							ImGui::PopID();
						}
						ImGui::Unindent();
					}
					ImGui::Unindent();
				}
			}
		}
		ImGui::End();
	}

	void QuestUtils::OnSKSEDataLoaded() {
		if (RE::TESDataHandler* tes = RE::TESDataHandler::GetSingleton()) {
			for (RE::TESForm* form : tes->GetFormArray(RE::FormType::Quest)) {
				if (RE::TESQuest* quest = skyrim_cast<RE::TESQuest*>(form)) {
					questArray.push_back(quest);
				}
			}
		}
	}
}