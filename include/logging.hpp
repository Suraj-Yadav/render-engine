#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringStlView.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/MaterialData.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/std.h>
#include <spdlog/cfg/env.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <filesystem>
#include <iostream>

template <typename T, typename... AllowedTypes>
concept IsOneOf = (std::same_as<T, AllowedTypes> || ...);

template <typename T>
concept LoggingTypes = IsOneOf<
	T, Corrade::Containers::String, Corrade::Containers::StringView,
	Magnum::PixelFormat, Magnum::Trade::MaterialTypes,
	Magnum::Trade::MaterialAttributeType, Magnum::Trade::MaterialLayer,
	Magnum::Color3, Magnum::Trade::MaterialAttribute, Magnum::Matrix3,
	Magnum::Vector2i, Magnum::GL::Shader::Type>;

template <LoggingTypes T>
struct fmt::formatter<T> : formatter<std::string_view> {
	auto format(const T& c, format_context& ctx) const
		-> format_context::iterator {
		static std::ostringstream os;
		os.str("");
		os.clear();
		Corrade::Utility::Debug{&os} << c;

		auto sv = os.view();
		while (!sv.empty() && std::isspace(sv.front())) { sv.remove_prefix(1); }
		while (!sv.empty() && std::isspace(sv.back())) { sv.remove_suffix(1); }

		return formatter<std::string_view>::format(sv, ctx);
	}
};

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

#define PARENT std::filesystem::path(__FILE__).parent_path()
