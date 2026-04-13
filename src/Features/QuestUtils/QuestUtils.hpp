#pragma once

namespace BU::Features {

	class QuestUtils : public CInitSingleton<QuestUtils>, public EventListener, public UI::UIEntry<QuestUtils> {


		friend UI::UIItemRegistry;
		static constexpr std::string_view UICategoryName = "Quest Utils";
		void OnSKSEDataLoaded() override;
		static void Draw();


	};
}
