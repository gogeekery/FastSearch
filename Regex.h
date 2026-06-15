#pragma once

#include "Defines.h"

#include <string>
#include <string_view>
#include <memory>
#include <regex>

#ifdef USE_PCRE2
#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 8
#endif
#include "PCRE2/pcre2.h"
#endif

class RegexMatcher {
public:
	bool compile(const std::wstring& pattern, bool caseSensitive);
	bool search(const std::wstring& text) const;

private:
	std::wregex regex_;
};

#ifdef USE_PCRE2
class Pcre2Regex {
public:
	Pcre2Regex();
	~Pcre2Regex();

	bool compile(const std::string& patternUtf8, bool caseSensitive);
	bool valid() const;
	bool jitEnabled() const;
	pcre2_code* code() const;

	Pcre2Regex(const Pcre2Regex&) = delete;
	Pcre2Regex& operator=(const Pcre2Regex&) = delete;

private:
	pcre2_code* code_ = nullptr;
	bool jitEnabled_ = false;
};

class Pcre2ThreadContext {
public:
	Pcre2ThreadContext(pcre2_code* code, bool useJit);
	~Pcre2ThreadContext();

	bool search(std::string_view text) const;

private:
	pcre2_code* code_ = nullptr;
	bool useJit_ = false;
	pcre2_match_data* matchData_ = nullptr;
	pcre2_match_context* matchContext_ = nullptr;
	pcre2_jit_stack* jitStack_ = nullptr;
};
#endif