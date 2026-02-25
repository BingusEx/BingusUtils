#pragma once
#include "Util/Data/Utils/MapRecord.hpp"
#include "Util/Data/Utils/VectorRecord.hpp"

namespace BU::Features {

	class ManaTanks : public CInitSingleton<ManaTanks>, public EventListener, public UI::UIEntry<ManaTanks> {

		enum Mode {
			kNone,
			kMT,
			kAbs
		};

		void OnSerdeSave(SKSE::SerializationInterface* a_this) override;
		void OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) override;
		void OnActorReset(RE::Actor* a_actor) override;
		void OnActorUpdate(RE::Actor* a_actor) override;
		void OnSerdePostLoad() override;

		static void RemoveActor(RE::Actor* a_actor);
		static void InvalidateData(RE::Actor* a_actor);

		struct Data {
			uint8_t CurMode;
			uint8_t ReqMode;
			float LastKnownValue = -1.0f;
			uint16_t Reference = 300;
			float ScaleMult = 1.0f;
			float AbsScale = 1.0f;
		};

		struct MorphEntry {
			std::string morphName;
			float scale = 0.0f;
		};

		static inline std::vector<MorphEntry> DefaultEntries{
			{"Breasts",             0.15f },
			{"BreastsFantasy",      0.20f },
			{"BreastsNewSH",        0.10f },
			{"NippleSize",         -0.45f },
			{"SSBBW2 Boobs Growth", 0.06f },
			{"BreastClevage",       0.15f },
			{"BreastsTogether",     0.05f },
			{"NipplePuffy_v2",      0.10f },
			{"AreolaPull_v2",       0.10f },
			{"NippleThicc_v2",      0.10f },
			{"BreastCenterBig",     0.10f },
			{"BreastsConverage_v2",-0.15f },
			{"BreastWidth",         0.25f },
			{"NipplePerkiness",     0.20f },
			{"BreastGravity2",      0.35f },
		};


		static inline std::mutex _Lock;
		static constexpr std::string_view MorphKey = "BU_ManaTanks";
		static inline Serialization::MapRecord<Data, 'MTAD'> ActorData = {};
		static inline Serialization::VectorRecord<MorphEntry, 'MTME'> MorphData = {};
		static inline Serialization::BasicRecord<float, 'MTDV'> EPS = {};

		friend UI::UIItemRegistry;
		static constexpr std::string_view UICategoryName = "Mana Tanks";
		static void Draw();

	};
}
