#include "Common/Utilities.hpp"
#include "Features/OverlaySaver/OverlaySaver.hpp"
#include "Common/SKEE/SKEE.hpp"
#include "Common/UI/UIUtils.hpp"


namespace {

	using BodyLoc = SKEEIntfc::IOverlayInterface::OverlayLocation;

	const BU::SKEE::Overlays::BodyPartInfo* GetBodyPartInfo(uint8_t a_loc) {
		switch (static_cast<BodyLoc>(a_loc)) {
			case BodyLoc::Face: return &BU::SKEE::Overlays::FaceInfo;
			case BodyLoc::Body: return &BU::SKEE::Overlays::BodyInfo;
			case BodyLoc::Hand: return &BU::SKEE::Overlays::HandsInfo;
			case BodyLoc::Feet: return &BU::SKEE::Overlays::FeetInfo;
			default: return nullptr;
		}
	}

	uint32_t ClampRGB(uint32_t c) { return c & 0x00FFFFFFu; }

	void RGBToFloat3(uint32_t rgb, std::array<float, 3>& out) {
		rgb = ClampRGB(rgb);
		out[0] = static_cast<float>((rgb >> 16) & 0xFFu) / 255.0f;
		out[1] = static_cast<float>((rgb >> 8) & 0xFFu) / 255.0f;
		out[2] = static_cast<float>(rgb & 0xFFu) / 255.0f;
	}

	uint32_t Float3ToRGB(std::array<float, 3> in) {
		auto to_u8 = [](float v) -> uint32_t {
			v = std::clamp(v, 0.0f, 1.0f);
			return (static_cast<uint32_t>(std::lround(v * 255.0f)) & 0xFFu);
		};
		const uint32_t r = to_u8(in[0]);
		const uint32_t g = to_u8(in[1]);
		const uint32_t b = to_u8(in[2]);
		return (r << 16) | (g << 8) | b;
	}

	void ReindexCompactTop(std::vector<BU::Features::OverlayTools::OverlayEntry>& v) {
		int16_t idx = 0;
		for (auto& e : v) {
			e.Index = idx++;
		}
	}

	void ReindexCompactBottom(std::vector<BU::Features::OverlayTools::OverlayEntry>& v, int16_t count) {
		if (count <= 0) {
			return;
		}
		const int16_t n = static_cast<int16_t>(v.size());
		const int16_t start = static_cast<int16_t>(std::max<int>(0, count - n));
		for (int i = 0; i < n; ++i) {
			v[i].Index = static_cast<int16_t>(start + i);
		}
	}

	void ReindexFlip(std::vector<BU::Features::OverlayTools::OverlayEntry>& v, int16_t count) {
		if (count <= 0) {
			return;
		}
		for (auto& e : v) {
			e.Index = static_cast<int16_t>((count - 1) - std::clamp<int>(e.Index, 0, count - 1));
		}
	}
}

namespace BU::Features {

	// -- Events ----------------------------------------------------------------

	void OverlayTools::OnSerdeSave(SKSE::SerializationInterface* a_this) {
		std::scoped_lock lock{ m_mutex };
		ActorData.Save(a_this);
	}

	void OverlayTools::OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {
		std::scoped_lock lock{ m_mutex };
		ActorData.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
	}

	void OverlayTools::OnSerdePostLoad() {
		std::scoped_lock lock{ m_mutex };
		for (auto& data : ActorData.value | std::views::values) {
			data.AlreadyApplied = false;
		}
	}

	void OverlayTools::OnActorReset(RE::Actor* a_actor) {
		RemoveActor(a_actor);
	}

	void OverlayTools::OnActorUpdate(RE::Actor* a_actor) {
		/*
		if (!a_actor || !SKEE::Overlays::Loaded()) return;

		{
			std::scoped_lock lock{ m_mutex };
			auto it = ActorData.value.find(a_actor->formID);
			if (it == ActorData.value.end() || it->second.AlreadyApplied || !a_actor->Is3DLoaded())
				return;
		}

		ApplyStoredOvls(a_actor);
		*/
	}

	void OverlayTools::OnActorLoad3D(RE::Actor* a_actor) {
		if (!a_actor || !SKEE::Overlays::Loaded()) return;

		{
			std::scoped_lock lock{ m_mutex };
			if (!ActorData.value.contains(a_actor->formID))
				return;
		}

		ApplyStoredOvls(a_actor);
	}

	void OverlayTools::OnActorEquip(RE::Actor* a_actor) {
		//InvalidateAndApplyToActor(a_actor);
	}

	void OverlayTools::OnActorUnequip(RE::Actor* a_actor) {
		//InvalidateAndApplyToActor(a_actor);
	}


	// -- Core ------------------------------------------------------------------

	void OverlayTools::RemoveActor(RE::Actor* a_actor) {
		if (!a_actor) {
			return;
		}

		{
			std::scoped_lock lock{ m_mutex };
			ActorData.value.erase(a_actor->formID);
		}

	}

	void OverlayTools::ClearStoredOverlays(RE::Actor* a_actor) {
		if (!a_actor) {
			return;
		}

		{
			std::scoped_lock lock{ m_mutex };
			if (auto it = ActorData.value.find(a_actor->formID); it != ActorData.value.end()) {
				it->second.OvlFace.clear();
				it->second.OvlBody.clear();
				it->second.OvlHands.clear();
				it->second.OvlFeet.clear();
			}
		}
	}

	void OverlayTools::ClearGameOverlays(RE::Actor* a_actor) {
		if (!a_actor) {
			return;
		}

		{
			SKEE::Overlays::ClearBodyPart(a_actor, SKEE::Overlays::FaceInfo);
			SKEE::Overlays::ClearBodyPart(a_actor, SKEE::Overlays::BodyInfo);
			SKEE::Overlays::ClearBodyPart(a_actor, SKEE::Overlays::HandsInfo);
			SKEE::Overlays::ClearBodyPart(a_actor, SKEE::Overlays::FeetInfo);
		}
	}

	void OverlayTools::BuildOverlayList(RE::Actor* a_actor) {
		if (!SKEE::Overlays::Loaded() || !a_actor) {
			return;
		}

		auto faceIdxs =  SKEE::Overlays::GetUsedSlotIndices(a_actor, SKEE::Overlays::FaceInfo);
		auto bodyIdxs =  SKEE::Overlays::GetUsedSlotIndices(a_actor, SKEE::Overlays::BodyInfo);
		auto handsIdxs = SKEE::Overlays::GetUsedSlotIndices(a_actor, SKEE::Overlays::HandsInfo);
		auto feetIdxs =  SKEE::Overlays::GetUsedSlotIndices(a_actor, SKEE::Overlays::FeetInfo);

		std::vector<OverlayEntry> face, body, hands, feet;

		for (uint16_t idx : faceIdxs)  SKEE::Overlays::BuildOverlayAtIdx(a_actor, SKEE::Overlays::FaceInfo, idx, &face.emplace_back());
		for (uint16_t idx : bodyIdxs)  SKEE::Overlays::BuildOverlayAtIdx(a_actor, SKEE::Overlays::BodyInfo, idx, &body.emplace_back());
		for (uint16_t idx : handsIdxs) SKEE::Overlays::BuildOverlayAtIdx(a_actor, SKEE::Overlays::HandsInfo, idx, &hands.emplace_back());
		for (uint16_t idx : feetIdxs)  SKEE::Overlays::BuildOverlayAtIdx(a_actor, SKEE::Overlays::FeetInfo, idx, &feet.emplace_back());

		
		{
			std::scoped_lock lock{ m_mutex };

			auto it = ActorData.value.find(a_actor->formID);
			if (it == ActorData.value.end()) {
				return;
			}

			auto& data = it->second;
			data.OvlFace  = std::move(face);
			data.OvlBody  = std::move(body);
			data.OvlHands = std::move(hands);
			data.OvlFeet  = std::move(feet);
			data.AlreadyApplied = false;
		}
	}

	void OverlayTools::ApplyStoredOvls(RE::Actor* a_actor) {
		if (!SKEE::Overlays::Loaded() || !a_actor) {
			return;
		}
		if (!a_actor->Is3DLoaded()) {
			return;
		}

		std::vector<OverlayEntry> body, face, hands, feet;
		{
			std::scoped_lock lock{ m_mutex };
			auto it = ActorData.value.find(a_actor->formID);
			if (it == ActorData.value.end()) {
				return;
			}
			body = it->second.OvlBody;
			face = it->second.OvlFace;
			hands = it->second.OvlHands;
			feet = it->second.OvlFeet;
		}

		auto clear_indices = [&](const std::vector<OverlayEntry>& list) {
			for (const auto& ovl : list) {
				if (const auto* info = GetBodyPartInfo(ovl.BodyPart)) {
					SKEE::Overlays::RemoveSingle(a_actor, *info, static_cast<uint16_t>(ovl.Index));
				}
			}
		};

		clear_indices(body);
		clear_indices(face);
		clear_indices(feet);
		clear_indices(hands);

		for (auto& ovl : body)  SKEE::Overlays::SetSingle(a_actor, ovl);
		for (auto& ovl : face)  SKEE::Overlays::SetSingle(a_actor, ovl);
		for (auto& ovl : feet)  SKEE::Overlays::SetSingle(a_actor, ovl);
		for (auto& ovl : hands) SKEE::Overlays::SetSingle(a_actor, ovl);

		{
			std::unique_lock lock{ m_mutex };
			if (auto it = ActorData.value.find(a_actor->formID); it != ActorData.value.end()) {
				it->second.AlreadyApplied = true;
			}
		}
	}

	void OverlayTools::AddNewActor(RE::Actor* a_actor) {
		if (!a_actor) {
			return;
		}

		{
			std::scoped_lock lock{ m_mutex };
			auto [it, inserted] = ActorData.value.emplace(a_actor->formID, Data{});
			it->second.AlreadyApplied = false;
		}

		BuildOverlayList(a_actor);
		ApplyStoredOvls(a_actor);
	}

	void OverlayTools::InvalidateAndApplyToActor(RE::Actor* a_actor) {
		if (!a_actor || !SKEE::Overlays::Loaded()) return;

		ClearStoredOverlays(a_actor);
		BuildOverlayList(a_actor);
		ApplyStoredOvls(a_actor);
	}


	// -- UI ------------------------------------------------------------------------

	void OverlayTools::DrawOverlayEntry(RE::Actor* a_actor) {

		if (!a_actor) {
			ImGui::TextDisabled("[Invalid Actor]");
			return;
		}
		if (!SKEE::Overlays::Loaded()) {
			ImGui::TextDisabled("SKEE not loaded.");
			return;
		}

		Data* data = nullptr;
		{
			std::scoped_lock lock{ m_mutex };
			auto it = ActorData.value.find(a_actor->formID);
			if (it == ActorData.value.end()) {
				ImGui::TextDisabled("Actor not tracked.");
				return;
			}
			data = &it->second;
		}

		if (!data) return;

		// -- Region selector ------------------------------------------------------
		struct RegionDef {
			const char* label;
			uint8_t loc;
			std::vector<OverlayEntry>& list;
		};

		RegionDef regions[] = {
			{ "Body",  static_cast<uint8_t>(BodyLoc::Body),  data->OvlBody  },
			{ "Face",  static_cast<uint8_t>(BodyLoc::Face),  data->OvlFace  },
			{ "Hands", static_cast<uint8_t>(BodyLoc::Hand),  data->OvlHands },
			{ "Feet",  static_cast<uint8_t>(BodyLoc::Feet),  data->OvlFeet  },
		};

		static int s_regionIdx = 0;
		if (s_regionIdx >= static_cast<int>(std::size(regions)))
			s_regionIdx = 0;

		RegionDef& region = regions[s_regionIdx];
		const auto* info = GetBodyPartInfo(region.loc);
		const int16_t maxCount = info ? info->Count : 0;

		// Region ComboBox
		
		ImGui::Text("Overlay Regions");

		ImVec2 availableWidth;
		ImGui::GetContentRegionAvail(&availableWidth);

		ImGui::SetNextItemWidth(availableWidth.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
		if (ImGui::BeginCombo("##Region", region.label)) {
			for (int i = 0; i < static_cast<int>(std::size(regions)); ++i) {
				const bool sel = (s_regionIdx == i);
				if (ImGui::Selectable(regions[i].label, sel)) s_regionIdx = i;
				if (sel)                                      ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Select which body region's overlays to view and edit.");
		}

		ImGui::Spacing();

		// -- Region-level actions -------------------------------------------------

		if (maxCount > 0) {

			{
				if (ImGui::Button("Compact Top")) {
					if (info) SKEE::Overlays::ClearBodyPart(a_actor, *info);

					{
						std::scoped_lock lock{ m_mutex };
						ReindexCompactTop(region.list);
						data->AlreadyApplied = false;
					}

				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Reindex overlays compacted toward slot 0.");
				}
			}

			ImGui::SameLine();

			{
				if (ImGui::Button("Compact Bottom")) {
					if (info) SKEE::Overlays::ClearBodyPart(a_actor, *info);
					{
						std::scoped_lock lock{ m_mutex };
						ReindexCompactBottom(region.list, maxCount);
						data->AlreadyApplied = false;
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Reindex overlays compacted toward the last slot.");
				}
			}

			ImGui::SameLine();

			{
				if (ImGui::Button("Flip Order")) {
					if (info) SKEE::Overlays::ClearBodyPart(a_actor, *info);

					{
						std::scoped_lock lock{ m_mutex };
						ReindexFlip(region.list, maxCount);
						data->AlreadyApplied = false;
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Reverse the index order of all stored overlays.");
				}
			}

			ImGui::SameLine();

			{
				if (ImGui::Button("Clear RM Ovls")) {
					if (info) SKEE::Overlays::ClearBodyPart(a_actor, *info);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Clear the specified bodypart's racemenu overlays without modifying the stored ones.");
				}
			}

		}

		ImGui::Spacing();

		

		ImGui::Text("Stored Overlay Editor");

		// -- Overlay selector -----------------------------------------------------
		auto GetOverlayLabel = [](const OverlayEntry& e) -> std::string {
			std::string path = e.TexturePath;
			std::ranges::replace(path, '\\', '/');
			const auto slash = path.rfind('/');
			std::string filename = (slash != std::string::npos) ? path.substr(slash + 1) : path;

			if (const auto dot = filename.rfind('.'); dot != std::string::npos) filename.erase(dot);
			if (filename.empty())                                               filename = "[Unnamed]";

			const uint8_t r = (e.DiffuseColor >> 16) & 0xFF;
			const uint8_t g = (e.DiffuseColor >> 8) & 0xFF;
			const uint8_t b = e.DiffuseColor & 0xFF;
			const uint8_t a = static_cast<uint8_t>(std::clamp(e.DiffuseAlpha, 0.0f, 1.0f) * 255.0f + 0.5f);
			return fmt::format("{} #{:02X}{:02X}{:02X} A:{:02X}", filename, r, g, b, a);
		};

		static int s_overlayIdx = 0;
		auto& list = region.list;

		if (list.empty())										s_overlayIdx = 0;
		else if (s_overlayIdx >= static_cast<int>(list.size())) s_overlayIdx = static_cast<int>(list.size()) - 1;

		// Add Overlay Button
		{
			ImUtil::ButtonStyle_Green();
			if (ImGui::Button("Add")) {
				OverlayEntry e{};
				e.BodyPart = region.loc;
				e.Index = 0;
				e.DiffuseAlpha = 1.0f;
				e.DiffuseColor = 0xFFFFFFu;
				e.TexturePath = "textures\\actors\\character\\overlays\\default.dds";
				std::unique_lock lock{ m_mutex };
				list.emplace_back(std::move(e));
				s_overlayIdx = static_cast<int>(list.size()) - 1;
				data->AlreadyApplied = false;
			}
			ImUtil::ButtonStyle_Reset();

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Add a new overlay entry with default settings.");
			}
		}

		if (!list.empty()) {
			ImGui::SameLine();

			ImVec2 removeTextSize;
			ImGui::GetContentRegionAvail(&availableWidth);
			ImGui::CalcTextSize(&removeTextSize, "X", nullptr, true, -1.0f);

			ImGui::SetNextItemWidth(availableWidth.x - removeTextSize.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
			const std::string overlayPreview = fmt::format("[{}] {}", s_overlayIdx, GetOverlayLabel(list[s_overlayIdx]));
			if (ImGui::BeginCombo("##OverlaySelect", overlayPreview.c_str())) {
				for (int i = 0; i < static_cast<int>(list.size()); ++i) {
					const std::string label = fmt::format("[{}] {}", i, GetOverlayLabel(list[i]));
					const bool sel = (s_overlayIdx == i);
					if (ImGui::Selectable(label.c_str(), sel)) s_overlayIdx = i;
					if (sel)                                   ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Select a stored overlay entry to view and edit.");
			}

			ImGui::SameLine();

			//Remove Button
			{
				ImUtil::ButtonStyle_Red();
				if (ImGui::Button("X##RemoveOverlay")) {
					std::unique_lock lock{ m_mutex };
					list.erase(list.begin() + s_overlayIdx);
					if (s_overlayIdx >= static_cast<int>(list.size()))
						s_overlayIdx = std::max(0, static_cast<int>(list.size()) - 1);
					data->AlreadyApplied = false;
				}
				ImUtil::ButtonStyle_Reset();

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Remove this overlay entry.");
				}
			}
		}

		// -- Selected overlay editor -----------------------------------------------
		if (!list.empty()) {
			auto& ovl = list[s_overlayIdx];
			ovl.BodyPart = region.loc;

			ImGui::Spacing();

			// Index
			{
				int idx = ovl.Index;
				const int maxIdx = std::max<int>(0, maxCount - 1);
				if (ImGui::SliderInt("Index", &idx, 0, maxIdx)) {
					std::unique_lock lock{ m_mutex };
					ovl.Index = static_cast<int16_t>(idx);
					data->AlreadyApplied = false;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("The overlay slot index.");
				}
			}

			// Alpha
			{
				float alpha = ovl.DiffuseAlpha;
				if (ImGui::SliderFloat("Alpha", &alpha, 0.0f, 1.0f)) {
					std::unique_lock lock{ m_mutex };
					ovl.DiffuseAlpha = alpha;
					data->AlreadyApplied = false;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Opacity.");
				}
			}

			// Color
			{
				std::array<float, 4> col4{};
				{
					std::array<float, 3> rgb{};
					RGBToFloat3(ovl.DiffuseColor, rgb);
					col4[0] = rgb[0]; col4[1] = rgb[1]; col4[2] = rgb[2]; col4[3] = 1.0f;
				}
				if (ImGui::ColorEdit4("Color", col4.data(), ImGuiColorEditFlags_NoAlpha)) {
					std::array rgb{ col4[0], col4[1], col4[2] };
					const uint32_t keep = ovl.DiffuseColor & 0xFF000000u;
					std::unique_lock lock{ m_mutex };
					ovl.DiffuseColor = keep | Float3ToRGB(rgb);
					data->AlreadyApplied = false;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Diffuse tint color.");
				}
			}

			// Texture Path
			{
				char buf[768]{};
				std::strncpy(buf, ovl.TexturePath.c_str(), sizeof(buf) - 1);
				if (ImGui::InputText("Texture", buf, sizeof(buf))) {
					std::unique_lock lock{ m_mutex };
					ovl.TexturePath = buf;
					data->AlreadyApplied = false;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Path to the overlay texture.");
				}
			}
		}

	}

	void OverlayTools::Draw() {

		// --- Options ------------------------------------------------------------
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

			ImVec2 contentWidth, addTextSize;
			ImGui::GetContentRegionAvail(&contentWidth);
			ImGui::CalcTextSize(&addTextSize, "Add", nullptr, true, -1.0f);

			ImGui::SetNextItemWidth(contentWidth.x - addTextSize.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
			if (ImGui::BeginCombo("##AddActor", previewStr.c_str())) {
				for (RE::Actor* actor : actorList) {
					if (!actor) continue;
					const char* name = actor->GetDisplayFullName();
					const RE::FormID id = actor->GetFormID();
					const std::string item = fmt::format("{} [0x{:08X}]", name ? name : "[Unnamed]", id);
					const bool is_selected = (s_selected.get().get() == actor);

					if (ImGui::Selectable(item.c_str(), is_selected)) s_selected = actor->GetHandle();
					if (is_selected)                                  ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();

			const bool canAdd = [&] {
				{
					std::scoped_lock lock{ m_mutex };
					return s_selected.get() && !ActorData.value.contains(s_selected.get()->formID);
				}
			}();

			// Add Actor Button
			{
				ImGui::BeginDisabled(!canAdd);
				if (canAdd) ImUtil::ButtonStyle_Green();

				if (ImGui::Button("Add")) {
					if (RE::Actor* actor = s_selected.get().get())
						AddNewActor(actor);
				}

				if (canAdd) ImUtil::ButtonStyle_Reset();
				ImGui::EndDisabled();

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
					if (!s_selected.get())  ImGui::SetTooltip("Select an actor first.");
					else if (!canAdd)       ImGui::SetTooltip("This actor has already been added.");
					else                    ImGui::SetTooltip("Add the selected actor to the overlay tools system.");
				}
			}
				AlreadyAddedText = !canAdd && s_selected.get() ? "[Already added]" : "";

				ImGui::Spacing();
				ImGui::Spacing();
				ImGui::Spacing();
		}

		// --- Edit Actor ---------------------------------------------------------
		if (ImGui::CollapsingHeader("Edit Actor", ImGuiTreeNodeFlags_DefaultOpen)) {

			const bool isEmpty = [&] {
				{
					std::scoped_lock lock{ m_mutex };
					return ActorData.value.empty();
				}
			}();

			if (isEmpty) {
				ImGui::TextDisabled("No actors added yet. Use the Options section above.");
			}
			else {
				static RE::FormID s_selectedID = 0;

				// Snapshot keys under a short shared lock — no lock held while rendering
				std::vector<RE::FormID> keys;
				{
					std::scoped_lock scoped_lock{ m_mutex };
					for (const auto& k : ActorData.value | std::views::keys) keys.push_back(k); 

					if (s_selectedID != 0 && !ActorData.value.contains(s_selectedID)) s_selectedID = 0;
					if (s_selectedID == 0 && !ActorData.value.empty())                s_selectedID = ActorData.value.begin()->first;
				}

				auto BuildActorLabel = [](RE::FormID id) -> std::string {
					if (id == 0) return "[Select Actor]";

					if (auto* form = RE::TESForm::LookupByID(id)) {
						if (auto* actor = form->As<RE::Actor>()) {
							const char* name = actor->GetDisplayFullName();
							return fmt::format("{} [0x{:08X}]", name ? name : "[Unnamed]", id);
						}
					}

					return fmt::format("[Missing] [0x{:08X}]", id);
				};

				{
					ImVec2 availableWidth, removeTextSize;
					ImGui::GetContentRegionAvail(&availableWidth);
					ImGui::CalcTextSize(&removeTextSize, "Remove", nullptr, true, -1.0f);

					ImGui::SetNextItemWidth(availableWidth.x - removeTextSize.x - ImGui::GetStyle()->ItemSpacing.x * 2.0f - ImGui::GetStyle()->FramePadding.x * 4.0f);
					if (ImGui::BeginCombo("##EditActor", BuildActorLabel(s_selectedID).c_str())) {
						for (const RE::FormID key : keys) {
							const bool is_selected = (s_selectedID == key);
							if (ImGui::Selectable(BuildActorLabel(key).c_str(), is_selected))
								s_selectedID = key;
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
				ImGui::SameLine();

				if (s_selectedID != 0) {
					RE::Actor* actor = nullptr;
					if (auto* form = RE::TESForm::LookupByID(s_selectedID)) {
						actor = form->As<RE::Actor>();
					}

					bool doRemove = false;
					{
						ImGui::SameLine();
						ImUtil::ButtonStyle_Red();
						doRemove = ImGui::Button("Remove##ActorOptions");
						ImUtil::ButtonStyle_Reset();
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Remove this actor.");
						}
					}

					ImGui::Spacing();
					

					if (doRemove) {
						std::unique_lock lock{ m_mutex };
						ActorData.value.erase(s_selectedID);
						s_selectedID = ActorData.value.empty() ? 0 : ActorData.value.begin()->first;
					}
					else if (actor) {

						ImGui::Text("Actions");

						{
							if (ImGui::Button("Apply Stored")) {
								ApplyStoredOvls(actor);
							}
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("Non destructively apply all currently stored overlays to the actor.");
							}
						}

						ImGui::SameLine();

						{
							if (ImGui::Button("Invalidate")) {
								InvalidateAndApplyToActor(actor);
							}
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("Reconstruct stored overlays and apply them to the actor.");
							}
						}

						ImGui::SameLine();

						{
							if (ImGui::Button("Clear All RM Ovls")) {
								ClearGameOverlays(actor);
							}
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("Remove All Racemenu-side overlays without modifying the stored overlay data.");
							}
						}

						ImGui::Spacing();
						DrawOverlayEntry(actor);
					}

					
				}
			}
		}
	}
}