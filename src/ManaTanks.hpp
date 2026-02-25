#pragma once
#include "Util/Data/Utils/MapRecord.hpp"
#include "Util/Data/Utils/VectorRecord.hpp"

namespace BU::Features {

	struct MorphEntry {
		std::string morphName;
		float scale = 0.0f;
	};

	class ManaTanks :
		public CInitSingleton<ManaTanks>,
		public EventListener,
		public UI::UIEntry<ManaTanks> {

		void OnSerdeSave(SKSE::SerializationInterface* a_this) override;
		void OnSerdeLoad(SKSE::SerializationInterface* a_this) override;
		void OnActorReset(RE::Actor* a_actor) override;
		void OnActorUpdate(RE::Actor* a_actor) override;
		static void RemoveActor(RE::Actor* a_actor);
		static void InvalidateData(RE::Actor* a_actor);



		enum Mode {
			kNone,
			kMT,
			kAbs
		};

		#pragma pack(push, 1)
		struct Data{
			uint8_t CurMode = 1; //0 = None, 1 = Mana Tank, 2 = Absolute Scale Mode
			uint8_t ReqMode = 1;
			uint16_t Reference = 300;
			float ScaleMult = 1.0f;
			float AbsScale = 0.0f;
			float LastKnownValue = 0.0;
		};
		#pragma pack(pop)

		inline static const std::vector<MorphEntry> DefaultEntries = {
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

		//Serialization
		static inline std::mutex _Lock = {};
		static inline Serialization::MapRecord<Data, 'MTAD'> ActorData = {};
		static inline Serialization::VectorRecord<MorphEntry, 'MTDT'> MorphData = {};
		static constexpr std::string_view MorphKey = "BU_ManaTanks";

		public:
		static constexpr std::string_view UICategoryName = "Mana Tanks";
		static void Draw();
	};


}
