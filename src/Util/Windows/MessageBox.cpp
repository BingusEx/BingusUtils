#include "Messagebox.hpp"
#include "Util/Text/Text.hpp"

namespace {

	HWND GetHandle() {

		const RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
		if (!renderer) {
			return nullptr;
		}

		const RE::BSGraphics::RendererWindow* window = RE::BSGraphics::Renderer::GetCurrentRenderWindow();
		if (!window) {
			return nullptr;
		}

		return reinterpret_cast<HWND>(window->hWnd);
	}

	auto force_show_cursor = [] {
		while (ShowCursor(TRUE) < 0) {}
	};

	auto restore_cursor_hide = [] {
		while (ShowCursor(FALSE) >= 0) {}
	};
}

namespace Util::Win32 {

	const wchar_t* const CloseMsg = L"\nThe game will now close.";

	void ReportAndExit(std::string_view a_message) {
		std::wstring wideMessage = Util::Text::Utf8ToUtf16(a_message);
		wideMessage += CloseMsg;

		force_show_cursor();

		MessageBoxW(
			GetHandle(),
			wideMessage.data(),
			L"Size Matters - GtsPlugin.dll",
			MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND
		);

		restore_cursor_hide();

		REX::W32::TerminateProcess(REX::W32::GetCurrentProcess(), EXIT_FAILURE);
	}

	void ReportAndExit(std::wstring_view a_message) {
		std::wstring wideMessage(a_message);
		wideMessage += CloseMsg;

		force_show_cursor();

		MessageBoxW(
			GetHandle(),
			wideMessage.data(),
			L"Size Matters - GtsPlugin.dll",
			MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND
		);

		restore_cursor_hide();

		REX::W32::TerminateProcess(REX::W32::GetCurrentProcess(), EXIT_FAILURE);
	}

	void ReportInfo(std::string_view a_message) {
		std::wstring wideMessage = Util::Text::Utf8ToUtf16(a_message);

		force_show_cursor();

		MessageBoxW(
			GetHandle(),
			wideMessage.data(),
			L"Size Matters - GtsPlugin.dll",
			MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND
		);

		restore_cursor_hide();
	}

	void ReportInfo(std::wstring_view a_message) {
		std::wstring wideMessage(a_message);
		wideMessage += CloseMsg;

		force_show_cursor();

		MessageBoxW(
			GetHandle(),
			wideMessage.data(),
			L"Size Matters - GtsPlugin.dll",
			MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND
		);

		restore_cursor_hide();
	}
}