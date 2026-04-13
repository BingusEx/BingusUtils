#include "Features/NPCUtils/NPCUtils.hpp"
#include "Common/Utilities.hpp"

namespace BU::Features {

	void NPCUtils::StartWorker() {
		m_stopWorker.store(false, std::memory_order_release);
		m_dirtyFlag.store(true, std::memory_order_release);
		m_workerThread = std::thread(&NPCUtils::WorkerLoop);
	}

	void NPCUtils::WorkerLoop() {
		while (!m_stopWorker.load(std::memory_order_acquire)) {

			// Sleep until the dirty flag is set (or we're asked to stop).
			m_dirtyFlag.wait(false, std::memory_order_acquire);

			if (m_stopWorker.load(std::memory_order_acquire)) {
				break;
			}

			// Consume the flag before scanning so any request that arrives
			// mid-scan isn't silently dropped — it will trigger another pass.
			m_dirtyFlag.store(false, std::memory_order_release);

			RebuildSnapshot();
		}
	}

	void NPCUtils::RebuildSnapshot() {
		std::shared_ptr<std::vector<NPCEntry>> newSnapshot = std::make_shared<Snapshot>();

		const auto& [map, lock] = RE::TESForm::GetAllForms();

		for (auto& [id, form] : *map) {
			if (form->GetSavedFormType() != RE::FormType::ActorCharacter) {
				continue;
			}

			RE::Actor* actor = form->As<RE::Actor>();
			if (!actor) {
				continue;
			}

			const char* rawEditorID = actor->GetFormEditorID();
			const std::string editorID = rawEditorID ? rawEditorID : "";
			std::string name = actor->GetDisplayFullName();

			if (name.empty()) {
				name = BU::Utils::GetFormName(actor);
			}

			std::string label;
			if (!name.empty()) {
				label = std::format("{} - 0x{:08X}", name, actor->GetFormID());
			}
			else if (!editorID.empty()) {
				label = std::format("{} - 0x{:08X}", editorID, actor->GetFormID());
			}
			else {
				label = std::format("0x{:08X}", actor->GetFormID());
			}

			const std::string searchText = std::format("{} {} {:08X}", name, editorID, actor->GetFormID());

			newSnapshot->push_back(NPCEntry{
				.actor = actor,
				.formID = actor->GetFormID(),
				.name = std::move(name),
				.editorID = std::move(editorID),
				.label = std::move(label),
				.searchText = std::move(searchText)
			});
		}

		// Publish
		std::unique_lock writeLock(m_snapshotMutex);
		m_snapshot = std::move(newSnapshot);
	}

	void NPCUtils::Draw() {

		std::shared_ptr<const Snapshot> snapshot;
		{
			std::shared_lock readLock(m_snapshotMutex);
			snapshot = m_snapshot;
		}


		if (ImGui::Button("Refresh")) {
			m_dirtyFlag.store(true, std::memory_order_release);
			m_dirtyFlag.notify_one();
		}

		ImGui::SameLine();

		m_filter.Draw("Search Instanced Actors", 400.0f);

		if (!snapshot) {
			ImGui::TextDisabled("Building NPC list...");
			return;
		}

		std::vector<const NPCEntry*> filtered;
		filtered.reserve(snapshot->size());

		for (const NPCEntry& entry : *snapshot) {
			if (m_filter.PassFilter(entry.searchText.c_str())) {
				filtered.push_back(&entry);
			}
		}

		if (filtered.empty()) {
			ImGui::TextDisabled("No match found.");
			return;
		}

		int selectedIndex = 0;
		for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
			if (filtered[i]->formID == m_selectedNPCFormID) {
				selectedIndex = i;
				break;
			}
		}
		m_selectedNPCFormID = filtered[selectedIndex]->formID;

		// Combobox Size Set
		{
			ImVec2 contentWidth, title_textSize;
			ImGui::GetContentRegionAvail(&contentWidth);
			ImGui::CalcTextSize(&title_textSize, "Actor List", nullptr, true, -1.0f);
			ImGui::SetNextItemWidth(contentWidth.x - title_textSize.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
		}

		if (ImGui::BeginCombo("Actor List", filtered[selectedIndex]->label.c_str())) {
			for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
				const NPCEntry& entry = *filtered[i];
				const bool      isSelected = (i == selectedIndex);
				const std::string comboLabel =
					std::format("{}##{:08X}", entry.label, entry.formID);

				if (ImGui::Selectable(comboLabel.c_str(), isSelected)) {
					selectedIndex = i;
					m_selectedNPCFormID = entry.formID;
				}

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Name: %s\n"
						"Editor ID: %s\n"
						"Reference ID: %08X",
						entry.name.empty() ? "[None]" : entry.name.c_str(),
						entry.editorID.empty() ? "[None]" : entry.editorID.c_str(),
						entry.formID
					);
				}

				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		const NPCEntry& selected = *filtered[selectedIndex];
		RE::Actor* actor = selected.actor;

		if (!actor) {
			return;
		}

		ImGui::Spacing();

		{
			const std::string formIDText = std::format("{:08X}", selected.formID);
			const char* displayName = selected.name.empty() ? "[None]" : selected.name.c_str();
			const char* editorID = selected.editorID.empty() ? "[None]" : selected.editorID.c_str();

			ImGui::TextUnformatted("Name:");
			ImGui::SameLine();
			ImGui::TextUnformatted(displayName);

			ImGui::TextUnformatted("Editor ID:");
			ImGui::SameLine();
			ImGui::TextUnformatted(editorID);
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy##EditorID")) {
				ImGui::SetClipboardText(selected.editorID.c_str());
			}

			ImGui::TextUnformatted("Reference ID:");
			ImGui::SameLine();
			ImGui::TextUnformatted(formIDText.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy##FormID")) {
				ImGui::SetClipboardText(formIDText.c_str());
			}
		}

		ImGui::Spacing();

		{
			bool enabled = !actor->IsDisabled();
			if (ImGui::Checkbox("Enabled", &enabled)) {
				if (enabled) {
					actor->Enable(false);
				}
				else {
					actor->Disable();
				}

				// Actor state changed — trigger a fresh snapshot.
				m_dirtyFlag.store(true, std::memory_order_release);
				m_dirtyFlag.notify_one();
			}
		}

		{
			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
			if (player) {
				if (ImGui::Button("Go To")) {
					player->MoveTo(actor);
				}
				ImGui::SameLine();
				if (ImGui::Button("Move Here")) {
					actor->MoveTo(player);
				}
			}
		}
	}

	void NPCUtils::OnSKSEDataLoaded() {
		StartWorker();
	}
}
