#include "Hooks/Hooks.hpp"
#include "Hooks/Util/HookUtil.hpp"

namespace Hooks {

	struct MainUpdateNullSub {

		static void thunk (RE::Main* a_this, float a_deltaTime) {
			func(a_this, a_deltaTime);
			BU::EventDispatcher::DispatchUpdate();

		}

		FUNCTYPE_CALL func;

	};

	struct Load3D {

		static constexpr std::size_t funcIndex = 0x6A;

		template<int ID>
		static RE::NiAVObject* thunk(RE::Actor* a_actor, bool a_backgroundLoading) {
			RE::NiAVObject* Res = func<ID>(a_actor, a_backgroundLoading);

			{
				const auto& intfc = SKSE::GetTaskInterface();
				intfc->AddTask([a_actor] {
					BU::EventDispatcher::DispatchActorLoad3D(a_actor);
				});
			}

			return Res;
		}

		template<int ID>
		FUNCTYPE_VFUNC_UNIQUE func;
	};

	struct Update {

		static constexpr std::size_t funcIndex = 0xAD;
		template<int ID>
		static void thunk(RE::Actor* a_this, float a_deltaTime) {

			func<ID>(a_this, a_deltaTime);
			BU::EventDispatcher::DispatchActorUpdate(a_this);
		}
		template<int ID>
		FUNCTYPE_VFUNC_UNIQUE func;
	};

	struct EquipObject {

		static void thunk(RE::ActorEquipManager* a_this, RE::Actor* a_actor, RE::TESBoundObject* a_object, std::uint64_t a_unk) {
			func(a_this, a_actor, a_object, a_unk);

			{
				const auto& intfc = SKSE::GetTaskInterface();
				intfc->AddTask([a_actor] {
					BU::EventDispatcher::DispatchActorEquipEvent(a_actor);
				});
			}

		}

		FUNCTYPE_CALL func;
	};

	struct UnEquipObject {

		static void thunk(RE::ActorEquipManager* a_this, RE::Actor* a_actor, RE::TESBoundObject* a_object, std::uint64_t a_unk) {
			func(a_this, a_actor, a_object, a_unk);

			{
				const auto& intfc = SKSE::GetTaskInterface();
				intfc->AddTask([a_actor] {
					BU::EventDispatcher::DispatchActorUnEquipEvent(a_actor);
				});
			}

		}

		FUNCTYPE_CALL func;
	};

	void Install() {


		stl::write_call<MainUpdateNullSub>(REL::RelocationID(35565, 36564, NULL), REL::VariantOffset(0x748, 0xC26, NULL));

		stl::write_vfunc_unique<Load3D, 1>(RE::VTABLE_Character[0]);
		stl::write_vfunc_unique<Load3D, 2>(RE::VTABLE_PlayerCharacter[0]);

		stl::write_vfunc_unique<Update, 1>(RE::VTABLE_Character[0]);
		stl::write_vfunc_unique<Update, 2>(RE::VTABLE_PlayerCharacter[0]);

		stl::write_call<EquipObject>(REL::RelocationID(37938, 38894, NULL), REL::VariantOffset(0xE5, 0x170, NULL));
		stl::write_call<UnEquipObject>(REL::RelocationID(37945, 38901, NULL), REL::VariantOffset(0x138, 0x1b9, NULL));

	}
}