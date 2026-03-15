#pragma once
#include <RE/B/BSTEvent.h>

namespace BU {

	class EventListener;

	class EventDispatcher : public CInitSingleton<EventDispatcher>, 
	public RE::BSTEventSink<RE::TESResetEvent>, 
	public RE::BSTEventSink<RE::MenuOpenCloseEvent> {

		public:
		static void Init(uint32_t a_serdeID);
		static void AddListener(EventListener* a_listener);
		static void RemoveListener(EventListener* a_listener);
		static void Compact();

        //Update
        static void DispatchUpdate();

        //Actor
        static void DispatchActorUpdate(RE::Actor* a_actor);
        static void DispatchActorLoad3D(RE::Actor* a_actor);
        static void DispatchActorReset(RE::Actor* a_actor);
        static void DispatchActorEquipEvent(RE::Actor* a_actor);
		static void DispatchActorUnEquipEvent(RE::Actor* a_actor);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESResetEvent* evn, RE::BSTEventSource<RE::TESResetEvent>* dispatcher) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;

	private:
        //SKSE
        static void SKSEDispatch(SKSE::MessagingInterface::Message* a_message);
        static void SerdeDispatchFormDelete(RE::VMHandle a_callback);
        static void SerdeDispatchLoad(SKSE::SerializationInterface* a_this);
        static void SerdeDispatchSave(SKSE::SerializationInterface* a_this);
        static void SerdeDispatchRevert(SKSE::SerializationInterface* a_this);

        struct ListenerEntry {
            std::atomic<EventListener*> ptr{ nullptr };

            ListenerEntry() = default;
            explicit ListenerEntry(EventListener* p) : ptr(p) {}

            ListenerEntry(const ListenerEntry& other) : ptr(other.ptr.load(std::memory_order_relaxed)) {}
            ListenerEntry& operator=(const ListenerEntry& other) {
                ptr.store(other.ptr.load(std::memory_order_relaxed), std::memory_order_relaxed);
                return *this;
            }

            ListenerEntry(ListenerEntry&& other) noexcept : ptr(other.ptr.exchange(nullptr, std::memory_order_relaxed)) {}
            ListenerEntry& operator=(ListenerEntry&& other) noexcept {
                ptr.store(other.ptr.exchange(nullptr, std::memory_order_relaxed), std::memory_order_relaxed);
                return *this;
            }
        };

        static inline std::mutex m_lock;
        static inline tbb::concurrent_vector<ListenerEntry> m_listeners;

        template <typename Func>
        static void ForEachListener(Func&& func) {
            for (auto& entry : m_listeners) {
                if (EventListener* listener = entry.ptr.load(std::memory_order_acquire)) {
                    func(listener);
                }
            }
        }

        template <typename Func>
        static void ForEachListenerConcurrent(Func&& func) {
            tbb::parallel_for_each(m_listeners.begin(), m_listeners.end(), [&](auto& entry) {
                if (EventListener* listener = entry.ptr.load(std::memory_order_acquire)) {
                    func(listener);
                }
            });
        }
	};
}