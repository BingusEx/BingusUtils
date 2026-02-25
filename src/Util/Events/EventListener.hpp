#pragma once

namespace BU {

	class EventDispatcher;

	class EventListener {
	public:
		EventListener() = default;
		virtual ~EventListener() = default;
		EventListener(EventListener const&) = delete;
		EventListener& operator=(EventListener const&) = delete;

		//Update
		virtual void OnUpdate() {}

		//Actor
		virtual void OnActorUpdate(RE::Actor* a_actor) {}
		virtual void OnActorLoad3D(RE::Actor* a_actor) {}
		virtual void OnActorReset(RE::Actor* a_actor) {}
		virtual void OnActorEquip(RE::Actor* a_actor) {}
		virtual void OnActorUnequip(RE::Actor* a_actor) {}

		//SKSESerialization
		virtual void OnSerdePreSave() {}
		virtual void OnSerdeSave(SKSE::SerializationInterface* a_this) {}
		virtual void OnSerdePostSave() {}

		virtual void OnSerdePreLoad() {}
		virtual void OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {}
		virtual void OnSerdePostLoad() {}

		virtual void OnSerdePreRevert() {}
		virtual void OnSerdeRevert(SKSE::SerializationInterface* a_this) {}
		virtual void OnSerdePostRevert() {}

		virtual void OnSerdePreFormDelete() {}
		virtual void OnSerdeFormDelete(RE::VMHandle a_handle) {}
		virtual void OnSerdePostFormDelete() {}


		//SKSE
		virtual void OnSKSEPostLoad() {}
		virtual void OnSKSEPostPostLoad() {}
		virtual void OnSKSEInputLoaded() {}
		virtual void OnSKSEDataLoaded() {}
		virtual void OnSKSEPostLoadGame() {}
		virtual void OnSKSENewGame() {}
		virtual void OnSKSEPreLoadGame() {}
		virtual void OnSKSESaveGame() {}
		virtual void OnSKSEDeleteGame() {}

	};

} // namespace BU
