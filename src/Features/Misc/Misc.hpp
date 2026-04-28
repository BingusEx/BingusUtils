#pragma once
#include "Util/Data/Utils/MapRecord.hpp"
#include "Util/Data/Utils/VectorRecord.hpp"

namespace BU::Features {

	class Misc : public CInitSingleton<Misc>, public EventListener/*, public UI::UIEntry<Misc>*/ {

		//void OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) override;
		//void OnSerdeSave(SKSE::SerializationInterface* a_this) override;

		void OnSKSEDataLoaded() override;
		void OnUpdate() override;
		void OnActorSet3D(RE::Actor* a_actor, RE::NiAVObject* a_object) override;
		void OnMenuChange(const RE::MenuOpenCloseEvent* a_event) override;

		//friend UI::UIItemRegistry;
		//static constexpr std::string_view UICategoryName = "Misc";
		//static void Draw();

	};
}
