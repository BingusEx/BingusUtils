#pragma once

#include "SKEEInterface.hpp"

namespace BU::SKEE {

	class Overlays final : public CInitSingleton<Overlays>, public EventListener {

		public:

		class Variant : public SKEEIntfc::IOverrideInterface::SetVariant, public SKEEIntfc::IOverrideInterface::GetVariant {
			public:
			Variant()                                  : _type(Type::None),       _u() {}
			Variant(int32_t a_i)                       : _type(Type::Int),        _u{ a_i } {}
			Variant(float a_f)                         : _type(Type::Float),      _u{ ._flt = a_f } {}
			Variant(const char* a_str)                 : _type(Type::String),     _u{ ._str = a_str } {}
			Variant(bool a_b)                          : _type(Type::Bool),       _u{ ._bool = a_b } {}
			Variant(const RE::BGSTextureSet* a_texset) : _type(Type::TextureSet), _u{ ._texset = a_texset } {}

			Type GetType() override                                       { return _type; }
			int32_t Int() override                                        { return _u._int; }
			float Float() override                                        { return _u._flt; }
			const char* String() override                                 { return _u._str; }
			bool Bool() override                                          { return _u._bool; }
			RE::BGSTextureSet* TextureSet() override                      { return const_cast<RE::BGSTextureSet*>(_u._texset); }
			void Int(const int32_t i) override                            { _type = Type::Int; _u._int = i; }
			void Float(const float f) override                            { _type = Type::Float; _u._flt = f; }
			void String(const char* str) override                         { _type = Type::String; _u._str = str; }
			void Bool(const bool b) override                              { _type = Type::Bool; _u._bool = b; }
			void TextureSet(const RE::BGSTextureSet* textureSet) override { _type = Type::TextureSet; _u._texset = textureSet; }

			private:
			Type _type;
			union _u_t {
				int32_t                  _int;
				float                    _flt;
				bool                     _bool;
				const char* _str;
				const RE::BGSTextureSet* _texset;
			} _u;
		};

		struct Overlay {
			float DiffuseAlpha = 0.0f;
			uint32_t DiffuseColor = 0x000000;
			std::string TexturePath;

			uint8_t BodyPart = 0;
			int16_t Index = 0;
			bool DontOverwrite = false;
		};

		struct BodyPartInfo {
			int16_t Count;
			const char* const FmtName;
			SKEEIntfc::IOverlayInterface::OverlayLocation loc;
		};

		enum class Layers : uint16_t {

			kShaderEmissiveColor = 0,
			kShaderEmissiveMultiple = 1,
			kShaderGlossiness = 2,
			kShaderSpecularStrength = 3,
			kShaderLightingEffect1 = 4,
			kShaderLightingEffect2 = 5,
			kShaderTextureSet = 6,
			kShaderTintColor = 7,
			kShaderAlpha = 8,
			kShaderTexture = 9,

			kControllerStartStop = 20,
			kControllerStartTime = 21,
			kControllerStopTime = 22,
			kControllerFrequency = 23,
			kControllerPhase = 24
		};


		//EventListener
		void OnSKSEDataLoaded() override;

		static void Register();
		[[nodiscard]] static bool Loaded();

		static float GetNodeOverrideFloat(RE::TESObjectREFR* a_ref, bool a_female, const char* a_node, int16_t a_key, uint8_t a_index);
		static int32_t GetNodeOverrideInt(RE::TESObjectREFR* a_ref, bool a_female, const char* a_node, int16_t a_key, uint8_t a_index);
		static RE::BSFixedString GetNodeOverrideString(RE::TESObjectREFR* a_ref, bool a_female, const char* a_node, int16_t a_key, uint8_t a_index);
		static void ClearBodyPart(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart);
		static void RemoveSingle(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart, uint16_t a_idx);
		static void ClearAll(RE::Actor* a_Actor);
		static int16_t GetNumOfUsedSlots(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart);
		static void SetSingle(RE::Actor* a_Actor, const Overlay& a_overlay);
		static std::list<uint16_t> GetUsedSlotIndices(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart);
		static bool BuildOverlayAtIdx(RE::Actor* a_Actor, const BodyPartInfo& a_bodyPart, uint16_t a_idx, Overlay* a_outOverlayObject);


		using Loc = SKEEIntfc::IOverlayInterface::OverlayLocation;
		static inline BodyPartInfo FaceInfo{ 0, "Face [Ovl{}]",   Loc::Face };
		static inline BodyPartInfo BodyInfo{ 0, "Body [Ovl{}]",   Loc::Body };
		static inline BodyPartInfo HandsInfo{ 0, "Hands [Ovl{}]", Loc::Hand };
		static inline BodyPartInfo FeetInfo{ 0, "Feet [Ovl{}]",   Loc::Feet };

	private:
		static inline SKEEIntfc::IOverlayInterface* OverlayInterface = nullptr;
		static inline SKEEIntfc::IOverrideInterface* OverrideInterface = nullptr;



	};

	class Morphs final : public CInitSingleton<Morphs>, public EventListener {

		public:
		//EventListener
		void OnSKSEDataLoaded() override;

		static void Register();
		[[nodiscard]] static bool Loaded();

		static void Set(RE::Actor* a_actor, const char* a_morphName, float a_value, const char* a_key, bool a_immediate = false);
		static float Get(RE::Actor* a_actor, const char* a_morphName, const char* a_key);
		static void ClearAll(RE::Actor* a_actor);
		static void Clear(RE::Actor* a_actor, const char* a_key);
		static void Clear(RE::Actor* a_actor, const char* a_morphName, const char* a_key);
		static void Apply(RE::Actor* a_actor);
		static bool HasKey(RE::Actor* a_actor, const char* a_key);


		private:
		static inline SKEEIntfc::IBodyMorphInterface* MorphInterFace = nullptr;
	};





}
