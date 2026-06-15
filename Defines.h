#pragma once

#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <commdlg.h>
#include <richedit.h>
#include <dwmapi.h>

#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <regex>
#include <fstream>
#include <sstream>
#include <memory>
#include <locale>

#include <chrono>
#include <cwctype>

#include <algorithm>
#include <unordered_set>
#include <filesystem>



#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")



// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK EditorWndProc(HWND, UINT, WPARAM, LPARAM);




// ---------- Configuration ----------
extern const wchar_t* LINE_NUM_CLASS;
extern const wchar_t* WINDOW_CLASS;
extern const wchar_t* EDITOR_CLASS;




// Messages			TODO: These shouldn't be globally exposed.
extern const UINT WM_SEARCH_BATCH;
extern const UINT WM_SEARCH_PROGRESS;
extern const UINT WM_SEARCH_DONE;
extern const UINT WM_SEARCH_READY;
extern const UINT WM_SEARCH_NO_FILES;



// TODO: Clean this up. These should be localized.
//		Status indicator should be updated with a utility function.

// ---------- Globals ----------
extern HWND g_hWnd;
extern HWND g_btnFolder;
extern HWND g_editQuery;
extern HWND g_checkRegex;
extern HWND g_checkCase;
extern HWND g_btnStart;
extern HWND g_btnStop;
extern HWND g_btnExport;
extern HWND g_progress;
extern HWND g_listView;
extern HWND g_status;
extern HWND g_checkFindAll;

extern HWND g_editFileTypes;
extern HWND g_checkAllFiles;

extern bool g_isLightMode;
extern HBRUSH g_hbrWindow;
extern HBRUSH g_hbrEdit;
extern HBRUSH g_hbrStatic;


extern std::atomic<bool> g_stopFlag;
extern std::atomic<long> g_totalFiles;
extern std::atomic<long> g_scannedFiles;
extern std::atomic<long> g_matchCount;

extern std::mutex g_queueMutex;
extern std::queue<std::wstring> g_fileQueue;

extern std::mutex g_resultsMutex;
extern std::vector<std::tuple<std::wstring,int,std::wstring>> g_results;

extern std::atomic<int> g_workersAlive;
extern std::vector<std::thread> g_workers;
extern std::wstring g_folderPath;

extern std::thread g_searchController;
extern std::atomic<bool> g_searchActive;



struct ResultRow {
	std::wstring file;
	int line;
	std::wstring match;
};
