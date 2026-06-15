// TODO: Some of this code should be refactored and made into utility functions.

#include "Search.h"
#include "Regex.h"

#include <immintrin.h> // AVX2 intrinsics (guarded use)
#include <cstring>
#include <cstdint>

#if defined(_MSC_VER)
  #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
  #include <cpuid.h>
#endif



// We'll just ignore files larger than this from the file map
const size_t MAX_FILE_SIZE = 20 * 1024 * 1024; // 20 MB

const int MAX_THREADS = std::max(4u, std::thread::hardware_concurrency());


#ifdef USE_PCRE2
static std::shared_ptr<Pcre2Regex> g_sharedRegex;
#endif


// Helper: runtime AVX2 detection (works on MSVC/GCC/Clang on x86/x64)
static bool cpu_has_avx2() {
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
	// MSVC path
	#if defined(_MSC_VER)
		int info[4] = {0};
		__cpuid(info, 0);
		int nIds = info[0];
		if (nIds >= 7) {
			__cpuidex(info, 7, 0);
			return (info[1] & (1 << 5)) != 0; // EBX bit 5 = AVX2
		}
		return false;
	// GCC/Clang path using <cpuid.h>
	#elif defined(__GNUC__) || defined(__clang__)
		unsigned int eax, ebx, ecx, edx;
		if (!__get_cpuid_max(0, nullptr)) return false;
		__cpuid_count(7, 0, eax, ebx, ecx, edx);
		return (ebx & (1 << 5)) != 0; // EBX bit 5 = AVX2
	#else
		return false;
	#endif
#else
	// Non-x86 targets (ARM, etc.) — detect via other means if needed
	return false;
#endif
}

// ---------- Fast Boyer-Moore-Horspool (byte) ----------
struct BMH {
	std::vector<int> skip;
	std::string pat;
	bool caseInsensitive;

	// Precomputed last-byte for SIMD probe (lowercased if caseInsensitive)
	unsigned char lastByteFolded = 0;
	bool avx2Available = false;

	BMH() : skip(256), caseInsensitive(false) {}

	void build(const std::string& p, bool ci) {
		pat = p;
		caseInsensitive = ci;
		int m = (int)pat.size();
		for (int i = 0; i < 256; ++i) skip[i] = m;
		for (int i = 0; i < m - 1; ++i) {
			unsigned char c = (unsigned char)pat[i];
			if (caseInsensitive && c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
			skip[c] = m - i - 1;
		}
		// last byte folded for probe
		if (m > 0) {
			unsigned char lb = (unsigned char)pat[m - 1];
			if (caseInsensitive && lb >= 'A' && lb <= 'Z') lb = lb - 'A' + 'a';
			lastByteFolded = lb;
		}
		avx2Available = cpu_has_avx2();
	}

private:
	// Scalar fallback memchr that respects case-insensitive ASCII folding
	static const char* scalar_memchr_fold(const char* data, size_t len, unsigned char target, bool fold) {
		for (size_t i = 0; i < len; ++i) {
			unsigned char c = (unsigned char)data[i];
			if (fold && c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
			if (c == target) return data + i;
		}
		return nullptr;
	}

	// AVX2 memchr-like probe: returns pointer to first occurrence of target in [data, data+len)
	// Only used for ASCII case-insensitive mode where we fold by mapping A-Z -> a-z via mask.
	static const char* avx2_memchr_fold(const char* data, size_t len, unsigned char target) {
#if defined(__AVX2__) || defined(_MSC_VER)
		const __m256i vtarget = _mm256_set1_epi8((char)target);
		size_t i = 0;
		// Align to 32 bytes for faster loads (we can process unaligned loads too)
		const size_t stride = 32;
		for (; i + stride <= len; i += stride) {
			__m256i chunk = _mm256_loadu_si256((const __m256i*)(data + i));
			// fold ASCII uppercase to lowercase: if 'A'..'Z' then set bit 0x20
			// Create mask of uppercase letters: (c >= 'A') & (c <= 'Z')
			__m256i A = _mm256_set1_epi8('A' - 1);
			__m256i Z = _mm256_set1_epi8('Z' + 1);
			__m256i gtA = _mm256_cmpgt_epi8(chunk, A); // c > 'A' - 1  => c >= 'A'
			__m256i ltZ = _mm256_cmpgt_epi8(Z, chunk); // 'Z' + 1 > c => c <= 'Z'
			__m256i isUpper = _mm256_and_si256(gtA, ltZ);
			// mask bit 0x20
			__m256i bit20 = _mm256_set1_epi8(0x20);
			__m256i folded = _mm256_or_si256(chunk, _mm256_and_si256(isUpper, bit20));
			__m256i cmp = _mm256_cmpeq_epi8(folded, vtarget);
			unsigned int mask = (unsigned int)_mm256_movemask_epi8(cmp);
			if (mask) {
				unsigned int offset = __builtin_ctz(mask);
				return data + i + offset;
			}
		}
		// tail
		size_t tail = len - i;
		if (tail) {
			const char* p = scalar_memchr_fold(data + i, tail, target, true);
			if (p) return p;
		}
		return nullptr;
#else
		// AVX2 not compiled in; fallback
		return scalar_memchr_fold(data, len, target, true);
#endif
	}

	// Generic probe that chooses AVX2 or scalar based on runtime availability and case folding
	const char* probe_last_byte(const char* data, size_t len) const {
		if (caseInsensitive && avx2Available) {
#if defined(__AVX2__) || defined(_MSC_VER)
			return avx2_memchr_fold(data, len, lastByteFolded);
#else
			return scalar_memchr_fold(data, len, lastByteFolded, true);
#endif
		} else {
			// no folding or no avx2: simple memchr
			const void* p = memchr(data, (int)lastByteFolded, len);
			return (const char*)p;
		}
	}

	// Verify full pattern at candidate position i (pattern length m)
	bool verify_at(const char* data, size_t len, size_t i, int m) const {
		// ensure we have enough bytes
		if (i + (size_t)m > len) return false;
		if (!caseInsensitive) {
			// fast memcmp
			return memcmp(data + i, pat.data(), m) == 0;
		} else {
			// ASCII-only case-insensitive verify
			for (int k = 0; k < m; ++k) {
				unsigned char td = (unsigned char)data[i + k];
				unsigned char pd = (unsigned char)pat[k];
				if (td >= 'A' && td <= 'Z') td = td - 'A' + 'a';
				if (pd >= 'A' && pd <= 'Z') pd = pd - 'A' + 'a';
				if (td != pd) return false;
			}
			return true;
		}
	}

public:
	bool search(const char* data, size_t len) const {
		int m = (int)pat.size();
		if (m == 0 || len < (size_t)m) return false;

		size_t i = 0;

		// Special-case pattern length 1: just probe whole buffer
		if (m == 1) {
			const char* p = probe_last_byte(data, len);
			return p != nullptr;
		}

		while (i <= len - m) {
			// IMPORTANT: probe for the last byte at position (i + m - 1) and beyond.
			// This ensures any candidate start = pos - (m - 1) is >= i, so we always make forward progress.
			const char* probeStart = data + i + (m - 1);
			size_t probeLen = len - (i + (m - 1));
			if (probeLen == 0) break;

			const char* found = probe_last_byte(probeStart, probeLen);
			if (!found) break;

			// 'found' points to the last byte of a candidate match.
			size_t pos = (size_t)(found - data); // absolute index of last byte
			size_t start = pos + 1 - m;          // candidate start index (guaranteed >= i)

			// Verify full pattern at 'start'
			if (verify_at(data, len, start, m)) return true;

			// Advance i using the skip table for the byte at start + m - 1 (the last byte we probed)
			unsigned char nextc = (unsigned char)data[start + m - 1];
			if (caseInsensitive && nextc >= 'A' && nextc <= 'Z') nextc = nextc - 'A' + 'a';
			size_t advance = (size_t)skip[nextc];
			// Ensure we always advance at least 1 to avoid infinite loop if skip[nextc] == 0
			if (advance == 0) advance = 1;
			i = start + advance;
		}

		return false;
	}
};


// Case-insensitive hash and equality for std::wstring_view
struct WStringHash {
	size_t operator()(std::wstring_view sv) const {
		size_t h = 2166136261u;
		for (wchar_t c : sv) {
			h ^= static_cast<size_t>(towlower(c));
			h *= 16777619u;
		}
		return h;
	}
};

struct WStringEqual {
	bool operator()(std::wstring_view s1, std::wstring_view s2) const {
		return s1.size() == s2.size() &&
			   _wcsnicmp(s1.data(), s2.data(), s1.size()) == 0;
	}
};

struct FilterState {
	bool checkAll = false;
	std::unordered_set<std::wstring, WStringHash, WStringEqual> filters;
};

static FilterState g_state;

static bool HasValidExtension(const std::wstring& name) {
	if (g_state.checkAll) return true;
	if (g_state.filters.empty()) return false;

	const wchar_t* extPtr = PathFindExtensionW(name.c_str());
	if (!extPtr || extPtr[0] == L'\0') return false;

	return g_state.filters.find(extPtr) != g_state.filters.end();
}

void UpdateFilterState() {
	g_state.checkAll = (SendMessageW(g_checkAllFiles, BM_GETCHECK, 0, 0) == BST_CHECKED);
	g_state.filters.clear();

	int len = GetWindowTextLengthW(g_editFileTypes);
	if (len <= 0) return;

	std::wstring raw(len, L'\0');
	GetWindowTextW(g_editFileTypes, raw.data(), len + 1);

	size_t start = 0;
	while (start < raw.size()) {
		size_t end = raw.find_first_of(L", ", start);

		if (end == std::wstring::npos) {
			end = raw.size();
		}

		if (end > start) {
			std::wstring token = raw.substr(start, end - start);
			if (!token.empty()) {
				if (token[0] != L'.') token = L"." + token;
				g_state.filters.insert(token);
			}
		}

		start = end + 1;
	}
}

void EnqueueFilesRecursive(const std::wstring& dir) {
	WIN32_FIND_DATAW fd{};
	std::wstring pattern = dir;
	if (pattern.back() != L'\\' && pattern.back() != L'/') pattern += L"\\*";
	else pattern += L"*";

	HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;

	do {
		if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
			continue;

		std::wstring full = dir;
		if (full.back() != L'\\' && full.back() != L'/') full += L"\\";
		full += fd.cFileName;

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			EnqueueFilesRecursive(full);
		} else {
			LARGE_INTEGER sz{};
			sz.LowPart = fd.nFileSizeLow;
			sz.HighPart = fd.nFileSizeHigh;

			if (sz.QuadPart > 0 && sz.QuadPart <= (LONGLONG)MAX_FILE_SIZE) {
				if (HasValidExtension(full)) {
					std::lock_guard<std::mutex> lk(g_queueMutex);
					g_fileQueue.push(full);
					g_totalFiles++;
				}
			}
		}
	} while (FindNextFileW(h, &fd));

	FindClose(h);
}



void WorkerThreadFunc(
	const std::string& queryUtf8,
	const std::wstring& queryWide,
	bool useRegex,
	bool caseSensitive,
	bool findAll
#ifdef USE_PCRE2
	, std::shared_ptr<Pcre2Regex> sharedRegex
#endif
) {
	constexpr size_t BATCH_SIZE = 1024; // larger batch for fewer UI messages
	std::vector<ResultRow*> batch;
	batch.reserve(BATCH_SIZE);
	g_workersAlive.fetch_add(1, std::memory_order_relaxed);

	bool queryAscii = std::all_of(queryUtf8.begin(), queryUtf8.end(),
		[](unsigned char c) { return c < 128; });


	#ifdef USE_PCRE2
	std::unique_ptr<Pcre2ThreadContext> regexCtx;
	if (useRegex && sharedRegex && sharedRegex->valid()) {
		regexCtx = std::make_unique<Pcre2ThreadContext>(
			sharedRegex->code(),
			sharedRegex->jitEnabled()
		);
	}
	#else
	RegexMatcher regexMatcher;
	bool regexReady = false;
	if (useRegex) {
		regexReady = regexMatcher.compile(queryWide, caseSensitive);
	}
	#endif

	// Throttle progress updates to UI (milliseconds)
	const std::chrono::milliseconds progressThrottle(300);
	auto lastProgressPost = std::chrono::steady_clock::now() - progressThrottle;

	auto postBatchToUI = [&](std::vector<ResultRow*>& localBatch) {
		if (localBatch.empty()) return;
		auto* heapBatch = new std::vector<ResultRow*>();
		heapBatch->reserve(localBatch.size());
		for (auto* r : localBatch) heapBatch->push_back(r);
		localBatch.clear();
		PostMessageW(g_hWnd, WM_SEARCH_BATCH, (WPARAM)heapBatch, 0);
		// Post a progress update (throttled) so UI can refresh progress bar
		auto now = std::chrono::steady_clock::now();
		if (now - lastProgressPost >= progressThrottle) {
			PostMessageW(g_hWnd, WM_SEARCH_PROGRESS, 0, 0);
			lastProgressPost = now;
		}
	};

	BMH bmh;
	if (!useRegex && queryAscii) {
		if (caseSensitive)
			bmh.build(queryUtf8, false);
		else {
			std::string lower = queryUtf8;
			for (auto& c : lower)
				if ((unsigned)(c - 'A') <= 25)
					c |= 0x20;//c = (wchar_t)towlower(c);
			bmh.build(lower, true);
		}
	}

	while (!g_stopFlag) {
		std::wstring file;
		{
			std::lock_guard<std::mutex> lk(g_queueMutex);
			if (g_fileQueue.empty())
				break;
			file = std::move(g_fileQueue.front());
			g_fileQueue.pop();
		}

		HANDLE hFile = CreateFileW(file.c_str(), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

		if (hFile == INVALID_HANDLE_VALUE) {
			g_scannedFiles++;
			// occasionally post progress if queue is large
			auto now = std::chrono::steady_clock::now();
			if (now - lastProgressPost >= progressThrottle) {
				PostMessageW(g_hWnd, WM_SEARCH_PROGRESS, 0, 0);
				lastProgressPost = now;
			}
			continue;
		}

		LARGE_INTEGER sz;
		if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (LONGLONG)MAX_FILE_SIZE) {
			CloseHandle(hFile);
			g_scannedFiles++;
			continue;
		}

		HANDLE hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
		if (!hMap) {
			CloseHandle(hFile);
			g_scannedFiles++;
			continue;
		}

		const char* data = (const char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
		if (!data) {
			CloseHandle(hMap);
			CloseHandle(hFile);
			g_scannedFiles++;
			continue;
		}

		const char* p = data;
		const char* end = data + (size_t)sz.QuadPart;
		int lineNum = 0;

		std::wstring queryWideFolded;
		if (!caseSensitive) {
			queryWideFolded = queryWide;
			for (auto& c : queryWideFolded)
				c = (wchar_t)towlower(c);
		}

		while (p < end && !g_stopFlag) {
			const char* lineEnd = (const char*)memchr(p, '\n', (size_t)(end - p));
			if (!lineEnd) lineEnd = end;

			size_t len = (size_t)(lineEnd - p);
			if (len && p[len - 1] == '\r') len--;

			std::string_view line(p, len);
			++lineNum;

			bool matched = false;

			std::wstring wline;
			bool wlineReady = false;

			auto ensureWLine = [&]() -> const std::wstring& {
				if (!wlineReady) {
					wline = Utf8ToWString(line);
					wlineReady = true;
				}
				return wline;
			};

			if (!useRegex && queryAscii) {
				matched = bmh.search(line.data(), line.size());
			}
			else if (useRegex) {
				#ifdef USE_PCRE2
						matched = (regexCtx && regexCtx->search(line));
				#else
						if (regexReady) matched = regexMatcher.search(ensureWLine());
				#endif
			}
			else {
				const std::wstring& w = ensureWLine();

				if (caseSensitive) {
					matched = (w.find(queryWide) != std::wstring::npos);
				}
				else {
					std::wstring folded = w;
					for (auto& c : folded)
						c = (wchar_t)towlower(c);

					matched = (folded.find(queryWideFolded) != std::wstring::npos);
				}
			}

			if (matched) {
				auto* r = new ResultRow{ file, lineNum, ensureWLine() };
				batch.push_back(r);
				g_matchCount++;

				if (!findAll) {
					postBatchToUI(batch);
					break;
				}

				if (batch.size() >= BATCH_SIZE) {
					postBatchToUI(batch);
				}
			}

			p = (lineEnd < end) ? lineEnd + 1 : end;
		}

		if (data) {
			UnmapViewOfFile(data);
		}

		CloseHandle(hMap);
		CloseHandle(hFile);

		g_scannedFiles++;

		// Periodically flush small batches even if not full to keep UI responsive
		auto now = std::chrono::steady_clock::now();
		if (!batch.empty() && (now - lastProgressPost >= progressThrottle)) {
			postBatchToUI(batch);
		}
	}

	// final flush
	if (!batch.empty()) postBatchToUI(batch);

	// if (g_workersAlive.fetch_sub(1) == 1) {
	// 	PostMessageW(g_hWnd, WM_SEARCH_DONE, 0, 0);	// Completion message
	// }
	g_workersAlive.fetch_sub(1, std::memory_order_relaxed);

}


void SearchControllerThreadFunc(
	std::wstring folderPath,
	std::string queryUtf8,
	std::wstring queryWide,
	bool useRegex,
	bool caseSensitive,
	bool findAll)
{
	g_searchActive = true;

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

	{
		std::lock_guard<std::mutex> lk(g_queueMutex);
		while (!g_fileQueue.empty())
			g_fileQueue.pop();
	}

	g_totalFiles = 0;
	g_scannedFiles = 0;
	g_matchCount = 0;

	// Blocking recursive enumeration now happens away from UI.
	EnqueueFilesRecursive(folderPath);

	if (g_stopFlag) {
		PostMessageW(g_hWnd, WM_SEARCH_DONE, 0, 0);
		g_searchActive = false;
		return;
	}

	long total = g_totalFiles.load();

	if (total == 0) {
		PostMessageW(g_hWnd, WM_SEARCH_NO_FILES, 0, 0);
		g_searchActive = false;
		return;
	}

	PostMessageW(g_hWnd, WM_SEARCH_READY, (WPARAM)total, 0);

	#ifdef USE_PCRE2
	g_sharedRegex.reset();

	if (useRegex) {
		auto re = std::make_shared<Pcre2Regex>();
		if (!re->compile(queryUtf8, caseSensitive)) {
			PostMessageW(g_hWnd, WM_SEARCH_DONE, 0, 0);
			g_searchActive = false;
			return;
		}
		g_sharedRegex = re;
	}
	#endif

	g_workers.clear();

	// Messy, but create worker threads (potentially passing pre-compiled regex)
	for (int i = 0; i < MAX_THREADS; ++i) {
		g_workers.emplace_back([
			queryUtf8, queryWide, useRegex, caseSensitive, findAll
			#ifdef USE_PCRE2
				, sharedRegex = g_sharedRegex
			#endif
		]() {
			WorkerThreadFunc(
				queryUtf8,
				queryWide,
				useRegex,
				caseSensitive,
				findAll
				#ifdef USE_PCRE2
					, sharedRegex
				#endif
			);
		});
	}

	for (auto& t : g_workers) {
		if (t.joinable())
			t.join();
	}

	g_workers.clear();

	PostMessageW(g_hWnd, WM_SEARCH_DONE, 0, 0);
	g_searchActive = false;
}

