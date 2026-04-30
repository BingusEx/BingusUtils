#pragma once
#include "Common/SKEE/SKEE.hpp"
#include "Util/Data/Utils/MapRecord.hpp"

namespace BU::Features {

	class OverlayTools : public CInitSingleton<OverlayTools>, public EventListener, public UI::UIEntry<OverlayTools> {
		public:
		using OverlayEntry = SKEE::Overlays::Overlay;

		static void ClearStoredOverlays(RE::Actor* a_actor);
		static void ClearGameOverlays(RE::Actor* a_actor);
		static void BuildOverlayList(RE::Actor* a_actor);
		static void ApplyStoredOvls(RE::Actor* a_actor);
		static void AddNewActor(RE::Actor* a_actor);
		static void DrawOverlayEntry(RE::Actor* a_actor);

		void OnSerdeSave(SKSE::SerializationInterface* a_this) override;
		void OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) override;
		void OnSerdePostLoad() override;

		void OnActorReset(RE::Actor* a_actor) override;
		void OnActorUpdate(RE::Actor* a_actor) override;
		void OnActorLoad3D(RE::Actor* a_actor) override;
		void OnActorEquip(RE::Actor* a_actor) override;
		void OnActorUnequip(RE::Actor* a_actor) override;

		static void RemoveActor(RE::Actor* a_actor);
		static void InvalidateAndApplyToActor(RE::Actor* a_actor);

		struct Data {
			std::vector<OverlayEntry> OvlFace = {};
			std::vector<OverlayEntry> OvlBody = {};
			std::vector<OverlayEntry> OvlHands = {};
			std::vector<OverlayEntry> OvlFeet = {};

			bool AlreadyApplied = false;
		};

		static inline std::mutex m_mutex;
		static inline Serialization::MapRecord<Data, 'OUAD'> ActorData = {};

		friend UI::UIItemRegistry;
		static constexpr std::string_view UICategoryName = "Overlay Saver";
		static void Draw();
	};
}