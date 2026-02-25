#include "Common/Utilities.hpp"
#include "Features/OverlayUtils/OverlayUtils.hpp"
#include "Common/SKEE/SKEE.hpp"


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

	void OverlayTools::ClearStoredOverlays(RE::Actor* a_actor) {
		if (!a_actor) {
			return;
		}
		std::unique_lock lock(_Lock);
		if (auto it = ActorData.value.find(a_actor->formID); it != ActorData.value.end()) {
			it->second.OvlFace.clear();
			it->second.OvlBody.clear();
			it->second.OvlHands.clear();
			it->second.OvlFeet.clear();
		}
	}

	void OverlayTools::BuildOverlayList(RE::Actor* a_actor) {
		if (!SKEE::Overlays::Loaded() || !a_actor) {
			return;
		}

		auto faceIdxs = SKEE::Overlays::GetUsedSlotIndices(a_actor, SKEE::Overlays::FaceInfo);
		auto bodyIdxs = SKEE::Overlays::GetUsedSlotIndices(a_actor, SKEE::Overlays::BodyInfo);
		auto handsIdxs = SKEE::Overlays::GetUsedSlotIndices(a_actor, SKEE::Overlays::HandsInfo);
		auto feetIdxs = SKEE::Overlays::GetUsedSlotIndices(a_actor, SKEE::Overlays::FeetInfo);

		std::vector<OverlayEntry> face, body, hands, feet;

		for (uint16_t idx : faceIdxs) {
			SKEE::Overlays::BuildOverlayAtIdx(a_actor, SKEE::Overlays::FaceInfo, idx, &face.emplace_back());
		}
		for (uint16_t idx : bodyIdxs) {
			SKEE::Overlays::BuildOverlayAtIdx(a_actor, SKEE::Overlays::BodyInfo, idx, &body.emplace_back());
		}
		for (uint16_t idx : handsIdxs) {
			SKEE::Overlays::BuildOverlayAtIdx(a_actor, SKEE::Overlays::HandsInfo, idx, &hands.emplace_back());
		}
		for (uint16_t idx : feetIdxs) {
			SKEE::Overlays::BuildOverlayAtIdx(a_actor, SKEE::Overlays::FeetInfo, idx, &feet.emplace_back());
		}

		std::unique_lock lock(_Lock);
		auto it = ActorData.value.find(a_actor->formID);
		if (it == ActorData.value.end()) {
			return;
		}
		auto& data = it->second;
		data.OvlFace = std::move(face);
		data.OvlBody = std::move(body);
		data.OvlHands = std::move(hands);
		data.OvlFeet = std::move(feet);
		data.AlreadyApplied = false;
	}

	void OverlayTools::ApplyStoredOvls(RE::Actor* a_actor) {
		if (!SKEE::Overlays::Loaded() || !a_actor) {
			return;
		}

		std::vector<OverlayEntry> body, face, hands, feet;
		{
			std::unique_lock lock(_Lock);
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
	}

	void OverlayTools::AddNewActor(RE::Actor* a_actor) {
		if (!a_actor) {
			return;
		}

		std::unique_lock lock(_Lock);
		auto [it, inserted] = ActorData.value.emplace(a_actor->formID, Data{});
		it->second.AlreadyApplied = false;
		lock.unlock();

		BuildOverlayList(a_actor);
		ApplyStoredOvls(a_actor);
	}

	void OverlayTools::DrawOverlayEntry(RE::Actor* a_actor) {
		if (!a_actor) {
			ImGui::TextUnformatted("<Invalid Actor>");
			return;
		}
		if (!SKEE::Overlays::Loaded()) {
			ImGui::TextUnformatted("SKEE Overlays not loaded.");
			return;
		}

		std::unique_lock lock(_Lock);
		auto it = ActorData.value.find(a_actor->formID);
		if (it == ActorData.value.end()) {
			ImGui::TextUnformatted("Actor not tracked.");
			return;
		}
		Data& data = it->second;

		auto draw_list = [&](const char* header, std::vector<OverlayEntry>& list, uint8_t loc) {
			const auto* info = GetBodyPartInfo(loc);
			const int16_t maxCount = info ? info->Count : 0;

			if (!ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
				return;
			}

			ImGui::PushID(header);

			if (ImGui::Button("Capture From Actor")) {
				lock.unlock();
				BuildOverlayList(a_actor);
				lock.lock();
				data.AlreadyApplied = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("Apply To Actor")) {
				lock.unlock();
				ApplyStoredOvls(a_actor);
				lock.lock();
				data.AlreadyApplied = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Stored")) {
				list.clear();
				data.AlreadyApplied = false;
			}

			ImGui::SameLine();
			if (ImGui::Button("Clear Currently Applied")) {
				SKEE::Overlays::ClearBodyPart(a_actor, *info);
				data.AlreadyApplied = false;
			}

			if (maxCount > 0) {
				ImGui::Separator();
				if (ImGui::Button("Compact -> Top")) {
					SKEE::Overlays::ClearBodyPart(a_actor, *info);
					ReindexCompactTop(list);
					data.AlreadyApplied = false;
				}
				ImGui::SameLine();
				if (ImGui::Button("Compact -> Bottom")) {
					SKEE::Overlays::ClearBodyPart(a_actor, *info);
					ReindexCompactBottom(list, maxCount);
					data.AlreadyApplied = false;
				}
				ImGui::SameLine();
				if (ImGui::Button("Flip")) {
					SKEE::Overlays::ClearBodyPart(a_actor, *info);
					ReindexFlip(list, maxCount);
					data.AlreadyApplied = false;
				}
			}

			ImGui::Separator();
			if (ImGui::Button("Add Overlay")) {
				OverlayEntry e{};
				e.BodyPart = loc;
				e.Index = 0;
				e.DiffuseAlpha = 1.0f;
				e.DiffuseColor = 0xFFFFFFu;
				e.TexturePath = "textures\\actors\\character\\overlays\\default.dds";
				list.emplace_back(std::move(e));
				data.AlreadyApplied = false;
			}

			ImGui::Separator();

			for (size_t i = 0; i < list.size(); ++i) {
				auto& ovl = list[i];
				ovl.BodyPart = loc;

				ImGui::PushID(static_cast<int>(i));

				ImGui::Text("#%zu", i);
				ImGui::SameLine();
				if (ImGui::SmallButton("Remove")) {
					list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
					--i;
					data.AlreadyApplied = false;
					ImGui::PopID();
					continue;
				}

				int idx = ovl.Index;
				const int maxIdx = std::max<int>(0, maxCount - 1);
				if (ImGui::SliderInt("Index", &idx, 0, maxIdx)) {
					ovl.Index = static_cast<int16_t>(idx);
					data.AlreadyApplied = false;
				}

				float alpha = ovl.DiffuseAlpha;
				if (ImGui::SliderFloat("Alpha", &alpha, 0.0f, 1.0f)) {
					ovl.DiffuseAlpha = alpha;
					data.AlreadyApplied = false;
				}

				std::array<float, 4> col4{};
				{
					std::array<float, 3> rgb{};
					RGBToFloat3(ovl.DiffuseColor, rgb);
					col4[0] = rgb[0];
					col4[1] = rgb[1];
					col4[2] = rgb[2];
					col4[3] = 1.0f; // dummy alpha
				}

				if (ImGui::ColorEdit4("Color", col4.data(), ImGuiColorEditFlags_NoAlpha)) {
					std::array rgb{ col4[0], col4[1], col4[2] };
					const uint32_t keep = ovl.DiffuseColor & 0xFF000000u;
					ovl.DiffuseColor = keep | Float3ToRGB(rgb);
					data.AlreadyApplied = false;
				}

				char buf[512]{};
				std::strncpy(buf, ovl.TexturePath.c_str(), sizeof(buf) - 1);
				if (ImGui::InputText("Texture", buf, sizeof(buf))) {
					ovl.TexturePath = buf;
					data.AlreadyApplied = false;
				}

				ImGui::Separator();
				ImGui::PopID();
			}

			ImGui::PopID();
		};

		draw_list("Body", data.OvlBody, static_cast<uint8_t>(BodyLoc::Body));
		draw_list("Face", data.OvlFace, static_cast<uint8_t>(BodyLoc::Face));
		draw_list("Hands", data.OvlHands, static_cast<uint8_t>(BodyLoc::Hand));
		draw_list("Feet", data.OvlFeet, static_cast<uint8_t>(BodyLoc::Feet));
	}

	void OverlayTools::OnSerdeSave(SKSE::SerializationInterface* a_this) {
		std::unique_lock lock(_Lock);
		ActorData.Save(a_this);
	}

	void OverlayTools::OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {
		std::unique_lock lock(_Lock);
		ActorData.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
	}

	void OverlayTools::OnSerdePostLoad() {
		std::unique_lock lock(_Lock);
		for (auto& data : ActorData.value | std::views::values) {
			data.AlreadyApplied = false;
		}
	}

	void OverlayTools::OnActorReset(RE::Actor* a_actor) { RemoveActor(a_actor); }

	void OverlayTools::OnActorUpdate(RE::Actor* a_actor) {
		if (!a_actor || !SKEE::Overlays::Loaded()) {
			return;
		}

		std::unique_lock lock(_Lock);
		auto it = ActorData.value.find(a_actor->formID);
		if (it == ActorData.value.end() || it->second.AlreadyApplied || !a_actor->Is3DLoaded()) {
			return;
		}

		lock.unlock();
		ApplyStoredOvls(a_actor);
		lock.lock();
		it->second.AlreadyApplied = true;
	}

	void OverlayTools::OnActorLoad3D(RE::Actor* a_actor) {
		if (!a_actor || !SKEE::Overlays::Loaded()) {
			return;
		}

		std::unique_lock lock(_Lock);
		auto it = ActorData.value.find(a_actor->formID);
		if (it == ActorData.value.end()) {
			return;
		}

		lock.unlock();
		ApplyStoredOvls(a_actor);
		lock.lock();
		it->second.AlreadyApplied = true;
	}

	void OverlayTools::OnActorEquip(RE::Actor* a_actor) { InvalidateData(a_actor); }
	void OverlayTools::OnActorUnequip(RE::Actor* a_actor) { InvalidateData(a_actor); }

	void OverlayTools::RemoveActor(RE::Actor* a_actor) {
		if (!a_actor) {
			return;
		}
		std::unique_lock lock(_Lock);
		ActorData.value.erase(a_actor->formID);
	}

	void OverlayTools::InvalidateData(RE::Actor* a_actor) {
		if (!a_actor || !SKEE::Overlays::Loaded()) {
			return;
		}

		{
			std::unique_lock lock(_Lock);
			auto it = ActorData.value.find(a_actor->formID);
			if (it == ActorData.value.end()) {
				return;
			}
			it->second.AlreadyApplied = false;
		}

		BuildOverlayList(a_actor);

		if (a_actor->Is3DLoaded()) {
			ApplyStoredOvls(a_actor);
			std::unique_lock lock(_Lock);
			if (auto it = ActorData.value.find(a_actor->formID); it != ActorData.value.end()) {
				it->second.AlreadyApplied = true;
			}
		}
	}

	void OverlayTools::Draw() {
		std::unique_lock lock(_Lock);

		if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto actorList = Utils::GetAllLoadedActors();

			static RE::ActorHandle s_selected{};
			const char* preview = "<Select Actor>";
			RE::FormID previewID = 0;

			if (auto sel = s_selected.get()) {
				preview = sel->GetDisplayFullName();
				previewID = sel->GetFormID();
			}

			const std::string previewStr =
				(previewID != 0) ? fmt::format("{} [0x{:08X}]", preview, previewID) : std::string(preview);

			if (ImGui::BeginCombo("##AddActor", previewStr.c_str())) {
				for (RE::Actor* actor : actorList) {
					if (!actor) {
						continue;
					}
					const auto name = actor->GetDisplayFullName();
					const auto id = actor->GetFormID();
					const std::string item = fmt::format("{} [0x{:08X}]", name ? name : "<Unnamed>", id);

					const bool is_selected = (s_selected.get().get() == actor);
					if (ImGui::Selectable(item.c_str(), is_selected)) {
						s_selected = actor->GetHandle();
					}
					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();

			if (ImGui::Button("Add New Actor")) {
				if (RE::Actor* actor = s_selected.get().get()) {
					lock.unlock();
					AddNewActor(actor);
					lock.lock();
				}
			}
		}

		if (ImGui::CollapsingHeader("Edit Actor", ImGuiTreeNodeFlags_DefaultOpen)) {
			static RE::FormID s_selectedID = 0;

			if (ActorData.value.empty()) {
				ImGui::TextUnformatted("No actors added.");
				return;
			}

			std::string preview = "<Select Actor>";
			if (s_selectedID != 0) {
				if (auto* form = RE::TESForm::LookupByID(s_selectedID)) {
					if (auto* actor = form->As<RE::Actor>()) {
						const char* name = actor->GetDisplayFullName();
						preview = fmt::format("{} [0x{:08X}]", name ? name : "<Unnamed>", s_selectedID);
					}
					else {
						preview = fmt::format("<Not An Actor> [0x{:08X}]", s_selectedID);
					}
				}
				else {
					preview = fmt::format("<Missing> [0x{:08X}]", s_selectedID);
				}
			}

			if (ImGui::BeginCombo("Actor##Edit", preview.c_str())) {
				for (const auto& key : ActorData.value | std::views::keys) {
					RE::Actor* actor = nullptr;
					if (auto* form = RE::TESForm::LookupByID(key)) {
						actor = form->As<RE::Actor>();
					}
					const char* name = (actor && actor->GetDisplayFullName()) ? actor->GetDisplayFullName() : "<Unnamed>";
					const std::string label = fmt::format("{} [0x{:08X}]", name, key);

					const bool is_selected = (s_selectedID == key);
					if (ImGui::Selectable(label.c_str(), is_selected)) {
						s_selectedID = key;
					}
					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (s_selectedID != 0) {
				ImGui::SameLine();

				auto it = ActorData.value.find(s_selectedID);
				if (it == ActorData.value.end()) {
					ImGui::TextUnformatted("Selected actor no longer exists in ActorData.");
					return;
				}

				RE::Actor* actor = nullptr;
				if (auto* form = RE::TESForm::LookupByID(s_selectedID)) {
					actor = form->As<RE::Actor>();
				}

				if (ImGui::Button("Remove##Edit")) {
					const RE::FormID removeID = s_selectedID;
					ActorData.value.erase(removeID);

					s_selectedID = 0;
					if (!ActorData.value.empty()) {
						s_selectedID = ActorData.value.begin()->first;
					}
					return;
				}

				ImGui::Separator();

				if (actor) {
					if (ImGui::Button("Capture All From Actor")) {
						lock.unlock();
						BuildOverlayList(actor);
						lock.lock();
						if (auto it2 = ActorData.value.find(s_selectedID); it2 != ActorData.value.end()) {
							it2->second.AlreadyApplied = false;
						}
					}
					ImGui::SameLine();
					if (ImGui::Button("Apply All To Actor")) {
						lock.unlock();
						ApplyStoredOvls(actor);
						lock.lock();
						if (auto it2 = ActorData.value.find(s_selectedID); it2 != ActorData.value.end()) {
							it2->second.AlreadyApplied = true;
						}
					}
					ImGui::SameLine();
					if (ImGui::Button("Invalidate")) {
						lock.unlock();
						InvalidateData(actor);
						lock.lock();
					}

					ImGui::Separator();

					lock.unlock();
					DrawOverlayEntry(actor);
					lock.lock();
				}
			}
		}
	}
}