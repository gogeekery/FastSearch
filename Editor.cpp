
#include "Editor.h"
#include "Utils.h"
#include "Search.h"


const int ID_MENU_SAVE        = 40001;
const int ID_MENU_SAVE_AS     = 40002;
const int ID_MENU_SHOWFOLDER  = 40003;
const int ID_MENU_OPENEXT     = 40004;

const int ID_EDITOR_EDIT = 2001;
const int ID_EDITOR_SAVE = 2002;
const int ID_EDITOR_CLOSE = 2003;



void DrawEditorLineNumbers(HWND hEditorWnd, HDC hdc) {
	HWND hEdit = (HWND)GetPropW(hEditorWnd, L"EditHWND");
	HWND hLineNums = (HWND)GetPropW(hEditorWnd, L"LineNumsHWND");
	if (!hEdit || !hLineNums || !hdc)
		return;

	HFONT hEditFont = (HFONT)SendMessageW(hEdit, WM_GETFONT, 0, 0);
	HFONT hOld = nullptr;
	if (hEditFont)
		hOld = (HFONT)SelectObject(hdc, hEditFont);

	RECT rcGutter{};
	GetClientRect(hLineNums, &rcGutter);

	HBRUSH hbrLine = CreateSolidBrush(RGB(225, 240, 255));
	FillRect(hdc, &rcGutter, hbrLine);

	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(60, 90, 120));

	TEXTMETRIC tm{};
	GetTextMetrics(hdc, &tm);
	int lineHeight = std::max(1, (int)(tm.tmHeight + tm.tmExternalLeading));

	RECT rcEdit{};
	GetClientRect(hEdit, &rcEdit);

	int firstLine = (int)SendMessageW(hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
	int totalLines = (int)SendMessageW(hEdit, EM_GETLINECOUNT, 0, 0);

	int visibleLines = ((rcEdit.bottom - rcEdit.top) / lineHeight) + 3;

	for (int line = firstLine; line < totalLines && line < firstLine + visibleLines; ++line) {
		LRESULT charIndex = SendMessageW(hEdit, EM_LINEINDEX, line, 0);
		if (charIndex < 0)
			break;

		POINTL pt{};
		SendMessageW(hEdit, EM_POSFROMCHAR, (WPARAM)&pt, (LPARAM)charIndex);

		int y = (int)pt.y;
		if (y > rcEdit.bottom)
			break;

		RECT r = rcGutter;
		r.top = y;
		r.bottom = y + lineHeight;
		r.right -= 6;

		std::wstring num = std::to_wstring(line + 1);

		DrawTextW(
			hdc,
			num.c_str(),
			(int)num.size(),
			&r,
			DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER
		);
	}

	if (hOld)
		SelectObject(hdc, hOld);

	DeleteObject(hbrLine);
}

void InvalidateEditorLineNumbers(HWND hEditorWnd) {
	HWND hLineNums = (HWND)GetPropW(hEditorWnd, L"LineNumsHWND");
	if (hLineNums)
		InvalidateRect(hLineNums, NULL, TRUE);
}


void CenterEdittoLine(HWND hEdit, int lLine)
{
	if (!hEdit || lLine <= 0)
		return;

	int targetLine = lLine - 1;

	// Get character index of start of the logical line
	int charIndex = (int)SendMessageW(hEdit, EM_LINEINDEX, targetLine, 0);
	if (charIndex < 0)
		return;

	// Move caret to start of target line (no visible selection change)
	CHARRANGE newSel{ charIndex, charIndex };
	SendMessageW(hEdit, EM_EXSETSEL, 0, (LPARAM)&newSel);

	// Get visible line count
	RECT rc{};
	GetClientRect(hEdit, &rc);

	HDC hdc = GetDC(hEdit);
	HFONT hFont = (HFONT)SendMessageW(hEdit, WM_GETFONT, 0, 0);
	HFONT hOld = (HFONT)SelectObject(hdc, hFont);

	TEXTMETRIC tm{};
	GetTextMetrics(hdc, &tm);

	SelectObject(hdc, hOld);
	ReleaseDC(hEdit, hdc);

	int lineHeight = std::max(1, (int)(tm.tmHeight + tm.tmExternalLeading));
	int visibleLines = std::max(1, (int)((rc.bottom - rc.top) / lineHeight));

	// Determine current visual line
	int visualLine = (int)SendMessageW(hEdit, EM_LINEFROMCHAR, charIndex, 0);
	int firstVisible = (int)SendMessageW(hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);

	// Compute desired first visible line to center target
	int desiredFirst = visualLine - (visibleLines / 2);
	if (desiredFirst < 0)
		desiredFirst = 0;

	int scrollDelta = desiredFirst - firstVisible;
	if (scrollDelta != 0)
		SendMessageW(hEdit, EM_LINESCROLL, 0, scrollDelta);

}


void HighlightRichEditLine(HWND hEdit, int lLine, COLORREF backColor)
{
	if (!hEdit || lLine <= 0)
		return;

	int targetLine = lLine - 1;

	// Get start of the logical line
	int startIndex = (int)SendMessageW(hEdit, EM_LINEINDEX, targetLine, 0);
	if (startIndex < 0)
		return;


	CHARRANGE tmpSel{ startIndex, startIndex };
	SendMessageW(hEdit, EM_EXSETSEL, 0, (LPARAM)&tmpSel);
	SendMessageW(hEdit, EM_SCROLLCARET, 0, 0); // ensures line is realized

	// Get end of the logical line
	int endIndex = (int)SendMessageW(hEdit, EM_LINEINDEX, targetLine + 1, 0);
	if (endIndex < 0)
		endIndex = GetWindowTextLengthW(hEdit);

	// Select the target line
	CHARRANGE lineSel{ startIndex, endIndex };
	SendMessageW(hEdit, EM_EXSETSEL, 0, (LPARAM)&lineSel);

	// Apply background color
	CHARFORMAT2W cf{};
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_BACKCOLOR;
	cf.crBackColor = backColor;
	SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

	RedrawWindow(hEdit, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}




void ShowFileEditor(const std::wstring& path, int highlightLine) {
	HWND hEditorWnd = CreateWindowExW(0, EDITOR_CLASS, path.c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
		NULL, NULL, GetModuleHandle(NULL), NULL);

	HMENU hMenu = CreateMenu();
	HMENU hFile = CreatePopupMenu();
	AppendMenuW(hFile, MF_STRING, ID_MENU_SAVE,       L"Save");
	AppendMenuW(hFile, MF_STRING, ID_MENU_SAVE_AS,    L"Save As...");
	AppendMenuW(hFile, MF_STRING, ID_MENU_SHOWFOLDER, L"Show in Folder");
	AppendMenuW(hFile, MF_STRING, ID_MENU_OPENEXT,    L"Open Externally");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"File");
	SetMenu(hEditorWnd, hMenu);

	HWND hLineNums = CreateWindowExW(
		0,
		LINE_NUM_CLASS,
		L"",
		WS_CHILD | WS_VISIBLE,
		0, 0, 60, 700,
		hEditorWnd,
		NULL,
		GetModuleHandle(NULL),
		NULL
	);

	HWND hEdit = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
					WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,// | ES_READONLY,
					60, 0, 840, 700, hEditorWnd, (HMENU)ID_EDITOR_EDIT, GetModuleHandle(NULL), NULL);

	// Load file into UTF-16 string (same as before)
	std::wstring contentW;
	{
		std::ifstream ifs(WStringToUtf8(path), std::ios::binary);
		if (ifs.good()) {
			ifs.seekg(0, std::ios::end);
			std::streamoff sz = ifs.tellg();
			ifs.seekg(0, std::ios::beg);
			std::string buf;
			buf.reserve((size_t)std::max<std::streamoff>(sz, 4096));
			buf.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

			if (buf.size() >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
				contentW = Utf8ToWString(std::string(buf.data() + 3, buf.size() - 3));
			} else if (buf.size() >= 2 && ((unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE)) {
				size_t n = (buf.size() - 2) / 2;
				contentW.resize(n);
				memcpy(&contentW[0], buf.data() + 2, n * sizeof(wchar_t));
			} else if (buf.size() >= 2 && ((unsigned char)buf[0] == 0xFE && (unsigned char)buf[1] == 0xFF)) {
				size_t n = (buf.size() - 2) / 2;
				contentW.resize(n);
				for (size_t i = 0; i < n; ++i) {
					unsigned char hi = (unsigned char)buf[2 + i*2];
					unsigned char lo = (unsigned char)buf[2 + i*2 + 1];
					contentW[i] = (wchar_t)((hi << 8) | lo);
				}
			} else {
				contentW = Utf8ToWString(buf);
			}
		}
	}

	// TODO: Fix memory leak here on font creation - WS
	// SetPropW(hEditorWnd, L"Font", hFont);
	HFONT hFont = CreateFontW(
		-MulDiv(11, GetDeviceCaps(GetDC(hEdit), LOGPIXELSY), 72),
		0,0,0,FW_NORMAL, FALSE,FALSE,FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, FF_MODERN, L"Consolas"
	);
	SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

	// Put text into control
	SetWindowTextW(hEdit, contentW.c_str());

	// Store props
	SetPropW(hEditorWnd, L"EditHWND", hEdit);
	SetPropW(hEditorWnd, L"LineNumsHWND", hLineNums);
	SetPropW(hEditorWnd, L"FilePath", (HANDLE)_wcsdup(path.c_str()));

	SetWindowSubclass(hEdit, EditorEditSubclassProc, 1, (DWORD_PTR)hEditorWnd);

	CenterEdittoLine(hEdit, highlightLine);			// Center to highlight
	HighlightRichEditLine(hEdit, highlightLine);	// Highlight breaks centering??
	CenterEdittoLine(hEdit, highlightLine);			// Re-center

	// DrawEditorLineNumbers(hEditorWnd);
	InvalidateEditorLineNumbers(hEditorWnd);

}



LRESULT CALLBACK LineNumberWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps{};
		HDC hdc = BeginPaint(hWnd, &ps);
		HWND hEditorWnd = GetParent(hWnd);
		DrawEditorLineNumbers(hEditorWnd, hdc);
		EndPaint(hWnd, &ps);
		return 0;
	}

	case WM_ERASEBKGND:
		return 1;

	default:
		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}
}





LRESULT CALLBACK EditorEditSubclassProc(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR,
	DWORD_PTR dwRefData)
{
	HWND hEditorWnd = (HWND)dwRefData;

	LRESULT result = DefSubclassProc(hWnd, msg, wParam, lParam);

	switch (msg) {
	case WM_VSCROLL:
	case WM_MOUSEWHEEL:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_SIZE:
	case WM_PAINT:
	case EM_LINESCROLL:
		InvalidateEditorLineNumbers(hEditorWnd);
		break;
	}

	return result;
}


// --------- Editor UI ---------------
LRESULT CALLBACK EditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		return 0;

	case WM_SIZE: {
		HWND hEdit = (HWND)GetPropW(hWnd, L"EditHWND");
		HWND hLine = (HWND)GetPropW(hWnd, L"LineNumsHWND");
		if (hEdit && hLine) {
			RECT r; GetClientRect(hWnd, &r);
			int width = r.right - r.left;
			int height = r.bottom - r.top;
			MoveWindow(hLine, 0, 0, 60, height, TRUE);
			MoveWindow(hEdit, 60, 0, width - 60, height, TRUE);
			// InvalidateRect(hEdit, NULL, TRUE);
			// UpdateWindow(hEdit);
			// DrawEditorLineNumbers(hWnd);
			InvalidateRect(hEdit, NULL, TRUE);
			InvalidateEditorLineNumbers(hWnd);
		}
		return 0;
	}

	case WM_COMMAND: {
		int id = LOWORD(wParam);
		int ev = HIWORD(wParam);
		HWND ctrl = (HWND)lParam;
		HWND hEdit = (HWND)GetPropW(hWnd, L"EditHWND");
		wchar_t* path = (wchar_t*)GetPropW(hWnd, L"FilePath");

		if (id == ID_EDITOR_EDIT) {
			if (ev == EN_VSCROLL || ev == EN_UPDATE || ev == EN_CHANGE) {
				// DrawEditorLineNumbers(hWnd);
				InvalidateEditorLineNumbers(hWnd);
			}
			return 0;
		}

		if (id == ID_MENU_SAVE || id == ID_MENU_SAVE_AS) {

			wchar_t filename[MAX_PATH];
			wcscpy_s(filename, path);

			OPENFILENAMEW ofn = { sizeof(ofn) };
			ofn.lpstrFile = filename;
			ofn.nMaxFile = MAX_PATH;

			if (id == ID_MENU_SAVE_AS && !GetSaveFileNameW(&ofn)) return 0;

			int len = GetWindowTextLengthW(hEdit);
			std::wstring content(len, 0);
			GetWindowTextW(hEdit, content.data(), len + 1);

			std::ofstream(WStringToUtf8(filename), std::ios::binary) << WStringToUtf8(content);
		}

		if (id == ID_MENU_OPENEXT)
			ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);

		if (id == ID_MENU_SHOWFOLDER) {
			std::wstring cmd = L"/select,\"" + std::wstring(path) + L"\"";
			ShellExecuteW(NULL, L"open", L"explorer.exe", cmd.c_str(), NULL, SW_SHOWNORMAL);
		}

		return 0;
	}

	case WM_DESTROY: {
		HWND hEdit = (HWND)GetPropW(hWnd, L"EditHWND");
		if (hEdit)
			RemoveWindowSubclass(hEdit, EditorEditSubclassProc, 1);

		wchar_t* p = (wchar_t*)GetPropW(hWnd, L"FilePath");
		if (p) {
			free(p);
		}

		RemovePropW(hWnd, L"FilePath");
		RemovePropW(hWnd, L"EditHWND");
		RemovePropW(hWnd, L"LineNumsHWND");
		return 0;
	}

	case WM_CTLCOLORSTATIC: {
		if ((HWND)lParam == (HWND)GetPropW(hWnd, L"LineNumsHWND")) {
			static HBRUSH hLineBg = CreateSolidBrush(RGB(225, 240, 255));
			SetBkMode((HDC)wParam, OPAQUE);
			SetBkColor((HDC)wParam, RGB(225, 240, 255));
			SetTextColor((HDC)wParam, RGB(60, 90, 120));
			return (LRESULT)hLineBg;
		}
		return 0;
	}

	default:
		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}
}

