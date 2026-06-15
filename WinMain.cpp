
// TODO: Add file menu with about section as well as a section for configuration settings (ifdef USE_PCRE2, add attribution to about section)
// TODO: Add support for a drop down with last 5 regex/searches (if a last search is regex, display "reg" in dark text and selecting it should auto enable regex)
// TODO: Along with the history support, add a config screen where users can save custom regex code for the drop down suggestions

// TODO: Files over MAX_FILE_SIZE (20MB) are currently ignored in the file map. This should be a settings option and adjustable.

// Find URLs - Regex - https?:\/\/(www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*)


#include "Defines.h"
#include "Utils.h"
#include "Search.h"
#include "Editor.h"


#ifndef USE_PCRE2
    #if defined(_MSC_VER)
        #pragma message("Compiling WITHOUT PCRE2 support enabled, using std:regex")
    #elif defined(__GNUC__) || defined(__clang__)
        #warning "Compiling WITHOUT PCRE2 support enabled, using std:regex"
    #endif
#endif


// Config
const wchar_t* LINE_NUM_CLASS = L"FastSearchLineNumberClass";
const wchar_t* WINDOW_CLASS = L"FastSearchGuiClass";
const wchar_t* EDITOR_CLASS = L"FastSearchEditorClass";


// Messages
const UINT WM_SEARCH_BATCH     = WM_USER + 1;
const UINT WM_SEARCH_PROGRESS  = WM_USER + 2;
const UINT WM_SEARCH_DONE      = WM_USER + 3;
const UINT WM_SEARCH_READY     = WM_USER + 4;
const UINT WM_SEARCH_NO_FILES  = WM_USER + 5;

// Buttons/etc
const int ID_BTN_FOLDER = 1001;
const int ID_EDIT_QUERY = 1002;
const int ID_CHECK_REGEX = 1003;
const int ID_CHECK_CASE = 1004;
const int ID_BTN_START = 1005;
const int ID_BTN_STOP = 1006;
const int ID_BTN_EXPORT = 1007;
const int ID_PROGRESS = 1008;
const int ID_LISTVIEW = 1009;
const int ID_CHECK_FIND_ALL = 1010;
const int ID_BTN_OPEN = 1011;

const int ID_CHECK_TEXTFILES = 1020;
const int ID_CHECK_ALLFILES  = 1021;
const int ID_EDIT_FILETYPES  = 1022;




// TODO: Clean this up. These should be localized.
//		Status indicator should be updated with a utility function. etc

// ---------- Globals ----------
HWND g_hWnd = nullptr;
HWND g_btnFolder = nullptr;
HWND g_editQuery = nullptr;
HWND g_checkRegex = nullptr;
HWND g_checkCase = nullptr;
HWND g_btnStart = nullptr;
HWND g_btnStop = nullptr;
HWND g_btnExport = nullptr;
HWND g_progress = nullptr;
HWND g_listView = nullptr;
HWND g_status = nullptr;
HWND g_checkFindAll = nullptr;

HWND g_editFileTypes = nullptr;
HWND g_checkAllFiles = nullptr;

bool g_isLightMode = true;	// TODO: Light and dark mode need work
HBRUSH g_hbrWindow = NULL;
HBRUSH g_hbrEdit = NULL;
HBRUSH g_hbrStatic = NULL;


std::atomic<bool> g_stopFlag(false);
std::atomic<long> g_totalFiles(0);
std::atomic<long> g_scannedFiles(0);
std::atomic<long> g_matchCount(0);

std::mutex g_queueMutex;
std::queue<std::wstring> g_fileQueue;

std::mutex g_resultsMutex;
std::vector<std::tuple<std::wstring,int,std::wstring>> g_results;

std::atomic<int> g_workersAlive = 0;
std::vector<std::thread> g_workers;
std::wstring g_folderPath;

std::thread g_searchController;
std::atomic<bool> g_searchActive(false);







// ---------- Main Window UI ----------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE: {

		g_isLightMode = true;//IsLightMode();
		RecreateThemeBrushes();

		g_btnFolder = CreateWindowW(L"BUTTON", L"Select Folder", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 10, 10, 110, 26, hWnd, (HMENU)ID_BTN_FOLDER, NULL, NULL);
		g_editQuery = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_LEFT|ES_AUTOHSCROLL, 130, 10, 420, 26, hWnd, (HMENU)ID_EDIT_QUERY, NULL, NULL);
		g_checkRegex = CreateWindowW(L"BUTTON", L"Regex", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, 560, 10, 80, 26, hWnd, (HMENU)ID_CHECK_REGEX, NULL, NULL);
		g_checkCase = CreateWindowW(L"BUTTON", L"Case Sensitive", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, 650, 10, 130, 26, hWnd, (HMENU)ID_CHECK_CASE, NULL, NULL);
		g_btnStart = CreateWindowW(L"BUTTON", L"Start", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 790, 10, 80, 26, hWnd, (HMENU)ID_BTN_START, NULL, NULL);
		g_btnStop = CreateWindowW(L"BUTTON", L"Stop", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 880, 10, 80, 26, hWnd, (HMENU)ID_BTN_STOP, NULL, NULL);
		g_btnExport = CreateWindowW(L"BUTTON", L"Export CSV", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 970, 10, 120, 26, hWnd, (HMENU)ID_BTN_EXPORT, NULL, NULL);
		//g_checkFindAll = CreateWindowW(L"BUTTON", L"Find All Occurrences", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, 790, 40, 200, 20, hWnd, (HMENU)ID_CHECK_FIND_ALL, NULL, NULL);

		g_progress = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD|WS_VISIBLE, 10, 646, 1080, 18, hWnd, (HMENU)ID_PROGRESS, NULL, NULL);
		SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

		g_status = CreateWindowW(L"STATIC", L"Idle", WS_CHILD|WS_VISIBLE, 10, 670, 1080, 18, hWnd, NULL, NULL, NULL);
		//g_folderlabel = CreateWindowW(L"STATIC", L"Idle", WS_CHILD|WS_VISIBLE, 10, 694, 1080, 18, hWnd, NULL, NULL, NULL);

		CreateWindowW(L"STATIC", L"Extensions:", WS_CHILD|WS_VISIBLE,
			10, 54, 80, 20, hWnd, NULL, NULL, NULL);

		g_editFileTypes = CreateWindowW(L"EDIT", L"c,cpp,py,h,txt",
			WS_CHILD|WS_VISIBLE|WS_BORDER,
			90, 50, 200, 24, hWnd, (HMENU)ID_EDIT_FILETYPES, NULL, NULL);

		g_checkAllFiles = CreateWindowW(L"BUTTON", L"All Files",
			WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
			410, 50, 100, 24, hWnd, (HMENU)ID_CHECK_ALLFILES, NULL, NULL);

		INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
		InitCommonControlsEx(&icex);
		g_listView = CreateWindowW(WC_LISTVIEWW, NULL, WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL, 10, 76, 1080, 560, hWnd, (HMENU)ID_LISTVIEW, NULL, NULL);
		ListView_SetExtendedListViewStyle(g_listView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
		LVCOLUMNW col = {};
		col.mask = LVCF_TEXT | LVCF_WIDTH;
		col.cx = 400; col.pszText = (LPWSTR)L"File"; ListView_InsertColumn(g_listView, 0, &col);
		col.cx = 60; col.pszText = (LPWSTR)L"Line"; ListView_InsertColumn(g_listView, 1, &col);
		col.cx = 620; col.pszText = (LPWSTR)L"Match"; ListView_InsertColumn(g_listView, 2, &col);
		break;
	}


	case WM_GETMINMAXINFO: {
		MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
		mmi->ptMinTrackSize.x = 760;
		mmi->ptMinTrackSize.y = 320;
		return 0;
	}

	case WM_SIZE: {
		int width  = LOWORD(lParam);
		int height = HIWORD(lParam);

		const int margin = 10;
		const int gap = 8;

		// ---------- Top row ----------
		const int topY = 10;
		const int rowH = 26;

		const int folderW = 110;
		const int regexW = 80;
		const int caseW = 130;
		const int startW = 70;
		const int stopW = 70;
		const int exportW = 100;   // TODO: Get rid of this with a file/about menu

		// Place right-side controls from right to left
		int xRight = width - margin;

		xRight -= exportW;
		MoveWindow(g_btnExport, xRight, topY, exportW, rowH, TRUE);

		xRight -= gap + stopW;
		MoveWindow(g_btnStop, xRight, topY, stopW, rowH, TRUE);

		xRight -= gap + startW;
		MoveWindow(g_btnStart, xRight, topY, startW, rowH, TRUE);

		xRight -= gap + caseW;
		MoveWindow(g_checkCase, xRight, topY, caseW, rowH, TRUE);

		xRight -= gap + regexW;
		MoveWindow(g_checkRegex, xRight, topY, regexW, rowH, TRUE);

		// Left-side controls
		MoveWindow(g_btnFolder, margin, topY, folderW, rowH, TRUE);

		int queryX = margin + folderW + gap;
		int queryW = xRight - gap - queryX;

		// Prevent negative/too-small widths
		if (queryW < 120)
			queryW = 120;

		MoveWindow(g_editQuery, queryX, topY, queryW, rowH, TRUE);

		// ---------- Filter row ----------
		const int row2Y = 50;
		const int row2H = 24;

		MoveWindow(g_editFileTypes, 90, row2Y, 200, row2H, TRUE);
		MoveWindow(g_checkAllFiles, 310, row2Y, 100, row2H, TRUE);

		// ---------- Bottom UI ----------
		const int progressHeight = 18;
		const int statusHeight = 18;
		const int folderHeight = 18;   // if you added g_folderLabel
		const int bottomGap = 4;

		bool hasFolderLabel = 0;//(g_folderLabel != nullptr);

		int folderY = height - margin - folderHeight;
		int statusY = hasFolderLabel
			? folderY - bottomGap - statusHeight
			: height - margin - statusHeight;
		int progressY = statusY - bottomGap - progressHeight;

		MoveWindow(
			g_progress,
			margin,
			progressY,
			width - 2 * margin,
			progressHeight,
			TRUE
		);

		MoveWindow(
			g_status,
			margin,
			statusY,
			width - 2 * margin,
			statusHeight,
			TRUE
		);

		// if (hasFolderLabel) {
		// 	MoveWindow(
		// 		g_folderLabel,
		// 		margin,
		// 		folderY,
		// 		width - 2 * margin,
		// 		folderHeight,
		// 		TRUE
		// 	);
		// }

		// ---------- ListView ----------
		const int listTop = 76;
		int listBottom = progressY - bottomGap;
		int listHeight = listBottom - listTop;
		if (listHeight < 50) listHeight = 50;

		MoveWindow(
			g_listView,
			margin,
			listTop,
			width - 2 * margin,
			listHeight,
			TRUE
		);

		// Better column sizing
		int listWidth = width - 2 * margin;
		int lineColW = 70;
		int fileColW = std::max(220, listWidth / 3);
		int matchColW = std::max(200, listWidth - fileColW - lineColW - 8);

		ListView_SetColumnWidth(g_listView, 0, fileColW);
		ListView_SetColumnWidth(g_listView, 1, lineColW);
		ListView_SetColumnWidth(g_listView, 2, matchColW);

		InvalidateRect(hWnd, NULL, TRUE);
		return 0;
	}



	case WM_COMMAND: {
		int id = LOWORD(wParam);
		int ev  = HIWORD(wParam);
		HWND ctrl = (HWND)lParam; // may be NULL for menu/accelerator commands

		if (id == ID_BTN_FOLDER) {
			BROWSEINFOW bi = { 0 };
			bi.hwndOwner = hWnd;
			bi.lpszTitle = L"Select folder to search";
			LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
			if (pidl) {
				wchar_t buf[MAX_PATH];
				if (SHGetPathFromIDListW(pidl, buf)) {
					g_folderPath = buf;
					SetWindowTextW(g_status, buf);
				}
				CoTaskMemFree(pidl);
			}
		} else if (id == ID_BTN_START) {
			if (g_folderPath.empty()) { MessageBoxW(hWnd, L"Select a folder first.", L"Error", MB_ICONERROR); break; }
			std::wstring query = GetWindowTextWstr(g_editQuery);
			if (query.empty()) { MessageBoxW(hWnd, L"Enter a search query.", L"Error", MB_ICONERROR); break; }
			bool findAll = true;//(SendMessageW(g_checkFindAll, BM_GETCHECK, 0, 0) == BST_CHECKED);

			g_stopFlag = false;
			g_totalFiles = 0;
			g_scannedFiles = 0;
			g_matchCount = 0;
			{
				std::lock_guard<std::mutex> lk(g_queueMutex);
				while (!g_fileQueue.empty()) g_fileQueue.pop();
			}
			{
				std::lock_guard<std::mutex> lk(g_resultsMutex);
				g_results.clear();
			}
			ListView_DeleteAllItems(g_listView);

			if (g_searchActive) {
				MessageBoxW(hWnd, L"A search is already running.", L"Info", MB_ICONINFORMATION);
				break;
			}

			UpdateFilterState();

			SendMessageW(g_progress, PBM_SETRANGE32, 0, 100);
			SendMessageW(g_progress, PBM_SETPOS, 0, 0);
			SetWindowTextW(g_status, L"Preparing search...");

			bool useRegex = (SendMessageW(g_checkRegex, BM_GETCHECK, 0, 0) == BST_CHECKED);
			bool caseSensitive = (SendMessageW(g_checkCase, BM_GETCHECK, 0, 0) == BST_CHECKED);

			int needed = WideCharToMultiByte(CP_UTF8, 0, query.c_str(), -1, NULL, 0, NULL, NULL);
			std::string qUtf8;
			if (needed > 0) {
				qUtf8.resize(needed - 1);
				WideCharToMultiByte(CP_UTF8, 0, query.c_str(), -1, &qUtf8[0], needed, NULL, NULL);
			}

			if (g_searchController.joinable())
				g_searchController.join();

			g_searchController = std::thread(
				SearchControllerThreadFunc,
				g_folderPath,
				qUtf8,
				query,
				useRegex,
				caseSensitive,
				findAll
			);

		} else if (id == ID_BTN_STOP) {
			g_stopFlag = true;
			SetWindowTextW(g_status, L"Stopped.");
		} else if (id == ID_BTN_EXPORT) {
			wchar_t filename[MAX_PATH] = L"results.csv";
			OPENFILENAMEW ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFile = filename;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = L"CSV Files\0*.csv\0All Files\0*.*\0";
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
			if (GetSaveFileNameW(&ofn)) {
				// convert filename to UTF-8 and write CSV
				std::string utf8path = WStringToUtf8(filename);
				std::ofstream ofs(utf8path, std::ios::binary);
				if (ofs.good()) {
					// write UTF-8 BOM for Excel compatibility
					const unsigned char bom[] = {0xEF,0xBB,0xBF};
					ofs.write((const char*)bom, sizeof(bom));
					std::lock_guard<std::mutex> lk(g_resultsMutex);
					ofs << "File,Line,Match\n";
					for (auto &t : g_results) {
						std::wstring f = std::get<0>(t);
						int ln = std::get<1>(t);
						std::wstring m = std::get<2>(t);
						std::string fs = WStringToUtf8(f);
						std::string ms = WStringToUtf8(m);
						// escape quotes by doubling
						for (auto &c : ms) if (c == '"') c = '\'';
						ofs << "\"" << fs << "\"," << ln << ",\"" << ms << "\"\n";
					}
					ofs.close();
					MessageBoxW(hWnd, L"Export complete.", L"Export", MB_OK);
				} else {
					MessageBoxW(hWnd, L"Failed to open file for writing.", L"Error", MB_ICONERROR);
				}
			}
		}
		break;
	}

	case WM_NOTIFY: {
	LPNMHDR nm = (LPNMHDR)lParam;
	if (nm->idFrom == ID_LISTVIEW && nm->code == NM_DBLCLK) {
		int idx = ListView_GetNextItem(g_listView, -1, LVNI_SELECTED);
		if (idx >= 0) {
			wchar_t buf[MAX_PATH];
			ListView_GetItemText(g_listView, idx, 0, buf, _countof(buf));
			wchar_t linebuf[32]; ListView_GetItemText(g_listView, idx, 1, linebuf, _countof(linebuf));
			int line = _wtoi(linebuf);
			ShowFileEditor(std::wstring(buf), line);
		}
	}
	break;
	}


	case WM_SEARCH_BATCH: { 
		auto* batch = reinterpret_cast<std::vector<ResultRow*>*>(wParam);
		if (!batch) break;

		// Turn off redraw to avoid per-item repaints
		ListView_SetRedraw(g_listView, FALSE);

		int baseIndex = ListView_GetItemCount(g_listView);

		{
			std::lock_guard<std::mutex> lk(g_resultsMutex);

			for (size_t i = 0; i < batch->size(); ++i) {
				ResultRow* r = (*batch)[i];
				if (!r) continue;

				// Store for CSV export
				g_results.emplace_back(r->file, r->line, r->match);

				// Insert into ListView
				LVITEMW item = {};
				item.mask = LVIF_TEXT;
				item.iItem = baseIndex + (int)i;
				item.iSubItem = 0;
				item.pszText = const_cast<LPWSTR>(r->file.c_str());
				ListView_InsertItem(g_listView, &item);

				std::wstring lineStr = std::to_wstring(r->line);
				ListView_SetItemText(g_listView, item.iItem, 1, (LPWSTR)lineStr.c_str());
				ListView_SetItemText(g_listView, item.iItem, 2, (LPWSTR)r->match.c_str());

				delete r;
			}
		}

		delete batch;

		// Redraw
		ListView_SetRedraw(g_listView, TRUE);

		break;
	}


	case WM_SEARCH_PROGRESS: {		// Progress update
		long scanned = g_scannedFiles.load();
		SendMessageW(g_progress, PBM_SETPOS, scanned, 0);

		long total = g_totalFiles.load();
		wchar_t statusbuf[256];
		swprintf(statusbuf, _countof(statusbuf),
				L"Searching. Scanned: %ld / %ld    Matches: %ld",
				scanned, total, g_matchCount.load());
		SetWindowTextW(g_status, statusbuf);
		break;
	}

	case WM_SEARCH_DONE: {
		long scanned = g_scannedFiles.load();
		SendMessageW(g_progress, PBM_SETPOS, scanned, 0);

		long total = g_totalFiles.load();
		wchar_t statusbuf[256];

		if (g_stopFlag) {
			swprintf(statusbuf, _countof(statusbuf),
				L"Stopped. Scanned: %ld / %ld    Matches: %ld",
				scanned, total, g_matchCount.load());
		} else {
			swprintf(statusbuf, _countof(statusbuf),
				L"Completed. Searched: %ld / %ld    Matches: %ld",
				scanned, total, g_matchCount.load());
		}

		SetWindowTextW(g_status, statusbuf);
		break;
	}

	case WM_SEARCH_READY: {
		long total = (long)wParam;
		SendMessageW(g_progress, PBM_SETRANGE32, 0, total);
		SendMessageW(g_progress, PBM_SETPOS, 0, 0);
		SetWindowTextW(g_status, L"Searching...");
		break;
	}

	case WM_SEARCH_NO_FILES: {
		SendMessageW(g_progress, PBM_SETPOS, 0, 0);
		SetWindowTextW(g_status, L"No supported files found.");
		MessageBoxW(hWnd, L"No supported files found.", L"Info", MB_ICONINFORMATION);
		break;
	}

	// Theme handling ---------
	case WM_SETTINGCHANGE:
	case WM_THEMECHANGED:
		{
			bool newMode = IsLightMode();
			if (newMode != g_isLightMode) {
				g_isLightMode = newMode;
				RecreateThemeBrushes();
				// update controls that need explicit colors
				ListView_SetBkColor(g_listView, g_isLightMode ? GetSysColor(COLOR_WINDOW) : RGB(32,32,32));
				ListView_SetTextColor(g_listView, g_isLightMode ? GetSysColor(COLOR_WINDOWTEXT) : RGB(230,230,230));
				InvalidateRect(hWnd, NULL, TRUE);
			}
		}
		break;

	case WM_CTLCOLORSTATIC: {
		HDC hdc = (HDC)wParam;
		SetTextColor(hdc, g_isLightMode ? GetSysColor(COLOR_WINDOWTEXT) : RGB(230,230,230));
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_hbrStatic;
	}

	case WM_CTLCOLOREDIT: {
		HDC hdc = (HDC)wParam;
		SetTextColor(hdc, g_isLightMode ? GetSysColor(COLOR_WINDOWTEXT) : RGB(230,230,230));
		SetBkMode(hdc, OPAQUE);
		return (LRESULT)g_hbrEdit;
	}

	case WM_CTLCOLORBTN: {
		HDC hdc = (HDC)wParam;
		SetTextColor(hdc, g_isLightMode ? GetSysColor(COLOR_BTNTEXT) : RGB(230,230,230));
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_hbrWindow;
	}
	// ------------------


	case WM_DESTROY:
		g_stopFlag = true;

		if (g_searchController.joinable())
			g_searchController.join();

		PostQuitMessage(0);
		break;

	default:
		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}
	return 0;
}


// ---------- WinMain ----------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {

	INITCOMMONCONTROLSEX icc = { sizeof(icc) };
	icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icc);

	WNDCLASSW wc = {};

	HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
	wc.hIcon         = hIcon;

	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = WINDOW_CLASS;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	RegisterClassW(&wc);

	LoadLibraryW(L"Msftedit.dll");

	g_hWnd = CreateWindowExW(0, WINDOW_CLASS, L"Code and Text Search", WS_OVERLAPPEDWINDOW,// & ~WS_THICKFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT, 1120, 760, NULL, NULL, hInstance, NULL);
	if (!g_hWnd) return 0;

	WNDCLASSW wcEditor = {};
	wcEditor.lpfnWndProc = EditorWndProc;
	wcEditor.hInstance = hInstance;
	wcEditor.lpszClassName = EDITOR_CLASS;
	wcEditor.hCursor = LoadCursor(NULL, IDC_IBEAM);
	RegisterClassW(&wcEditor);

	WNDCLASSW wcLine = {};
	wcLine.lpfnWndProc = LineNumberWndProc;
	wcLine.hInstance = hInstance;
	wcLine.lpszClassName = LINE_NUM_CLASS;
	wcLine.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcLine.hbrBackground = NULL;
	RegisterClassW(&wcLine);

	DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
	DwmSetWindowAttribute(g_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return 0;
}
