#include "Common/Utilities.hpp"

#include "Features/MorphAdjust/MorphAdjust.hpp"
#include "Common/SKEE/SKEE.hpp"
#include "Common/UI/UIUtils.hpp"

#include "Features/ArmorFactor/ArmorFactor.hpp"


namespace {
	
    RE::TESFaction* SLAnimatingFaction = nullptr;

    void GetSLFaction() {

        if (RE::TESDataHandler* tes = RE::TESDataHandler::GetSingleton()) {
	        RE::TESForm* form = tes->LookupForm(0xE50F, "SexLab.esm");
            if (RE::TESFaction* fact = skyrim_cast<RE::TESFaction*>(form)) {
                SLAnimatingFaction = fact;
                logger::trace("Found SLAnimating Faction: 0x{:X} | 0x{:X}", fact->formID, reinterpret_cast<uintptr_t>(fact));
            }
        }
    }
}


namespace BU::Features {

    void MorphAdjust::OnSerdeSave(SKSE::SerializationInterface* a_this) {

        {
            std::unique_lock lock(_Lock);

            ActorData.Save(a_this);
            ScalarDataUpper.Save(a_this);
            ScalarDataLower.Save(a_this);
        }
    }

    void MorphAdjust::OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {

        {
            std::unique_lock lock(_Lock);

            ActorData.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
            ScalarDataUpper.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
            ScalarDataLower.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
        }
    }

    void MorphAdjust::OnSerdePostLoad() {
        if (ScalarDataUpper.value.empty()) {
            ScalarDataUpper.value = m_defScalarsU;
        }

        if (ScalarDataLower.value.empty()) {
            ScalarDataLower.value = m_defScalarsL;
        }
    }

    void MorphAdjust::OnActorReset(RE::Actor* a_actor) {
        RemoveActor(a_actor);
    }

    void MorphAdjust::OnActorUpdate(RE::Actor* a_actor) {

        if (SKEE::Transforms::Loaded() && SKEE::Morphs::Loaded()) {

            std::unique_lock lock(_Lock);

            auto it = ActorData.value.find(a_actor->formID);
            if (it == ActorData.value.end() || !it->second.Enabled) {
                return;
            }

            Data& data = it->second;

            if (data.Enabled) {

                float RotationUpper = 0.0f;
                float RotationLower = 0.0f;

                absl::flat_hash_map < absl::string_view, std::pair<float, float >> weightMap;
                weightMap.reserve(std::max(ScalarDataUpper.value.size(), ScalarDataLower.value.size()));

                for (const MorphScalar& upper : ScalarDataUpper.value) weightMap[upper.morphName].first  = upper.weight;
                for (const MorphScalar& lower : ScalarDataLower.value) weightMap[lower.morphName].second = lower.weight;

                for (const SKEE::Morphs::MorphEntry& entry : SKEE::Morphs::CollectAll(a_actor)) {
                    if (auto wit = weightMap.find(entry.MorphName); wit != weightMap.end()) {
                        RotationUpper += entry.Value * wit->second.first;
                        RotationLower += entry.Value * wit->second.second;
                    }
                }

                RotationUpper *= data.RotationUpperMult;
                RotationLower *= data.RotationLowerMult;


                if (SLAnimatingFaction && a_actor->IsInFaction(SLAnimatingFaction)) {
                    RotationUpper = 0.0f;
                    RotationLower = 0.0f;
                }

                if (data.NeedsUpdate || std::abs((RotationUpper + RotationLower) - data.CachedWeights) > 1e-4) {

                    const SKEE::Transforms::Rotation RotUL = {  RotationUpper,  RotationUpper, 0.0f };
                    const SKEE::Transforms::Rotation RotUR = { -RotationUpper, -RotationUpper, 0.0f };
                    const SKEE::Transforms::Rotation RotLL = {  RotationLower,  RotationLower, 0.0f };
                    const SKEE::Transforms::Rotation RotLR = { -RotationLower, -RotationLower, 0.0f };

                    //UpperLeft
                    for (const std::string& node : m_defBonesUL) {
                        SKEE::Transforms::SetRotation(a_actor, node.data(), MorphKey.data(), RotUL);
                        SKEE::Transforms::Update(a_actor, node.data());
                    }

                    //UpperRight
                    for (const std::string& node : m_defBonesUR) {
                        SKEE::Transforms::SetRotation(a_actor, node.data(), MorphKey.data(), RotUR);
                        SKEE::Transforms::Update(a_actor, node.data());
                    }

                    //LowerLeft
                    for (const std::string& node : m_defBonesLL) {
                        SKEE::Transforms::SetRotation(a_actor, node.data(), MorphKey.data(), RotLL);
                        SKEE::Transforms::Update(a_actor, node.data());
                    }

                    //LowerRight
                    for (const std::string& node : m_DefBonesLR) {
                        SKEE::Transforms::SetRotation(a_actor, node.data(), MorphKey.data(), RotLR);
                        SKEE::Transforms::Update(a_actor, node.data());
                    }

                    data.CachedWeights = RotationUpper + RotationLower;
                    data.NeedsUpdate = false;
                }
            }
        }
    }

    void MorphAdjust::OnSKSEDataLoaded() {
        GetSLFaction();
    }

    void MorphAdjust::InvalidateData(RE::Actor* a_actor) {
        if (SKEE::Morphs::Loaded()) {
            if (ActorData.value.contains(a_actor->formID)) {
                auto& data = ActorData.value.at(a_actor->formID);

                std::vector<SKEE::Transforms::NodeEntry> list = SKEE::Transforms::CollectAll(a_actor, false);
                for (SKEE::Transforms::NodeEntry& entry : list) {

                    if (!entry.Key.compare(MorphKey)) {
                        SKEE::Transforms::RemoveNode(a_actor, entry.Node.data(), MorphKey.data(), false);
                        SKEE::Transforms::Update(a_actor, entry.Node.data());
                    }
                }

                data.NeedsUpdate = true;
            }
        }
    }

    void MorphAdjust::RemoveActor(RE::Actor* a_actor) {

        {
            std::unique_lock lock(_Lock);
            ActorData.value.erase(a_actor->formID);
        }

	    std::vector<SKEE::Transforms::NodeEntry> list = SKEE::Transforms::CollectAll(a_actor, false);

        for (SKEE::Transforms::NodeEntry& entry : list) {

            if (!entry.Key.compare(MorphKey)) {
                SKEE::Transforms::RemoveNode(a_actor, entry.Node.data(), MorphKey.data(), false);
                SKEE::Transforms::Update(a_actor, entry.Node.data());
            }
        }
    }

    void MorphAdjust::Draw() {

        // --------- Options
        if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen)) {

            static std::string AlreadyAddedText = "";
            ImGui::Text("Add Actor");
            ImGui::SameLine();
            ImGui::TextDisabled(AlreadyAddedText.c_str());

            auto actorList = Utils::GetAllLoadedActors(true);

            static RE::ActorHandle s_selected{};

            std::string previewStr = "[Select Actor]";
            if (auto sel = s_selected.get()) {
                const char* name = sel->GetDisplayFullName();
                previewStr = fmt::format("{} [0x{:08X}]", name ? name : "[Unnamed]", sel->GetFormID());
            }

            ImVec2 contentWidth, AddNewActor_textSize;
            ImGui::GetContentRegionAvail(&contentWidth);
            ImGui::CalcTextSize(&AddNewActor_textSize, "Add", nullptr, true, -1.0f);

            ImGui::SetNextItemWidth(contentWidth.x - AddNewActor_textSize.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
            if (ImGui::BeginCombo("##AddActor", previewStr.c_str())) {
                for (RE::Actor* actor : actorList) {
                    if (!actor) continue;
                    const char* name = actor->GetDisplayFullName();
                    const RE::FormID id = actor->GetFormID();
                    const std::string item = fmt::format("{} [0x{:08X}]", name ? name : "[Unnamed]", id);
                    const bool is_selected = (s_selected.get().get() == actor);
                    if (ImGui::Selectable(item.c_str(), is_selected))
                        s_selected = actor->GetHandle();
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Select a loaded actor to track.");
            }

            ImGui::SameLine();

            const bool canAdd = s_selected.get() && !ActorData.value.contains(s_selected.get()->formID);

            {
                ImGui::BeginDisabled(!canAdd);
                if (canAdd) ImUtil::ButtonStyle_Green();
                if (ImGui::Button("Add##Options")) {
                    if (RE::Actor* actor = s_selected.get().get())
                        ActorData.value.emplace(actor->formID, Data{});
                }
                if (canAdd) ImUtil::ButtonStyle_Reset();
                ImGui::EndDisabled();
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (!s_selected.get())  ImGui::SetTooltip("Select an actor first.");
                else if (!canAdd)       ImGui::SetTooltip("This actor has already been added.");
                else                    ImGui::SetTooltip("Add the selected actor to the morph adjust system.");
            }

            AlreadyAddedText = !canAdd && s_selected.get() ? "[Already added]" : "";

        }

        // --------- Edit Actor
        if (ImGui::CollapsingHeader("Edit Actor", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ActorData.value.empty()) {
                ImGui::TextDisabled("No actors added yet. Use the Options section above.");
            }
            else {
                static RE::FormID s_selectedID = 0;

                if (s_selectedID != 0 && !ActorData.value.contains(s_selectedID)) s_selectedID = 0;
                if (s_selectedID == 0 && !ActorData.value.empty())                s_selectedID = ActorData.value.begin()->first;

                auto BuildActorLabel = [](RE::FormID id) -> std::string {
                    if (id == 0) return "[Select Actor]";
                    if (RE::TESForm* form = RE::TESForm::LookupByID(id))
                        if (RE::Actor* actor = form->As<RE::Actor>()) {
                            const char* name = actor->GetDisplayFullName();
                            return fmt::format("{} [0x{:08X}]", name ? name : "[Unnamed]", id);
                        }
                    return fmt::format("[Missing] [0x{:08X}]", id);
                };

                // Actor selector + Remove button
                {
                    ImVec2 availableWidth, removeTextSize;
                    ImGui::GetContentRegionAvail(&availableWidth);
                    ImGui::CalcTextSize(&removeTextSize, "Remove", nullptr, true, -1.0f);

                    ImGui::SetNextItemWidth(availableWidth.x - removeTextSize.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
                    if (ImGui::BeginCombo("##EditActor", BuildActorLabel(s_selectedID).c_str())) {
                        for (const auto& key : ActorData.value | std::views::keys) {
                            const bool is_selected = (s_selectedID == key);
                            if (ImGui::Selectable(BuildActorLabel(key).c_str(), is_selected))
                                s_selectedID = key;
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

                if (s_selectedID != 0) {
                    auto it = ActorData.value.find(s_selectedID);
                    if (it != ActorData.value.end()) {
                        Data& data = it->second;

                        RE::Actor* actor = nullptr;
                        if (auto* form = RE::TESForm::LookupByID(s_selectedID))
                            actor = form->As<RE::Actor>();

                        // Remove button
                        ImGui::SameLine();
                        ImUtil::ButtonStyle_Red();
                        const bool doRemove = ImGui::Button("Remove##ActorOptions");
                        ImUtil::ButtonStyle_Reset();
                        ImGui::Spacing();

                        if (doRemove) {
                            ActorData.value.erase(s_selectedID);
                            s_selectedID = ActorData.value.empty() ? 0 : ActorData.value.begin()->first;
                            if (actor) {
                                SKEE::Morphs::Clear(actor, MorphKey.data());
                                SKEE::Morphs::Apply(actor);
                            }
                            // Guard against dangling ref after erase
                            goto end_edit_actor;
                        }

                        // -- Enabled
                        {
                            ImGui::Checkbox("Enabled##ActorOptions", &data.Enabled);
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Temporarily enable or disable morph adjustment for this actor.");

                        }

                        ImGui::Spacing();

                        // -- Rotation
                        {
                            ImGui::SetNextItemWidth(220.0f);
                            ImGui::InputFloat("Rotation Upper##ActorOptions", &data.RotationUpperMult, 0.1f, 1.0f, "%.2f");

                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Rotation applied to upper body nodes.");
                        }

                        {
                            ImGui::SetNextItemWidth(220.0f);
                            ImGui::InputFloat("Rotation Lower##ActorOptions", &data.RotationLowerMult, 0.1f, 1.0f, "%.2f");

                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Rotation applied to lower body nodes.");
                        }
                    }
                }
            	end_edit_actor:;
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

        }

        // --- Scalar Entries (shared / global) ------------------------------------
        // Shared lambda for a resizable InputText callback
        static auto resizeStrCb = [](ImGuiInputTextCallbackData* d) -> int {
            if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                auto* s = static_cast<std::string*>(d->UserData);
                s->resize(static_cast<size_t>(d->BufTextLen));
                d->Buf = s->data();
            }
            return 0;
        };

        // DrawScalarSection: renders an editable VectorRecord<MorphScalar> list.
        // 'header'       – CollapsingHeader label
        // 'list'         – reference to the vector to mutate
        // 'defaults'     – default vector to restore on "Reset to Defaults"
        // 'uid'          – outer PushID value for isolation
        auto DrawScalarSection = [&](const char* header,
            std::vector<MorphScalar>& list,
            const std::vector<MorphScalar>& defaults,
            int uid)
            {
                ImGui::PushID(uid);
                if (ImGui::CollapsingHeader(header)) {
                    bool changed = false;

                    // Add new entry row
                    ImGui::Text("Add New Scalar");
                    static std::string newName;
                    static float       newWeight = 0.0f;

                    ImGui::SetNextItemWidth(350.0f);
                    ImGui::InputText("##NewScalarName", newName.data(), newName.capacity() + 1,
                        ImGuiInputTextFlags_CallbackResize, resizeStrCb, &newName);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("The raw morph name (as shown in Outfit Studio).");

                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(150.0f);
                    ImGui::InputFloat("##NewScalarWeight", &newWeight, 0.01f, 0.1f, "%.3f");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Weight/strength of this morph scalar.\nNegative values are allowed.");

                    ImGui::SameLine();
                    const bool canAdd = !newName.empty();
                    ImGui::BeginDisabled(!canAdd);
                    if (canAdd) ImUtil::ButtonStyle_Green();
                    if (ImGui::Button("Add##Scalar")) {
                        list.push_back(MorphScalar{ newName, newWeight });
                        newName.clear();
                        newWeight = 0.0f;
                        changed = true;
                    }
                    if (canAdd) ImUtil::ButtonStyle_Reset();
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        if (!canAdd) ImGui::SetTooltip("Enter a morph name first.");
                        else         ImGui::SetTooltip("Add this scalar to the list.");
                    }

                    ImGui::Spacing();
                    ImGui::Text("Scalars (%zu)", list.size());

                    std::optional<std::size_t> eraseIndex;
                    for (std::size_t i = 0; i < list.size(); ++i) {
                        auto& e = list[i];
                        ImGui::PushID(static_cast<int>(i));

                        ImGui::SetNextItemWidth(350.0f);
                        if (ImGui::InputText("##Name", e.morphName.data(), e.morphName.capacity() + 1,
                            ImGuiInputTextFlags_CallbackResize, resizeStrCb, &e.morphName))
                            changed = true;
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Raw morph key name as shown in Outfit Studio.");

                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(150.0f);
                        if (ImGui::InputFloat("##Weight", &e.weight, 0.01f, 0.1f, "%.3f"))
                            changed = true;
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Weight/strength multiplier of this morph.\nNegative values are allowed.");

                        ImGui::SameLine();
                        ImUtil::ButtonStyle_Red();
                        if (ImGui::Button("X##Scalar"))
                            eraseIndex = i;
                        ImUtil::ButtonStyle_Reset();
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Remove this scalar.");

                        ImGui::PopID();
                    }

                    if (eraseIndex) {
                        list.erase(list.begin() + static_cast<std::ptrdiff_t>(*eraseIndex));
                        changed = true;
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("Reset to Defaults##Scalar")) {
                        list = defaults;
                        changed = true;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Restore this scalar list to its default configuration.");

                    // Propagate changes to all tracked actors
                    if (changed) {
                        for (const RE::FormID& id : ActorData.value | std::views::keys) {
                            if (RE::TESForm* form = RE::TESForm::LookupByID(id))
                                if (RE::Actor* actor = form->As<RE::Actor>())
                                    InvalidateData(actor);
                        }
                    }
                }
                ImGui::PopID();
            };

        DrawScalarSection("Upper Scalars", ScalarDataUpper.value, m_defScalarsU, 100);
        DrawScalarSection("Lower Scalars", ScalarDataLower.value, m_defScalarsL, 101);

    }
}
