#pragma once

#include "Defines.h"
#include <memory>
#include <string>

#ifdef USE_PCRE2
class Pcre2Regex;
#endif

void UpdateFilterState();
void EnqueueFilesRecursive(const std::wstring& dir);

void WorkerThreadFunc(
	const std::string& queryUtf8,
	const std::wstring& queryWide,
	bool useRegex,
	bool caseSensitive,
	bool findAll
#ifdef USE_PCRE2
	, std::shared_ptr<Pcre2Regex> sharedRegex
#endif
);

void SearchControllerThreadFunc(
	std::wstring folderPath,
	std::string queryUtf8,
	std::wstring queryWide,
	bool useRegex,
	bool caseSensitive,
	bool findAll
);


std::wstring Utf8ToWString(std::string_view s);
std::wstring Utf8ToWString(const std::string& s);

