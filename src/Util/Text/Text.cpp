#include "Util/Text/Text.hpp"

namespace Util::Text {

	bool StartsWith(std::string_view a_baseStr, std::string_view a_preffix) {
		return a_baseStr.compare(0, a_preffix.size(), a_preffix);
	}

	bool Regex_Matches(std::string_view a_baseStr, std::string_view a_reg) {
		std::regex reg(std::string(a_reg).c_str());
		return std::regex_match(std::string(a_baseStr), reg);
	}

	std::string ToLower(std::string a_str) {
		std::ranges::transform(a_str, a_str.begin(),[](unsigned char c){
			return std::tolower(c);
		});
		return a_str;
	}

	std::string ToUpper(std::string a_str) {
		std::ranges::transform(a_str, a_str.begin(),[](unsigned char c){
			return std::toupper(c);
		});
		return a_str;
	}

	void ReplaceFirst(std::string& s, std::string const& toReplace, std::string const& replaceWith) {
		std::size_t pos = s.find(toReplace);
		if (pos == std::string::npos) {
			return;
		}
		s.replace(pos, toReplace.length(), replaceWith);
	}

	std::string RemoveWhitespace(std::string a_str) {
		a_str.erase(std::ranges::remove(a_str,' ').begin(), a_str.end());
		return a_str;
	}

	std::string Trim(const std::string& s) {
		auto start = s.begin();
		while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
			++start;
		}

		auto end = s.end();
		do {
			--end;
		}
		while (end != start && std::isspace(static_cast<unsigned char>(*end)));

		return std::string(start, end + 1);
	}

	void TrimL(std::string& s) {
		s.erase(s.begin(), std::ranges::find_if(s,[](unsigned char ch) {
			return !std::isspace(ch);
		}));
	}

	void TrimR(std::string& s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
		}).base(), s.end());
	}

	std::wstring Utf8ToUtf16(std::string_view a_utf8) {
		if (a_utf8.empty()) return {};
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, a_utf8.data(), static_cast<int>(a_utf8.size()), nullptr, 0);
		std::wstring result(size_needed, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, a_utf8.data(), static_cast<int>(a_utf8.size()), result.data(), size_needed);
		return result;
	}

	std::string Utf16ToUtf8(std::wstring_view a_utf16) {
		if (a_utf16.empty()) return {};
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, a_utf16.data(), static_cast<int>(a_utf16.size()), nullptr, 0, nullptr, nullptr);
		std::string result(size_needed, '\0');
		WideCharToMultiByte(CP_UTF8, 0, a_utf16.data(), static_cast<int>(a_utf16.size()), result.data(), size_needed, nullptr, nullptr);
		return result;
	}

	bool ContainsInvariantStr(std::string_view a_Source, std::string_view a_Substr) {
		if (a_Substr.empty()) {
			return true;
		}

		const std::locale& loc = std::locale::classic();

		auto it = std::ranges::search(a_Source, a_Substr, [&loc](char ch1, char ch2) {
			return std::tolower(ch1, loc) == std::tolower(ch2, loc);
			}).begin();

		return it != a_Source.end();
	}

}
