#pragma once

#include "Defines.h"

void DrawEditorLineNumbers(HWND hEditorWnd, HDC hdc);
void InvalidateEditorLineNumbers(HWND hEditorWnd);

void CenterEdittoLine(HWND hEdit, int lLine);

void HighlightRichEditLine(HWND hEdit, int lLine, COLORREF backColor = RGB(255,250,200));
void ShowFileEditor(const std::wstring& path, int highlightLine = 0);

LRESULT CALLBACK LineNumberWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditorEditSubclassProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData);
LRESULT CALLBACK EditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
