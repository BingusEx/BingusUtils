#pragma once
#include "Common/SKEE/SKEE.hpp"

#include "Util/Data/Utils/MapRecord.hpp"
#include "Util/Data/Utils/VectorRecord.hpp"

namespace BU::Features {

	class MorphAdjust : public CInitSingleton<MorphAdjust>, public EventListener, public UI::UIEntry<MorphAdjust> {

		void OnSerdeSave(SKSE::SerializationInterface* a_this) override;
		void OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) override;
		void OnSerdePostLoad() override;
		void OnActorReset(RE::Actor* a_actor) override;
		void OnActorUpdate(RE::Actor* a_actor) override;
		void OnSKSEDataLoaded() override;
		static void InvalidateData(RE::Actor* a_actor);

		static void RemoveActor(RE::Actor* a_actor);

		struct Data {
			bool Enabled = true;
			bool NeedsUpdate = false;
			float RotationUpperMult = 5.0f; //Defaults
			float RotationLowerMult = 5.0f; //Defaults
			float CachedWeights = 0.0f;
		};

		struct MorphScalar {
			std::string morphName;
			float weight = 0.0f;
		};

		private:

		static inline const std::vector<std::string> m_defBonesLL {
			{"CME L Hip"},
		};

		static inline const std::vector<std::string> m_defBonesUL {
			{"CME L Clavicle [LClv]"},
			{"CME L Shoulder"},
		};

		static inline const std::vector<std::string> m_DefBonesLR {
			{"CME R Hip"},
		};

		static inline const std::vector<std::string> m_defBonesUR {
			{"CME R Clavicle [RClv]"},
			{"CME R Shoulder"},
		};

		static inline const std::vector<MorphScalar> m_defScalarsU {
			{"SSBBW2 body",   0.5f},
			{"SSBBW3 body",   0.5f},
			{"BigTorso",      0.3f},
			{"ChubbyWaist",   0.3f},
			{"ChubbyButt",    0.3f},
			{"Hips",          0.2f},
			{"SSBBW_Muscled", 0.4f},
			{"BBW2",          0.8f},
		};

		static inline const std::vector<MorphScalar> m_defScalarsL {
			{"SSBBW2 body",          0.5f},
			{"SSBBW3 body",          0.5f},
			{"Thighs",               0.3f},
			{"ChubbyLegs",           0.3f},
			{"SlimThighs",          -0.3f},
			{"ThighsInsideThicc_v2", 0.3f},
			{"LegsSpread_v2",       -0.4f},
			{"SSBBW_Muscled",        0.4f},
			{"BBW2",                 0.8f},
		};

		static inline std::vector<SKEE::Morphs::MorphEntry> m_scratchEntries;

		static inline std::mutex _Lock;
		static constexpr std::string_view                              MorphKey = "BU_MorphAdjust";
		static inline Serialization::MapRecord<Data, 'MAAD'>           ActorData = {};
		static inline Serialization::VectorRecord<MorphScalar, 'MASU'> ScalarDataUpper = {};
		static inline Serialization::VectorRecord<MorphScalar, 'MASL'> ScalarDataLower = {};

		friend UI::UIItemRegistry;
		static constexpr std::string_view UICategoryName = "Morph Adjust";
		static void Draw();

	};
}
