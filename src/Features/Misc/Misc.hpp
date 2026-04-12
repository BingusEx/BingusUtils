#pragma once
#include "Util/Data/Utils/MapRecord.hpp"
#include "Util/Data/Utils/VectorRecord.hpp"

namespace BU::Features {

	class Misc : public CInitSingleton<Misc>, public EventListener {

		void OnSKSEDataLoaded() override;
		void OnActorSet3D(RE::Actor* a_actor, RE::NiAVObject* a_object) override;
		void OnMenuChange(const RE::MenuOpenCloseEvent* a_event) override;

	};
}
