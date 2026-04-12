#pragma once 
#include <re2/re2.h>

namespace BU::Utils {

	inline std::vector<RE::Actor*> GetAllLoadedActors(bool a_sortByDst) {

		const RE::ProcessLists* const process_list = RE::ProcessLists::GetSingleton();
		const auto& handles = process_list->highActorHandles;
		const std::size_t n = handles.size();

		std::vector<RE::Actor*> result;
		result.reserve(n + 1);

		for (std::size_t i = 0; i < n; ++i) {
			if (const auto& a = handles[i].get(); a && a->Get3D1(false)) {
				result.emplace_back(a.get());
			}
		}

		if (RE::Actor* player = RE::PlayerCharacter::GetSingleton(); player && player->Get3D1(false)) {
			result.emplace_back(player);
		}


        if (a_sortByDst) {
            std::ranges::sort(result, [](RE::Actor* a, RE::Actor* b) {
                return a->GetDistance(RE::PlayerCharacter::GetSingleton()) < b->GetDistance(RE::PlayerCharacter::GetSingleton());
            });
        }

		return result;
	}

    static std::string GetFormName(const RE::TESForm* form) {
        if (!form) {
            return "[Null]";
        }

        if (const auto* fullName = form->As<RE::TESFullName>(); fullName && fullName->GetFullNameLength() > 0) {
            return fullName->GetFullName();
        }

        return std::format("{} [{:08X}]", form->GetName(), form->GetFormID());
    }

    static std::string GetRefDisplayName(RE::TESObjectREFR* ref) {
        if (!ref) {
            return "[Null]";
        }

        if (const auto* base = ref->GetObjectReference()) {
            if (const auto* fullName = base->As<RE::TESFullName>(); fullName && fullName->GetFullNameLength() > 0) {
                return std::format("{} [{:08X}]", fullName->GetFullName(), ref->GetFormID());
            }
        }

        return std::format("Ref [{:08X}]", ref->GetFormID());
    }


    namespace Regex {

        inline std::string EscapeRe2Literal(std::string_view s) {
        	std::string out;
            out.reserve(s.size() * 2);

            for (char c : s) {
                switch (c) {
                case '\\': case '^': case '$': case '.': case '|': case '?': case '*': case '+':
                case '(': case ')': case '[': case ']': case '{': case '}':
                    out.push_back('\\');
                    [[fallthrough]];
                default:
                    out.push_back(c);
                }
            }
            return out;
        }

        inline std::unique_ptr<RE2> Re2Check(std::string_view pattern, RE2::Options opt = [] { RE2::Options o; o.set_case_sensitive(false); o.set_log_errors(false); return o;} () ){

            auto rx = std::make_unique<RE2>(re2::StringPiece(pattern.data(), pattern.size()), opt);
            if (rx->ok()) {
                return rx;
            }

            const auto escaped = EscapeRe2Literal(pattern);
            rx = std::make_unique<RE2>(re2::StringPiece(escaped.data(), escaped.size()), opt);
            if (rx->ok()) {
                return rx;
            }

            return std::make_unique<RE2>(re2::StringPiece("$a", 2), opt); // match nothing
        }

        inline bool SearchSafe(const char* s, const RE2& rx){
            if (!s || !*s) {
                return false;
            }
            return RE2::PartialMatch(s, rx);
        }
    }
}