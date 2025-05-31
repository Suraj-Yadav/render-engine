#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL

#include "shader.hpp"

#include <Corrade/Containers/ArrayViewStlSpan.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Shaders/GenericGL.h>
#include <Magnum/Types.h>
#include <stb_include.h>

#include <filesystem>

#include "logging.hpp"

struct ShaderCode {
	std::string content;

	ShaderCode& reset() {
		content.clear();
		return *this;
	}
	ShaderCode& addDef(std::string_view name) {
		content += "#define ";
		content += name;
		content += "\n";
		return *this;
	}

	ShaderCode& addDef(std::string_view name, const auto& value) {
		content += "#define ";
		content += name;
		content += " ";
		content += std::to_string(value);
		content += "\n";
		return *this;
	}

	ShaderCode& addFile(const std::filesystem::path& path) {
		content += "// ";
		content += path.string();
		content += "\n";
		ASSERT_MESG(std::filesystem::is_regular_file(path), "path = {}", path);
		auto pth = path.string();
		auto dir = path.parent_path().string();

		constexpr auto LEN = 256;
		std::string errMsg(LEN, '\0');

		auto* data =
			stb_include_file(pth.data(), " ", dir.data(), errMsg.data());
		ASSERT_MESG(data != nullptr, "Unable to load shader: {}", errMsg);
		content += data;
		free(data);
		return *this;
	}

	[[nodiscard]] const auto& code() const { return content; }
};

CustomShader::CustomShader(
	const std::filesystem::path& vertPath,
	const std::filesystem::path& fragPath) {
	Magnum::GL::Shader vert(
		Magnum::GL::Version::GL430, Magnum::GL::Shader::Type::Vertex);
	Magnum::GL::Shader frag(
		Magnum::GL::Version::GL430, Magnum::GL::Shader::Type::Fragment);

	ShaderCode code;

	code.addDef(
			"POSITION_ATTRIBUTE_LOCATION",
			Magnum::Shaders::GenericGL3D::Position::Location)
		.addFile(vertPath);
	vert.addSource(code.code());
	ASSERT_MESG(vert.compile(), "{}", concat(vert.sources()));

	code.reset().addFile(fragPath);
	frag.addSource(code.code());
	ASSERT_MESG(frag.compile(), "{}", concat(frag.sources()));

	attachShaders({vert, frag});

	ASSERT_MESG(
		link(), "vert:\n{}\nfrag\n:{}", concat(vert.sources()),
		concat(frag.sources()));
}

void PbrMetallicRoughness::compile() {
	using Corrade::Utility::format;
	Magnum::GL::Shader vert(
		Magnum::GL::Version::GL430, Magnum::GL::Shader::Type::Vertex);
	Magnum::GL::Shader frag(
		Magnum::GL::Version::GL430, Magnum::GL::Shader::Type::Fragment);

	ShaderCode code;

	code.addDef(
			"POSITION_ATTRIBUTE_LOCATION",
			Magnum::Shaders::GenericGL3D::Position::Location)
		.addDef(
			"TEXTURECOORDINATES_ATTRIBUTE_LOCATION",
			Magnum::Shaders::GenericGL3D::TextureCoordinates::Location)
		.addDef(
			"NORMAL_ATTRIBUTE_LOCATION",
			Magnum::Shaders::GenericGL3D::Normal::Location)
		.addFile("./shaders/pbr.vert");

	vert.addSource(code.code());
	ASSERT_MESG(vert.compile(), "{}", concat(vert.sources()));

	code.reset().addDef("LIGHT_COUNT", _conf.lightCount());

#define USE_FLAGS(F, S) \
	if (_conf.flags() >= Flag::F) { code.addDef(#S); }

	USE_FLAGS(BaseColorTexture, HAS_BASECOLOR_TEXTURE);
	USE_FLAGS(MetalnessTexture, HAS_METALNESS_TEXTURE);
	USE_FLAGS(RoughnessTexture, HAS_ROUGHNESS_TEXTURE);
	USE_FLAGS(NormalTexture, HAS_NORMAL_TEXTURE);
	USE_FLAGS(AoTexture, HAS_AO_TEXTURE);
	USE_FLAGS(EmissiveTexture, HAS_EMISSIVE_TEXTURE);
	USE_FLAGS(IBL, HAS_IBL);

#undef USE_FLAGS

	code.addFile("./shaders/pbr.frag");

	frag.addSource(code.code());
	ASSERT_MESG(frag.compile(), "{}", concat(frag.sources()));

	attachShaders({vert, frag});

	ASSERT_MESG(
		link(), "vert:\n{}\nfrag\n:{}", concat(vert.sources()),
		concat(frag.sources()));
}

PbrMetallicRoughness& PbrMetallicRoughness::setLightPositions(
	const std::initializer_list<Magnum::Vector3>& lightPositions) {
	ASSERT(lightPositions.size() == _conf.lightCount());

	setUniformT("lightPositions", std::span(lightPositions));
	return *this;
}

PbrMetallicRoughness& PbrMetallicRoughness::setLightColors(
	const std::initializer_list<Magnum::Vector3>& lightColors) {
	ASSERT(lightColors.size() == _conf.lightCount());

	setUniformT("lightColors", std::span(lightColors));
	return *this;
}
