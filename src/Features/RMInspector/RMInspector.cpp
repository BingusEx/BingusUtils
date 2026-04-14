#include "RMInspector.hpp"

#include "Common/Utilities.hpp"
#include "Common/SKEE/SKEE.hpp"
#include "Common/UI/UIUtils.hpp"

namespace {

	struct MorphEditorState {
		std::vector<BU::SKEE::Morphs::MorphEntry> Entries;
		std::vector<float> WorkingValues;
		RE::FormID LastActorID = 0;

		void Refresh(RE::Actor* a_actor) {
			Entries = BU::SKEE::Morphs::CollectAll(a_actor);
			WorkingValues.assign(Entries.size(), 0.f);
			for (std::size_t i = 0; i < Entries.size(); ++i) {
				WorkingValues[i] = Entries[i].Value;
			}
			LastActorID = a_actor ? a_actor->formID : 0;
		}
	};

	struct NodeEditorState {
		std::vector<BU::SKEE::Transforms::NodeEntry> Entries;

		struct WorkingNode {
			float Pos[3] = { 0.f, 0.f, 0.f };
			float Rot[3] = { 0.f, 0.f, 0.f };
			float Scale = 1.f;
		};

		std::vector<WorkingNode> Working;
		RE::FormID LastActorID = 0;

		void Refresh(RE::Actor* a_actor) {
			Entries = BU::SKEE::Transforms::CollectAll(a_actor);
			Working.resize(Entries.size());
			for (std::size_t i = 0; i < Entries.size(); ++i) {
				const auto& e = Entries[i];
				auto& w = Working[i];
				w.Pos[0] = e.Pos.x;
				w.Pos[1] = e.Pos.y;
				w.Pos[2] = e.Pos.z;
				w.Rot[0] = e.Rot.heading;
				w.Rot[1] = e.Rot.attitude;
				w.Rot[2] = e.Rot.bank;
				w.Scale  = e.HasScale ? e.Scale : 1.f;
			}
			LastActorID = a_actor ? a_actor->formID : 0;
		}
	};

	enum class MorphColumn : ImGuiID {
		MorphName = 0,
		Key       = 1,
		Value     = 2
	};

	enum class NodeColumn : ImGuiID {
		Node     = 0,
		Key      = 1,
		Position = 2,
		Rotation = 3,
		Scale    = 4
	};

	RE::FormID s_selectedActorID = 0;
	MorphEditorState s_morphState;
	NodeEditorState s_nodeState;
	char s_morphFilter[512] = {};
	char s_nodeFilter[512] = {};

	RE::Actor* FindActorByFormID(const std::vector<RE::Actor*>& a_actors, RE::FormID a_id) {
		for (auto* a : a_actors) {
			if (a && a->formID == a_id) return a;
		}
		return nullptr;
	}

	std::string BuildActorLabel(RE::Actor* a_actor) {

		if (!a_actor) return "[Null]";

		const char* name = a_actor->GetDisplayFullName();
		if (name && name[0] != '\0') {
			return std::format("{} [{:08X}]", name, a_actor->formID);
		}
		return std::format("[{:08X}]", a_actor->formID);
	}

	bool PassesFilter(std::string_view a_str, const char* a_filter) {

		if (!a_filter || a_filter[0] == '\0') return true;

		const std::size_t filterLen = std::strlen(a_filter);
		auto it = std::search(
			a_str.begin(), a_str.end(),
			a_filter, a_filter + filterLen,
			[](char a, char b) {
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			}
		);
		return it != a_str.end();
	}

	int32_t CompareTextInsensitive(std::string_view a, std::string_view b) {

		const auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };

		const std::size_t n = std::min(a.size(), b.size());
		for (std::size_t i = 0; i < n; ++i) {
			const char ca = lower(static_cast<unsigned char>(a[i]));
			const char cb = lower(static_cast<unsigned char>(b[i]));
			if (ca < cb) return -1;
			if (ca > cb) return  1;
		}

		if (a.size() < b.size()) return -1;
		if (a.size() > b.size()) return  1;
		return 0;
	}

	template <class T>
	int32_t CompareScalar(T a, T b) {
		if (a < b) return -1;
		if (a > b) return  1;
		return 0;
	}

	void SortMorphIndices(std::vector<std::size_t>& a_indices, const MorphEditorState& a_state, const ImGuiTableSortSpecs* a_specs) {
		if (!a_specs || a_specs->SpecsCount <= 0) {
			return;
		}

		std::ranges::stable_sort(a_indices, [&](std::size_t lhsIdx, std::size_t rhsIdx) {
			
			const auto& lhs = a_state.Entries[lhsIdx];
			const auto& rhs = a_state.Entries[rhsIdx];

			for (int32_t i = 0; i < a_specs->SpecsCount; ++i) {
				const ImGuiTableColumnSortSpecs& spec = a_specs->Specs[i];

				int32_t result = 0;
				switch (static_cast<MorphColumn>(spec.ColumnUserID)) {
					case MorphColumn::MorphName: 
					{
						result = CompareTextInsensitive(lhs.MorphName, rhs.MorphName);
						break;
					}
						
					case MorphColumn::Key: 
					{
						result = CompareTextInsensitive(lhs.Key, rhs.Key);
						break;
					}
						
					case MorphColumn::Value:
					{
						result = CompareScalar(a_state.WorkingValues[lhsIdx], a_state.WorkingValues[rhsIdx]);
						break;
					}
				}

				if (result != 0) {
					return spec.SortDirection == ImGuiSortDirection_Ascending ? (result < 0) : (result > 0);
				}
			}

			return lhsIdx < rhsIdx;
		});
	}

	void SortNodeIndices(std::vector<std::size_t>& a_indices, const NodeEditorState& a_state, const ImGuiTableSortSpecs* a_specs) {
		
		if (!a_specs || a_specs->SpecsCount <= 0) {
			return;
		}

		std::ranges::stable_sort(a_indices, [&](std::size_t lhsIdx, std::size_t rhsIdx) {
			
			const auto& lhs = a_state.Entries[lhsIdx];
			const auto& rhs = a_state.Entries[rhsIdx];
			const auto& lhsW = a_state.Working[lhsIdx];
			const auto& rhsW = a_state.Working[rhsIdx];

			for (int32_t i = 0; i < a_specs->SpecsCount; ++i) {
				const ImGuiTableColumnSortSpecs& spec = a_specs->Specs[i];

				int32_t result = 0;
				switch (static_cast<NodeColumn>(spec.ColumnUserID)) {
					case NodeColumn::Node: 
					{
						result = CompareTextInsensitive(lhs.Node, rhs.Node);
						break;
					}

					case NodeColumn::Key:
					{
						result = CompareTextInsensitive(lhs.Key, rhs.Key);
						break;
					}

						
					case NodeColumn::Position: 
					{
						const float lhsMagSq =
							lhsW.Pos[0] * lhsW.Pos[0] +
							lhsW.Pos[1] * lhsW.Pos[1] +
							lhsW.Pos[2] * lhsW.Pos[2];

						const float rhsMagSq =
							rhsW.Pos[0] * rhsW.Pos[0] +
							rhsW.Pos[1] * rhsW.Pos[1] +
							rhsW.Pos[2] * rhsW.Pos[2];

						result = CompareScalar(lhsMagSq, rhsMagSq);
						break;
					}

					case NodeColumn::Rotation: 
					{
						const float lhsMagSq =
							lhsW.Rot[0] * lhsW.Rot[0] +
							lhsW.Rot[1] * lhsW.Rot[1] +
							lhsW.Rot[2] * lhsW.Rot[2];
						const float rhsMagSq =
							rhsW.Rot[0] * rhsW.Rot[0] +
							rhsW.Rot[1] * rhsW.Rot[1] +
							rhsW.Rot[2] * rhsW.Rot[2];

						result = CompareScalar(lhsMagSq, rhsMagSq);
						break;
					}

					case NodeColumn::Scale:
					{
						result = CompareScalar(lhsW.Scale, rhsW.Scale);
						break;
					}

				}

				if (result != 0) {
					return spec.SortDirection == ImGuiSortDirection_Ascending ? (result < 0) : (result > 0);
				}
			}

			return lhsIdx < rhsIdx;
		});
	}

}

namespace BU::Features {

	void RMInspector::Draw() {

		// Actor Select

		const std::vector<RE::Actor*> actors = Utils::GetAllLoadedActors(true);

		RE::Actor* selectedActor = FindActorByFormID(actors, s_selectedActorID);
		if (!selectedActor && !actors.empty()) {
			selectedActor = actors[0];
			s_selectedActorID = selectedActor ? selectedActor->formID : 0;
		}

		ImGui::Text("Actor");

		const std::string currentLabel = selectedActor ? BuildActorLabel(selectedActor) : "None";

		if (ImGui::BeginCombo("##ActorSelect", currentLabel.c_str())) {
			for (RE::Actor* actor : actors) {
				if (!actor) continue;
				const bool isSelected = (actor->formID == s_selectedActorID);
				const std::string label = BuildActorLabel(actor);
				if (ImGui::Selectable(label.c_str(), isSelected)) {
					s_selectedActorID = actor->formID;
					selectedActor = actor;
					s_morphState.LastActorID = 0; // Force refresh
					s_nodeState.LastActorID = 0;
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (!selectedActor) {
			ImGui::TextDisabled("No actors loaded.");
			return;
		}

		if (s_morphState.LastActorID != selectedActor->formID) {
			s_morphState.Refresh(selectedActor);
		}
		if (s_nodeState.LastActorID != selectedActor->formID) {
			s_nodeState.Refresh(selectedActor);
		}

		ImGui::Spacing();

		// Morph Editor

		if (ImGui::CollapsingHeader("Body Morphs", ImGuiTreeNodeFlags_DefaultOpen)) {

			if (!SKEE::Morphs::Loaded()) {
				ImGui::TextDisabled("SKEE BodyMorphterface not available.");
			}
			else {
				// ---- Toolbar -----------------------------------------------
				{
					ImVec2 avail;
					ImGui::GetContentRegionAvail(&avail);
					ImGui::SetNextItemWidth(avail.x - 160.f);
					ImGui::InputTextWithHint("##MorphFilter", "Filter by name or key...", s_morphFilter, sizeof(s_morphFilter));
					ImGui::SameLine();
					if (ImGui::Button("Refresh##M", { 70.f, 0.f })) {
						s_morphState.Refresh(selectedActor);
					}
					ImGui::SameLine();
					ImUtil::ButtonStyle_Green();
					if (ImGui::Button("Apply##M", { 70.f, 0.f })) {
						SKEE::Morphs::Apply(selectedActor);
					}
					ImUtil::ButtonStyle_Reset();
				}

				ImGui::Spacing();

				// ---- Table -------------------------------------------------
				{

					if (s_morphState.Entries.empty()) {
						ImGui::TextDisabled("No morphs found on this actor.");
					}
					else {
						std::vector<std::size_t> filtered;
						filtered.reserve(s_morphState.Entries.size());
						for (std::size_t i = 0; i < s_morphState.Entries.size(); ++i) {
							const auto& e = s_morphState.Entries[i];
							if (PassesFilter(e.MorphName, s_morphFilter) ||
								PassesFilter(e.Key, s_morphFilter)) {
								filtered.push_back(i);
							}
						}

						if (filtered.empty()) {
							ImGui::TextDisabled("No morphs match the filter.");
						}
						else {
							constexpr ImGuiTableFlags kTableFlags =
								ImGuiTableFlags_BordersInnerV     |
								ImGuiTableFlags_RowBg             |
								ImGuiTableFlags_ScrollY           |
								ImGuiTableFlags_SizingStretchProp |
								ImGuiTableFlags_Sortable          |
								ImGuiTableFlags_SortMulti;

							const float rowH = ImGui::GetTextLineHeightWithSpacing();
							const float tableH = std::min(static_cast<float>(filtered.size()) * rowH + rowH + 4.f, 300.f);

							bool needRefresh = false;

							if (ImGui::BeginTable("##MorphTable", 4, kTableFlags, { 0.f, tableH })) {
								ImGui::TableSetupScrollFreeze(0, 1);
								ImGui::TableSetupColumn("Morph", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.45f, static_cast<ImGuiID>(MorphColumn::MorphName));
								ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.25f, static_cast<ImGuiID>(MorphColumn::Key));
								ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.25f, static_cast<ImGuiID>(MorphColumn::Value));
								ImGui::TableSetupColumn("##Del", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 28.f);
								ImGui::TableHeadersRow();

								if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
									SortMorphIndices(filtered, s_morphState, sortSpecs);
									sortSpecs->SpecsDirty = false;
								}

								for (const std::size_t idx : filtered) {
									auto& entry = s_morphState.Entries[idx];
									float& value = s_morphState.WorkingValues[idx];

									ImGui::TableNextRow();
									ImGui::PushID(static_cast<int32_t>(idx));

									ImGui::TableSetColumnIndex(0);
									ImGui::TextUnformatted(entry.MorphName.c_str());

									ImGui::TableSetColumnIndex(1);
									ImGui::TextDisabled("%s", entry.Key.c_str());

									ImGui::TableSetColumnIndex(2);
									ImGui::SetNextItemWidth(-1.f);
									if (ImGui::DragFloat("##v", &value, 0.001f, -10.f, 10.f, "%.4f")) {
										SKEE::Morphs::Set(selectedActor, entry.MorphName.c_str(), value, entry.Key.c_str(), false);
										entry.Value = value;
									}
									if (ImGui::IsItemDeactivatedAfterEdit()) {
										SKEE::Morphs::Apply(selectedActor);
									}

									ImGui::TableSetColumnIndex(3);
									if (ImGui::SmallButton("X")) {
										SKEE::Morphs::Clear(selectedActor, entry.MorphName.c_str(), entry.Key.c_str());
										SKEE::Morphs::Apply(selectedActor);
										needRefresh = true;
									}

									ImGui::PopID();
									if (needRefresh) break;
								}
								ImGui::EndTable();
							}

							if (needRefresh) {
								s_morphState.Refresh(selectedActor);
							}
						}
					}
				}
			}
		}

		ImGui::Spacing();

		// ===================================================================
		// Node Editor
		// ===================================================================

		if (ImGui::CollapsingHeader("Node Transforms", ImGuiTreeNodeFlags_DefaultOpen)) {

			if (!SKEE::Transforms::Loaded()) {
				ImGui::TextDisabled("SKEE NiTransformint32_terface not available.");
			}
			else {
				// ---- Toolbar -----------------------------------------------
				{
					ImVec2 avail;
					ImGui::GetContentRegionAvail(&avail);
					ImGui::SetNextItemWidth(avail.x - 170.f);
					ImGui::InputTextWithHint("##NodeFilter", "Filter by node or key...", s_nodeFilter, sizeof(s_nodeFilter));
					ImGui::SameLine();
					if (ImGui::Button("Refresh##N", { 70.f, 0.f })) {
						s_nodeState.Refresh(selectedActor);
					}
					ImGui::SameLine();
					ImUtil::ButtonStyle_Red();
					if (ImGui::Button("Clear All##N", { 80.f, 0.f })) {
						SKEE::Transforms::RemoveAll(selectedActor);
						s_nodeState.Refresh(selectedActor);
					}
					ImUtil::ButtonStyle_Reset();
				}

				ImGui::Spacing();
				{
					// ---- Table -------------------------------------------------
					if (s_nodeState.Entries.empty()) {
						ImGui::TextDisabled("No node transforms found on this actor.");
					}
					else {
						std::vector<std::size_t> filtered;
						filtered.reserve(s_nodeState.Entries.size());

						for (std::size_t i = 0; i < s_nodeState.Entries.size(); ++i) {
							const auto& e = s_nodeState.Entries[i];
							if (PassesFilter(e.Node, s_nodeFilter) ||
								PassesFilter(e.Key, s_nodeFilter)) {
								filtered.push_back(i);
							}
						}

						if (filtered.empty()) {
							ImGui::TextDisabled("No node transforms match the filter.");
						}
						else {
							constexpr ImGuiTableFlags kTableFlags =
								ImGuiTableFlags_BordersInnerV     |
								ImGuiTableFlags_RowBg             |
								ImGuiTableFlags_ScrollY           |
								ImGuiTableFlags_SizingStretchProp |
								ImGuiTableFlags_Sortable          |
								ImGuiTableFlags_SortMulti;

							const float rowH = ImGui::GetFrameHeightWithSpacing();
							const float tableH = std::min(static_cast<float>(filtered.size()) * rowH + ImGui::GetTextLineHeightWithSpacing() + 4.f, 360.f);

							bool needRefresh = false;

							if (ImGui::BeginTable("##NodeTable", 6, kTableFlags, { 0.f, tableH })) {
								ImGui::TableSetupScrollFreeze(0, 1);
								ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.22f, static_cast<ImGuiID>(NodeColumn::Node));
								ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.18f, static_cast<ImGuiID>(NodeColumn::Key));
								ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch, 0.22f, static_cast<ImGuiID>(NodeColumn::Position));
								ImGui::TableSetupColumn("Rotation", ImGuiTableColumnFlags_WidthStretch, 0.22f, static_cast<ImGuiID>(NodeColumn::Rotation));
								ImGui::TableSetupColumn("Scale", ImGuiTableColumnFlags_WidthStretch, 0.10f, static_cast<ImGuiID>(NodeColumn::Scale));
								ImGui::TableSetupColumn("##Del", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 28.f);
								ImGui::TableHeadersRow();

								if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
									SortNodeIndices(filtered, s_nodeState, sortSpecs);
									sortSpecs->SpecsDirty = false;
								}

								for (const std::size_t idx : filtered) {
									auto& entry = s_nodeState.Entries[idx];
									auto& working = s_nodeState.Working[idx];

									ImGui::TableNextRow();
									ImGui::PushID(static_cast<int32_t>(idx));

									ImGui::TableSetColumnIndex(0);
									ImGui::TextUnformatted(entry.Node.c_str());

									ImGui::TableSetColumnIndex(1);
									ImGui::TextDisabled("%s", entry.Key.c_str());

									ImGui::TableSetColumnIndex(2);
									if (entry.HasPosition) {
										ImGui::SetNextItemWidth(-1.f);
										if (ImGui::DragFloat3("##pos", working.Pos, 0.5f, -100000.f, 100000.f, "%.2f")) {
											SKEE::Transforms::SetPosition(
												selectedActor,
												entry.Node.c_str(),
												entry.Key.c_str(),
												{ working.Pos[0], working.Pos[1], working.Pos[2] }
											);
										}
									}
									else {
										ImGui::TextDisabled("-");
									}

									ImGui::TableSetColumnIndex(3);
									if (entry.HasRotation) {
										ImGui::SetNextItemWidth(-1.f);
										if (ImGui::DragFloat3("##rot", working.Rot, 0.1f, -360.f, 360.f, "%.2f\xc2\xb0")) {
											SKEE::Transforms::SetRotation(
												selectedActor,
												entry.Node.c_str(),
												entry.Key.c_str(),
												{ working.Rot[0], working.Rot[1], working.Rot[2] }
											);
										}
									}
									else {
										ImGui::TextDisabled("-");
									}

									ImGui::TableSetColumnIndex(4);
									if (entry.HasScale) {
										ImGui::SetNextItemWidth(-1.f);
										if (ImGui::DragFloat("##scl", &working.Scale, 0.001f, 0.001f, 100.f, "%.4f")) {
											SKEE::Transforms::SetScale(
												selectedActor,
												entry.Node.c_str(),
												entry.Key.c_str(),
												working.Scale
											);
										}
									}
									else {
										ImGui::TextDisabled("-");
									}

									ImGui::TableSetColumnIndex(5);
									ImUtil::ButtonStyle_Red();
									if (ImGui::SmallButton("X")) {
										SKEE::Transforms::RemoveNode(selectedActor, entry.Node.c_str(), entry.Key.c_str());
										needRefresh = true;
									}
									ImUtil::ButtonStyle_Reset();

									ImGui::PopID();

									if (needRefresh) {
										break;
									}
								}

								ImGui::EndTable();
							}

							if (needRefresh) {
								s_nodeState.Refresh(selectedActor);
							}
						}
					}
				}
			}
		}
	}
}