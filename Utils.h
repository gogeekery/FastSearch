#pragma once
#include "Defines.h"


// Overload - to avoid needing std::string conversions (they're slow)
std::wstring Utf8ToWString(std::string_view s);
std::string WStringToUtf8(const std::wstring& w);
std::wstring Utf8ToWString(const std::string& s);


// TODO: Refactor this into a Theme.cpp and make a UI.cpp for file dialogs etc

bool IsLightMode();
void RecreateThemeBrushes();

std::wstring GetWindowTextWstr(HWND h);
void ListView_SetRedraw(HWND hwnd, bool fRedraw);

