#pragma once

namespace BU::Features {

	class RMInspector : public CInitSingleton<RMInspector>, public EventListener, public UI::UIEntry<RMInspector> {

		friend UI::UIItemRegistry;
		static constexpr std::string_view UICategoryName = "RaceMenu Inspector";
		static void Draw();
	};
}