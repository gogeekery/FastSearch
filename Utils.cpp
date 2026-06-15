#include "Utils.h"


// Overload - to avoid needing std::string conversions (they're slow)
std::wstring Utf8ToWString(std::string_view s) {
	if (s.empty()) return std::wstring();

	int n = MultiByteToWideChar(
		CP_UTF8,
		0,
		s.data(),
		(int)s.size(),
		nullptr,
		0
	);

	if (n <= 0) return std::wstring();

	std::wstring w;
	w.resize(n);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		s.data(),
		(int)s.size(),
		&w[0],
		n
	);

	return w;
}

std::string WStringToUtf8(const std::wstring& w) {
	if (w.empty()) return std::string();
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
	if (n <= 0) return std::string();
	std::string s; s.resize(n);
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
	return s;
}

std::wstring Utf8ToWString(const std::string& s) {
	if (s.empty()) return std::wstring();
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
	if (n <= 0) return std::wstring();
	std::wstring w; w.resize(n);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
	return w;
}



// TODO: Refactor this into a Theme.cpp and make a UI.cpp

bool IsLightMode() {
	// Checks the registry to see if user enabled dark themes
	DWORD value = 1;
	DWORD size = sizeof(value);
	LSTATUS st = RegGetValueW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &size);
	return (st == ERROR_SUCCESS) ? (value != 0) : true; // defaults to light
}

void RecreateThemeBrushes() {
	// Delete old brushes if any
	if (g_hbrWindow) { DeleteObject(g_hbrWindow); g_hbrWindow = NULL; }
	if (g_hbrEdit)   { DeleteObject(g_hbrEdit);   g_hbrEdit = NULL; }
	if (g_hbrStatic) { DeleteObject(g_hbrStatic); g_hbrStatic = NULL; }

	if (g_isLightMode) {
		g_hbrWindow = GetSysColorBrush(COLOR_WINDOW);
		g_hbrEdit   = GetSysColorBrush(COLOR_WINDOW);
		g_hbrStatic = GetSysColorBrush(COLOR_WINDOW);
	} else {
		g_hbrWindow = CreateSolidBrush(RGB(32,32,32));
		g_hbrEdit   = CreateSolidBrush(RGB(24,24,24));
		g_hbrStatic = CreateSolidBrush(RGB(32,32,32));
	}
}


std::wstring GetWindowTextWstr(HWND h) {
	int len = GetWindowTextLengthW(h);
	std::wstring s(len, L'\0');
	GetWindowTextW(h, s.data(), len + 1);
	return s;
}

void ListView_SetRedraw(HWND hwnd, bool fRedraw) {
	// Send the WM_SETREDRAW message to the control
	SendMessage(hwnd, WM_SETREDRAW, (WPARAM)(fRedraw ? TRUE : FALSE), 0);

	// If we just re-enabled redrawing, we must force a repaint
	if (fRedraw) {
		InvalidateRect(hwnd, NULL, FALSE);
	}
}

