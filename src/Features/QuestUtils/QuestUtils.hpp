#pragma once
#include "Util/Data/Utils/MapRecord.hpp"
#include "Util/Data/Utils/VectorRecord.hpp"

namespace BU::Features {

	class QuestUtils : public CInitSingleton<QuestUtils>, public EventListener, public UI::UIEntry<QuestUtils> {


		friend UI::UIItemRegistry;
		static constexpr std::string_view UICategoryName = "QuestUtils";
		void OnSKSEDataLoaded() override;
		static void Draw();


	};
}
