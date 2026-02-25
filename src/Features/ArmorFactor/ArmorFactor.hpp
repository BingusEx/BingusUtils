#pragma once
#include "Util/Data/Utils/MapRecord.hpp"
#include "Util/Data/Utils/VectorRecord.hpp"

namespace BU::Features {

	class ArmorFactor : public CInitSingleton<ArmorFactor>, public EventListener, public UI::UIEntry<ArmorFactor> {

		void OnSerdeSave(SKSE::SerializationInterface* a_this) override;
		void OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) override;
		void OnSerdePostLoad() override;
		void OnActorReset(RE::Actor* a_actor) override;
		void OnActorUpdate(RE::Actor* a_actor) override;
		void OnActorEquip(RE::Actor* a_actor) override;
		void OnActorUnequip(RE::Actor* a_actor) override;

		static void RemoveActor(RE::Actor* a_actor);
		static void InvalidateData(RE::Actor* a_actor);

		struct Data {
			bool Enabled = false;
			float ScaleMult = 1.0f;
			bool NeedsUpdate = false;
		};

		struct MorphEntry {
			std::string morphName;
			float scale = 0.0f;
		};

		static inline std::vector<MorphEntry> DefaultEntries {
			{"PushUp",          0.45f},
			{"BreastsTogether", 0.75f},
			{"BreastCleavage",   1.5f},
		};


		static inline std::mutex _Lock;
		static constexpr std::string_view MorphKey = "BU_ArmorFactor";
		static inline Serialization::MapRecord<Data, 'AFAD'> ActorData = {};
		static inline Serialization::VectorRecord<MorphEntry, 'AFMD'> MorphData = {};
		static inline Serialization::BasicRecord<std::string, 'AFRG'> ValidNameRegex = {};

		friend UI::UIItemRegistry;
		static constexpr std::string_view UICategoryName = "Armor Factor";
		static void Draw();

	};
}
