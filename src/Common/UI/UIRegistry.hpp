#pragma once
#include "SKSEMenuFramework.hpp"

#include <absl/container/btree_map.h>
#include <absl/strings/string_view.h>


namespace BU::UI {

    // Single item registration: (name -> render fn)
    class UIItemRegistry : public CInitSingleton<UIItemRegistry>, public EventListener {
    public:
        using RenderFn = SKSEMenuFramework::Model::RenderFunction;

        void OnSKSEDataLoaded() override {
            if (!SKSEMenuFramework::IsInstalled()) {
                return;
            }

            SKSEMenuFramework::SetSection(SectionName.data());
            for (auto const& [name, fn] : Items()) {
                SKSEMenuFramework::AddSectionItem(name.data(), fn);
            }
        }

        template <class T>
        static bool RegisterType() {
            static_assert(requires { T::UICategoryName; });
            static_assert(requires { T::Draw; });

            Items().emplace(
                absl::string_view{ T::UICategoryName.data(), T::UICategoryName.size() },
                RenderFn{ T::Draw }
            );

            return true;
        }

        static constexpr std::string_view SectionName = "Bingus Utils";

    private:
        static absl::btree_map<absl::string_view, RenderFn>& Items() {
            static absl::btree_map<absl::string_view, RenderFn> map;
            return map;
        }
    };


    // Inherit and define:
    //  - static constexpr std::string_view Name
    //  - static void Draw(...)
    template <class Derived>
    struct UIEntry {
    private:
        static inline const bool _registered = UIItemRegistry::RegisterType<Derived>();
    };

} // namespace BU::UI