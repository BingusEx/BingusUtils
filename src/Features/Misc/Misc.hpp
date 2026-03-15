#pragma once
#include "Util/Data/Utils/MapRecord.hpp"
#include "Util/Data/Utils/VectorRecord.hpp"

namespace BU::Features {

	class Misc : public CInitSingleton<Misc>, public EventListener {


		void OnSerdeSave(SKSE::SerializationInterface* a_this) override;
		void OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) override;
		void OnActorReset(RE::Actor* a_actor) override;
		void OnActorUpdate(RE::Actor* a_actor) override;
		void OnSerdePostLoad() override;


		void OnSKSEDataLoaded() override;
		void OnActorLoad3D(RE::Actor* a_actor) override;
		void OnMenuChange(const RE::MenuOpenCloseEvent* a_event) override;


	};
}
