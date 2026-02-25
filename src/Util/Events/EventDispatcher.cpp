#include "Util/Events/EventDispatcher.hpp"
#include "Util/Events/EventListener.hpp"

#include "REX/W32/DBGHELP.h"

namespace {

	std::string GetPrettyTypeName(const std::type_info& ti) {
		char buffer[1024];
		if (REX::W32::UnDecorateSymbolName(
			ti.name(),
			buffer,
			sizeof(buffer),
			REX::W32::UNDNAME_NAME_ONLY)) {
			return buffer;
		}
		return ti.name();
	}

}

namespace BU {

	void EventDispatcher::AddListener(EventListener* a_listener) {
		if (!a_listener) return;

		{
			std::lock_guard lock(m_lock);
			logger::trace("Adding Listener: {}", GetPrettyTypeName(typeid(a_listener)));
			m_listeners.push_back(ListenerEntry(a_listener));
		}
	}

	void EventDispatcher::RemoveListener(EventListener* a_listener) {
		if (!a_listener) return;

		{
			std::lock_guard lock(m_lock);

			for (auto& entry : m_listeners) {
				EventListener* p = entry.ptr.load(std::memory_order_relaxed);
				if (p == a_listener) {
					logger::trace("Deleting Listener: {}", GetPrettyTypeName(typeid(a_listener)));
					entry.ptr.store(nullptr, std::memory_order_release);
				}
			}
		}
	}

	void EventDispatcher::Compact() {
		std::lock_guard lock(m_lock);
		tbb::concurrent_vector<ListenerEntry> compacted;
		compacted.reserve(m_listeners.size());

		for (auto& entry : m_listeners) {
			if (EventListener* p = entry.ptr.load(std::memory_order_relaxed)) {
				compacted.push_back(ListenerEntry(p));
			}
		}

		m_listeners.swap(compacted);
	}

	void EventDispatcher::Init(uint32_t a_serdeID) {

		if (const SKSE::MessagingInterface* mi = SKSE::GetMessagingInterface()) {
			logger::trace("Registering SKSE Messaging Interface Listener");
			mi->RegisterListener(SKSEDispatch);
		}

		if (const SKSE::SerializationInterface* serde = SKSE::GetSerializationInterface()) {
			logger::trace("Registering SKSE Serialization Interface Callbacks with ID: {}", a_serdeID);

			serde->SetUniqueID(a_serdeID);

			serde->SetLoadCallback(SerdeDispatchLoad);
			serde->SetSaveCallback(SerdeDispatchSave);
			serde->SetRevertCallback(SerdeDispatchRevert);
			serde->SetFormDeleteCallback(SerdeDispatchFormDelete);

		}

		if (RE::ScriptEventSourceHolder* evtSrcHolder = RE::ScriptEventSourceHolder::GetSingleton()) {

			logger::trace("Registering GameEvents");

			evtSrcHolder->AddEventSink<RE::TESResetEvent>(&GetSingleton());
		}

	}

	void EventDispatcher::SKSEDispatch(SKSE::MessagingInterface::Message* a_message) {

		switch (a_message->type) {

			// Called after all plugins have finished running SKSEPluginLoad.
			case SKSE::MessagingInterface::kPostLoad: 
			{
				ForEachListener([](EventListener* a_lst) {a_lst->OnSKSEPostLoad();});
				break;
			}

			// Called after all kPostLoad message handlers have run.
			case SKSE::MessagingInterface::kPostPostLoad: 
			{
				ForEachListener([](EventListener* a_lst) {a_lst->OnSKSEPostPostLoad();});
				break;
			}
			
			// Called when all game data has been found.
			case SKSE::MessagingInterface::kInputLoaded: 
			{
				ForEachListener([](EventListener* a_lst) {a_lst->OnSKSEInputLoaded();});
				break;
			}

			// All ESM/ESL/ESP plugins have loaded, main menu is now active.
			case SKSE::MessagingInterface::kDataLoaded: 
			{
				ForEachListener([](EventListener* a_lst) {a_lst->OnSKSEDataLoaded();});
				break;
			}


			// Player's selected save game has finished loading.
			case SKSE::MessagingInterface::kPostLoadGame: 
			{
				ForEachListener([](EventListener* a_lst) {a_lst->OnSKSEPostLoadGame();});
				break;
			}
			
			// Player starts a new game from main menu.
			case SKSE::MessagingInterface::kNewGame: 
			{
				ForEachListener([](EventListener* a_lst) {a_lst->OnSKSENewGame();});
				break;
			}

			// Player selected a game to load, but it hasn't loaded yet, data will be the name of the loaded save.
			case SKSE::MessagingInterface::kPreLoadGame: 
			{
				ForEachListener([](EventListener* a_lst) {a_lst->OnSKSEPreLoadGame(); });
				break;
			}

			// The player has saved a game.
			case SKSE::MessagingInterface::kSaveGame: 
			{
				ForEachListener([](EventListener* a_lst) {a_lst->OnSKSESaveGame(); });
				break;
			}

			// The player deleted a saved game from within the load menu, data will be the save name.
			case SKSE::MessagingInterface::kDeleteGame: 
			{
				ForEachListener([](EventListener* a_lst) {a_lst->OnSKSEDeleteGame(); });
				break;
			}

			default: 
			{
				logger::trace("Received unhandled SKSE message: {}", a_message->type);
				break;
			}
		}
	}

	void EventDispatcher::SerdeDispatchFormDelete(RE::VMHandle a_callback) {
		ForEachListener([](EventListener* a_lst)           {a_lst->OnSerdePreFormDelete();});
		ForEachListener([a_callback](EventListener* a_lst) {a_lst->OnSerdeFormDelete(a_callback);});
		ForEachListener([](EventListener* a_lst)           {a_lst->OnSerdePostFormDelete();});
	}

	void EventDispatcher::SerdeDispatchLoad(SKSE::SerializationInterface* a_this) {

		ForEachListener([](EventListener* a_lst)       {a_lst->OnSerdePreLoad();});

		std::uint32_t type, version, size;

		while (a_this->GetNextRecordInfo(type, version, size)) {
			ForEachListener([&](EventListener* a_lst) {
				a_lst->OnSerdeLoad(a_this, type, version, size);
			});
		}

		ForEachListener([](EventListener* a_lst)       {a_lst->OnSerdePostLoad();});

	}

	void EventDispatcher::SerdeDispatchSave(SKSE::SerializationInterface* a_this) {
		ForEachListener([](EventListener* a_lst)       {a_lst->OnSerdePreSave();});
		ForEachListener([a_this](EventListener* a_lst) {a_lst->OnSerdeSave(a_this);});
		ForEachListener([](EventListener* a_lst)       {a_lst->OnSerdePostSave();});
	}

	void EventDispatcher::SerdeDispatchRevert(SKSE::SerializationInterface* a_this) {
		ForEachListener([](EventListener* a_lst)	   {a_lst->OnSerdePreRevert();});
		ForEachListener([a_this](EventListener* a_lst) {a_lst->OnSerdeRevert(a_this);});
		ForEachListener([](EventListener* a_lst)       {a_lst->OnSerdePostRevert();});
	}

	void EventDispatcher::DispatchUpdate() {
		ForEachListener([](EventListener* a_lst) {a_lst->OnUpdate();});
	}

	void EventDispatcher::DispatchActorUpdate(RE::Actor* a_actor) {
		ForEachListener([a_actor](EventListener* a_lst) {a_lst->OnActorUpdate(a_actor);});
	}

	void EventDispatcher::DispatchActorLoad3D(RE::Actor* a_actor) {
		ForEachListener([a_actor](EventListener* a_lst) {a_lst->OnActorLoad3D(a_actor);});
	}

	void EventDispatcher::DispatchActorReset(RE::Actor* a_actor) {
		ForEachListener([a_actor](EventListener* a_lst) {a_lst->OnActorReset(a_actor);});
	}

	void EventDispatcher::DispatchActorEquipEvent(RE::Actor* a_actor) {
		ForEachListener([a_actor](EventListener* a_lst) {a_lst->OnActorEquip(a_actor); });
	}

	void EventDispatcher::DispatchActorUnEquipEvent(RE::Actor* a_actor) {
		ForEachListener([a_actor](EventListener* a_lst) {a_lst->OnActorUnequip(a_actor); });
	}

	RE::BSEventNotifyControl EventDispatcher::ProcessEvent(const RE::TESResetEvent* evn, RE::BSTEventSource<RE::TESResetEvent>* dispatcher) {
		if (evn) {
			if (RE::TESObjectREFR* object = evn->object.get()) {
				if (RE::Actor* actor = RE::TESForm::LookupByID<RE::Actor>(object->formID)) {
					ForEachListener([actor](EventListener* a_lst) { a_lst->OnActorUnequip(actor); });
				}
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
