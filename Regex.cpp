#include "Regex.h"

bool RegexMatcher::compile(const std::wstring& pattern, bool caseSensitive) {
	std::wregex::flag_type flags = std::regex_constants::ECMAScript;
	if (!caseSensitive) {
		flags |= std::regex_constants::icase;
	}

	try {
		regex_ = std::wregex(pattern, flags);
		return true;
	} catch (...) {
		return false;
	}
}

bool RegexMatcher::search(const std::wstring& text) const {
	try {
		return std::regex_search(text, regex_);
	} catch (...) {
		return false;
	}
}

#ifdef USE_PCRE2

Pcre2Regex::Pcre2Regex() = default;

Pcre2Regex::~Pcre2Regex() {
	if (code_) {
		pcre2_code_free(code_);
		code_ = nullptr;
	}
}

bool Pcre2Regex::compile(const std::string& patternUtf8, bool caseSensitive) {
	uint32_t options = 0;
	options |= PCRE2_UTF;
	options |= PCRE2_UCP;

	if (!caseSensitive) {
		options |= PCRE2_CASELESS;
	}

	int errorCode = 0;
	PCRE2_SIZE errorOffset = 0;

	code_ = pcre2_compile(
		reinterpret_cast<PCRE2_SPTR>(patternUtf8.data()),
		patternUtf8.size(),
		options,
		&errorCode,
		&errorOffset,
		nullptr
	);

	if (!code_) {
		return false;
	}

	int jitRc = pcre2_jit_compile(code_, PCRE2_JIT_COMPLETE);
	jitEnabled_ = (jitRc == 0);

	return true;
}

bool Pcre2Regex::valid() const {
	return code_ != nullptr;
}

bool Pcre2Regex::jitEnabled() const {
	return jitEnabled_;
}

pcre2_code* Pcre2Regex::code() const {
	return code_;
}

Pcre2ThreadContext::Pcre2ThreadContext(pcre2_code* code, bool useJit)
	: code_(code), useJit_(useJit)
{
	matchData_ = pcre2_match_data_create_from_pattern(code_, nullptr);
	matchContext_ = pcre2_match_context_create(nullptr);

	if (useJit_) {
		jitStack_ = pcre2_jit_stack_create(32 * 1024, 512 * 1024, nullptr);
		if (jitStack_ && matchContext_) {
			pcre2_jit_stack_assign(matchContext_, nullptr, jitStack_);
		}
	}
}

Pcre2ThreadContext::~Pcre2ThreadContext() {
	if (jitStack_)     pcre2_jit_stack_free(jitStack_);
	if (matchContext_) pcre2_match_context_free(matchContext_);
	if (matchData_)    pcre2_match_data_free(matchData_);
}

bool Pcre2ThreadContext::search(std::string_view text) const {
	if (!code_ || !matchData_) {
		return false;
	}

	int rc;
	if (useJit_) {
		rc = pcre2_jit_match(
			code_,
			reinterpret_cast<PCRE2_SPTR>(text.data()),
			text.size(),
			0,
			0,
			matchData_,
			matchContext_
		);
	} else {
		rc = pcre2_match(
			code_,
			reinterpret_cast<PCRE2_SPTR>(text.data()),
			text.size(),
			0,
			0,
			matchData_,
			matchContext_
		);
	}

	return rc >= 0;
}

#endif