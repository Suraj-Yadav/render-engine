#pragma once

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringStlView.h>

#include <iostream>

inline std::ostream& operator<<(
	std::ostream& os, const Corrade::Containers::String& c) {
	return os << std::string_view(c);
}

#include <fmt/base.h>
#include <fmt/std.h>
#include <spdlog/cfg/env.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

// fmt v10 and above requires `fmt::formatter<T>` extends
// `fmt::ostream_formatter`. See: https://github.com/fmtlib/fmt/issues/3318
template <>
struct fmt::formatter<Corrade::Containers::String> : fmt::ostream_formatter {};

inline auto concat(const Corrade::Containers::StringIterable& f) {
	return Corrade::Containers::String("").join(f);
}

#define DBG(X) SPDLOG_DEBUG(#X " = {}", (X))

#define ASSERT(COND)                                 \
	if (!(COND)) {                                   \
		SPDLOG_CRITICAL("Assertion failed: " #COND); \
		cpptrace::generate_trace().print();          \
		std::exit(1);                                \
	}

#define ASSERT_MESG(COND, ...)                       \
	if (!(COND)) {                                   \
		SPDLOG_CRITICAL("Assertion failed: " #COND); \
		SPDLOG_CRITICAL(__VA_ARGS__);                \
		cpptrace::generate_trace().print();          \
		std::exit(1);                                \
	}
