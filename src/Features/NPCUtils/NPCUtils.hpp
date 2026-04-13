#pragma once

namespace BU::Features {

	class NPCUtils : public CInitSingleton<NPCUtils>, public EventListener, public UI::UIEntry<NPCUtils> {

		friend UI::UIItemRegistry;
		static constexpr std::string_view UICategoryName = "NPC Utils";
		static void Draw();

		virtual void OnSKSEDataLoaded() override;

		struct NPCEntry {
			RE::Actor* actor{};
			RE::FormID formID{};
			std::string name;
			std::string editorID;
			std::string label;      // primary display label  (Name - 0xXXXXXXXX)
			std::string searchText; // concatenated search blob
		};

		using Snapshot = std::vector<NPCEntry>;

		static void StartWorker();
		static void WorkerLoop();
		static void RebuildSnapshot();

		static inline std::shared_mutex               m_snapshotMutex{};
		static inline std::shared_ptr<const Snapshot> m_snapshot{};
		static inline std::atomic<bool>               m_stopWorker{ false };
		static inline std::atomic<bool>               m_dirtyFlag{ true };
		static inline std::thread                     m_workerThread{};
		static inline RE::FormID                      m_selectedNPCFormID{ 0 };
		static inline ImGuiTextFilter                 m_filter{};
	};

}