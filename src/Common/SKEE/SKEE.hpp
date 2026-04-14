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

		// Visitor that collects all (morphName, key, value) triples for an actor.
		struct MorphEntry {
			std::string MorphName;
			std::string Key;
			float       Value = 0.f;
		};

		// Concrete visitor used to harvest every morph/key/value triple.
		class MorphValueCollector final : public SKEEIntfc::IBodyMorphInterface::MorphValueVisitor {
			public:
			std::vector<MorphEntry> Entries;
			void Visit(RE::TESObjectREFR*, const char* morphName, const char* key, float value) override {
				Entries.push_back({ morphName, key, value });
			}
		};

		// Concrete visitor that collects all morph names for an actor.
		class MorphNameCollector final : public SKEEIntfc::IBodyMorphInterface::MorphVisitor {
			public:
			std::vector<std::string> Names;
			void Visit(RE::TESObjectREFR*, const char* morphName) override {
				Names.emplace_back(morphName);
			}
		};

		// Concrete visitor that collects all keys for a given morph name.
		class MorphKeyCollector final : public SKEEIntfc::IBodyMorphInterface::MorphKeyVisitor {
			public:
			std::vector<std::pair<std::string, float>> Entries; // {key, value}
			void Visit(const char* key, float value) override {
				Entries.emplace_back(key, value);
			}
		};

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

		// Collects every (morphName, key, value) triple the actor currently has.
		static std::vector<MorphEntry> CollectAll(RE::Actor* a_actor);

		// Collects all morph names on an actor.
		static std::vector<std::string> CollectMorphNames(RE::Actor* a_actor);

		// Collects all keys+values for a given morph name on an actor.
		static std::vector<std::pair<std::string, float>> CollectKeysForMorph(RE::Actor* a_actor, const char* a_morphName);


		private:
		static inline SKEEIntfc::IBodyMorphInterface* MorphInterFace = nullptr;
	};


	// Wrapper for INiTransformInterface – skeleton node transform overrides.
	class Transforms final : public CInitSingleton<Transforms>, public EventListener {

		public:
		using Position = SKEEIntfc::INiTransformInterface::Position;
		using Rotation = SKEEIntfc::INiTransformInterface::Rotation;

		// Entry produced by the node visitor.
		struct NodeEntry {
			std::string Node;
			std::string Key;

			bool HasPosition = false;
			bool HasRotation = false;
			bool HasScale = false;

			Position Pos = {};
			Rotation Rot = {};
			float    Scale = 1.f;
		};

		// Concrete NodeVisitor that harvests all entries for an actor.
		class NodeCollector final : public SKEEIntfc::INiTransformInterface::NodeVisitor {
			public:
			std::vector<NodeEntry> Entries;

			bool VisitPosition(const char* node, const char* key, Position& pos) override {
				GetOrCreate(node, key).Pos = pos;
				GetOrCreate(node, key).HasPosition = true;
				return false;
			}

			bool VisitRotation(const char* node, const char* key, Rotation& rot) override {
				GetOrCreate(node, key).Rot = rot;
				GetOrCreate(node, key).HasRotation = true;
				return false;
			}

			bool VisitScale(const char* node, const char* key, float scale) override {
				GetOrCreate(node, key).Scale = scale;
				GetOrCreate(node, key).HasScale = true;
				return false;
			}


			bool VisitScaleMode(const char* /*node*/, const char* /*key*/, uint32_t /*mode*/) override {
				return false; // not needed for the editor
			}

			private:
			NodeEntry& GetOrCreate(const char* node, const char* key) {
				for (auto& e : Entries) {
					if (e.Node == node && e.Key == key) return e;
				}
				return Entries.emplace_back(NodeEntry{ node, key });
			}
		};

		// EventListener
		void OnSKSEDataLoaded() override;

		static void Register();
		[[nodiscard]] static bool Loaded();

		// Collect every node/key entry on an actor.
		static std::vector<NodeEntry> CollectAll(RE::Actor* a_actor, bool a_firstPerson = false);

		// Individual setters – each calls UpdateNodeTransforms after.
		static void SetPosition(RE::Actor* a_actor, const char* a_node, const char* a_key, Position a_pos, bool a_firstPerson = false);
		static void SetRotation(RE::Actor* a_actor, const char* a_node, const char* a_key, Rotation a_rot, bool a_firstPerson = false);
		static void SetScale(RE::Actor* a_actor, const char* a_node, const char* a_key, float    a_scale, bool a_firstPerson = false);

		static void RemoveNode(RE::Actor* a_actor, const char* a_node, const char* a_key, bool a_firstPerson = false);
		static void RemoveAll(RE::Actor* a_actor);

		private:
		static inline SKEEIntfc::INiTransformInterface* TransformInterface = nullptr;
	};
}