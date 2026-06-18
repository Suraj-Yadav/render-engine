#include "shader.hpp"

#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_NONE

#include <Corrade/Containers/ArrayViewStlSpan.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Containers/StructuredBindings.h>
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
#include <ranges>
#include <regex>

#include "logging.hpp"

void resolveInclude(
	std::ostringstream& out, const std::filesystem::path& path,
	const std::filesystem::path& dir, std::map<int, std::string>& mapping) {
	ASSERT_MESG(std::filesystem::is_regular_file(path), "path = {}", path);
	int lineNum = 1, fileNum = mapping.size();
	mapping[fileNum] = path.string();
	out << "#line " << lineNum << " " << fileNum << "\n";

	static const std::regex include(R"(#include\s*(<|")([^>"]+)(>|"))");

	std::ifstream file(path);
	std::string line;
	while (std::getline(file, line)) {
		if (std::smatch match; std::regex_search(line, match, include)) {
			resolveInclude(out, dir / match[2].str(), dir, mapping);
			out << "#line " << lineNum + 1 << " " << fileNum << "\n";
		} else {
			out << line << "\n";
		}
		lineNum++;
	}
}

std::string resolveIncludes(
	const std::filesystem::path& path, std::map<int, std::string>& mapping) {
	auto dir = path.parent_path();
	std::ostringstream out;
	resolveInclude(out, path, dir, mapping);
	return out.str();
}

struct ShaderCode {
	std::string content;
	std::map<int, std::string> mapping;

	ShaderCode& addDef(std::string_view name) {
		content += fmt::format("#define {} 1\n", name);
		return *this;
	}

	ShaderCode& addDef(std::string_view name, const auto& value) {
		content += fmt::format("#define {} {}\n", name, value);
		return *this;
	}

	ShaderCode& addFile(const std::filesystem::path& path) {
		content += resolveIncludes(std::filesystem::canonical(path), mapping);
		return *this;
	}
	void compile(
		Magnum::GL::Shader& shader, const std::string_view type,
		std::source_location location = std::source_location::current()) {
		shader.addSource(content);

		if (!shader.compile()) {
			const auto path =
				std::filesystem::temp_directory_path() / "temp.glsl";
			{
				std::ofstream out(path);
				out << content;
			}

			(spdlog ::default_logger_raw())
				->log(
					spdlog::source_loc{
						location.file_name(), static_cast<int>(location.line()),
						location.function_name()},
					spdlog::level::critical,
					"Failed to compile shader: ", type);
			for (auto& [k, v] : mapping) { SPDLOG_CRITICAL("\t{}: {}", k, v); }
			SPDLOG_CRITICAL("Look at the complete file content at {}", path);
			ASSERT(false);
		}
		content.clear();
	}
};

auto concat(const Corrade::Containers::StringIterable& f) {
	return Corrade::Containers::String("").join(f);
}

constexpr auto Locations = std::to_array<std::pair<int, std::string_view>>({
	{Magnum::Shaders::GenericGL3D::Position::Location,
	 "POSITION_ATTRIBUTE_LOCATION"},
	{Magnum::Shaders::GenericGL3D::Normal::Location,
	 "NORMAL_ATTRIBUTE_LOCATION"},
	{Magnum::Shaders::GenericGL3D::TextureCoordinates::Location,
	 "TEXTURECOORDINATES_ATTRIBUTE_LOCATION"},
	{Magnum::Shaders::GenericGL3D::Color3::Location,
	 "COLOR_ATTRIBUTE_LOCATION"},
	{Magnum::Shaders::GenericGL3D::TransformationMatrix::Location,
	 "TRANSFORMATION_MATRIX_ATTRIBUTE_LOCATION"},
});

CustomShader::CustomShader(
	const std::filesystem::path& vertPath,
	const std::filesystem::path& fragPath) {
	using Magnum::Shaders::GenericGL3D;

	Magnum::GL::Shader vert(
		Magnum::GL::Version::GL430, Magnum::GL::Shader::Type::Vertex);
	Magnum::GL::Shader frag(
		Magnum::GL::Version::GL430, Magnum::GL::Shader::Type::Fragment);

	ShaderCode code;

	for (const auto& [loc, name] : Locations) { code.addDef(name, loc); }

	code.addFile(vertPath).compile(vert, "VERT");

	code.addDef("TONEMAP_KHR_PBR_NEUTRAL").addFile(fragPath);
	code.compile(frag, "FRAG");

	attachShaders({vert, frag});

	ASSERT_MESG(
		link(), "vert:\n{}\nfrag\n:{}", concat(vert.sources()),
		concat(frag.sources()));

	auto [ok, msg] = validate();
	if (!ok) { SPDLOG_WARN("shader validation: {}", msg); }
}

constexpr auto FragDefines = std::to_array<
	std::pair<PbrGL::Flag, std::string_view>>({
	{PbrGL::Flag::MaterialClearCoat, "MATERIAL_CLEARCOAT"},
	{PbrGL::Flag::MaterialSheen, "MATERIAL_SHEEN"},
	{PbrGL::Flag::MaterialTransmission, "MATERIAL_TRANSMISSION"},
	{PbrGL::Flag::MaterialDiffuseTransmission, "MATERIAL_DIFFUSE_TRANSMISSION"},
	{PbrGL::Flag::MaterialSpecular, "MATERIAL_SPECULAR"},
	{PbrGL::Flag::MaterialVolume, "MATERIAL_VOLUME"},
	{PbrGL::Flag::MaterialIridescence, "MATERIAL_IRIDESCENCE"},
	{PbrGL::Flag::MaterialAnisotropy, "MATERIAL_ANISOTROPY"},
	{PbrGL::Flag::MaterialEmissiveStrength, "MATERIAL_EMISSIVE_STRENGTH"},
	{PbrGL::Flag::MaterialDispersion, "MATERIAL_DISPERSION"},
	{PbrGL::Flag::MaterialIor, "MATERIAL_IOR"},
	{PbrGL::Flag::NormalTexture, "HAS_NORMAL_MAP"},
	{PbrGL::Flag::NormalUVTransform, "HAS_NORMAL_UV_TRANSFORM"},
	{PbrGL::Flag::OcclusionTexture, "HAS_OCCLUSION_MAP"},
	{PbrGL::Flag::OcclusionUVTransform, "HAS_OCCLUSION_UV_TRANSFORM"},
	{PbrGL::Flag::EmissiveTexture, "HAS_EMISSIVE_MAP"},
	{PbrGL::Flag::EmissiveUVTransform, "HAS_EMISSIVE_UV_TRANSFORM"},
	{PbrGL::Flag::BaseColorTexture, "HAS_BASE_COLOR_MAP"},
	{PbrGL::Flag::BaseColorUVTransform, "HAS_BASECOLOR_UV_TRANSFORM"},
	{PbrGL::Flag::MetallicRoughnessTexture, "HAS_METALLIC_ROUGHNESS_MAP"},
	{PbrGL::Flag::MetallicRoughnessUVTransform,
	 "HAS_METALLICROUGHNESS_UV_TRANSFORM"},
	{PbrGL::Flag::ClearcoatTexture, "HAS_CLEARCOAT_MAP"},
	{PbrGL::Flag::ClearcoatUVTransform, "HAS_CLEARCOAT_UV_TRANSFORM"},
	{PbrGL::Flag::ClearcoatRoughnessTexture, "HAS_CLEARCOAT_ROUGHNESS_MAP"},
	{PbrGL::Flag::ClearcoatRoughnessUVTransform,
	 "HAS_CLEARCOATROUGHNESS_UV_TRANSFORM"},
	{PbrGL::Flag::ClearcoatNormalTexture, "HAS_CLEARCOAT_NORMAL_MAP"},
	{PbrGL::Flag::ClearcoatNormalUVTransform,
	 "HAS_CLEARCOATNORMAL_UV_TRANSFORM"},
	{PbrGL::Flag::SheenColorTexture, "HAS_SHEEN_COLOR_MAP"},
	{PbrGL::Flag::SheenColorUVTransform, "HAS_SHEENCOLOR_UV_TRANSFORM"},
	{PbrGL::Flag::SheenRoughnessTexture, "HAS_SHEEN_ROUGHNESS_MAP"},
	{PbrGL::Flag::SheenRoughnessUVTransform, "HAS_SHEENROUGHNESS_UV_TRANSFORM"},
	{PbrGL::Flag::TransmissionTexture, "HAS_TRANSMISSION_MAP"},
	{PbrGL::Flag::TransmissionUVTransform, "HAS_TRANSMISSION_UV_TRANSFORM"},
	{PbrGL::Flag::DiffuseTransmissionTexture, "HAS_DIFFUSE_TRANSMISSION_MAP"},
	{PbrGL::Flag::DiffuseTransmissionUVTransform,
	 "HAS_DIFFUSETRANSMISSION_UV_TRANSFORM"},
	{PbrGL::Flag::DiffuseTransmissionColorTexture,
	 "HAS_DIFFUSE_TRANSMISSION_COLOR_MAP"},
	{PbrGL::Flag::DiffuseTransmissionColorUVTransform,
	 "HAS_DIFFUSETRANSMISSIONCOLOR_UV_TRANSFORM"},
	{PbrGL::Flag::SpecularTexture, "HAS_SPECULAR_MAP"},
	{PbrGL::Flag::SpecularUVTransform, "HAS_SPECULAR_UV_TRANSFORM"},
	{PbrGL::Flag::SpecularColorTexture, "HAS_SPECULAR_COLOR_MAP"},
	{PbrGL::Flag::SpecularColorUVTransform, "HAS_SPECULARCOLOR_UV_TRANSFORM"},
	{PbrGL::Flag::ThicknessTexture, "HAS_THICKNESS_MAP"},
	{PbrGL::Flag::ThicknessUVTransform, "HAS_THICKNESS_UV_TRANSFORM"},
	{PbrGL::Flag::IridescenceTexture, "HAS_IRIDESCENCE_MAP"},
	{PbrGL::Flag::IridescenceUVTransform, "HAS_IRIDESCENCE_UV_TRANSFORM"},
	{PbrGL::Flag::IridescenceThicknessTexture, "HAS_IRIDESCENCE_THICKNESS_MAP"},
	{PbrGL::Flag::IridescenceThicknessUVTransform,
	 "HAS_IRIDESCENCETHICKNESS_UV_TRANSFORM"},
	{PbrGL::Flag::AnisotropyTexture, "HAS_ANISOTROPY_MAP"},
	{PbrGL::Flag::AnisotropyUVTransform, "HAS_ANISOTROPY_UV_TRANSFORM"},
	{PbrGL::Flag::ImageBasedLighting, "USE_IBL"},
	{PbrGL::Flag::PunctualLights, "USE_PUNCTUAL"},
	{PbrGL::Flag::VertexColor3, "HAS_COLOR_0_VEC3"},
	{PbrGL::Flag::VertexColor4, "HAS_COLOR_0_VEC4"},
});
constexpr auto VertDefines =
	std::to_array<std::pair<PbrGL::Flag, std::string_view>>({
		{PbrGL::Flag::Instancing, "USE_INSTANCING"},
		{PbrGL::Flag::VertexColor3, "HAS_COLOR_0_VEC3"},
		{PbrGL::Flag::VertexColor4, "HAS_COLOR_0_VEC4"},
	});

void PbrGL::compile() {
	using Magnum::Shaders::GenericGL3D;

	Magnum::GL::Shader vert(
		Magnum::GL::Version::GL430, Magnum::GL::Shader::Type::Vertex);
	Magnum::GL::Shader frag(
		Magnum::GL::Version::GL430, Magnum::GL::Shader::Type::Fragment);

	ShaderCode code;

	code.addDef("HAS_NORMAL_VEC3").addDef("HAS_TEXCOORD_0_VEC2");
	for (const auto& [loc, name] : Locations) { code.addDef(name, loc); }

	for (const auto& [flag, define] : VertDefines) {
		if (_conf.flags() >= flag) { code.addDef(define); }
	}

	code.addFile(PARENT / "../shaders/pbr.vert").compile(vert, "VERT");

	code.addDef("LIGHT_COUNT", _conf.lightCount())
		.addDef("ALPHAMODE_OPAQUE", 0)
		.addDef("ALPHAMODE_MASK", 1)
		.addDef("ALPHAMODE_BLEND", 2)
		.addDef("ALPHAMODE", "ALPHAMODE_OPAQUE")
		.addDef("TONEMAP_KHR_PBR_NEUTRAL", 1)
		.addDef("HAS_NORMAL_VEC3")
		.addDef("HAS_TEXCOORD_0_VEC2")
		.addDef("DEBUG", _conf.view())
		.addDef("DEBUG_NONE", 0)
		.addDef("DEBUG_NORMAL_SHADING", 1)
		.addDef("DEBUG_NORMAL_TEXTURE", 2)
		.addDef("DEBUG_NORMAL_GEOMETRY", 3)
		.addDef("DEBUG_TANGENT", 4)
		.addDef("DEBUG_BITANGENT", 5)
		.addDef("DEBUG_ALPHA", 6)
		.addDef("DEBUG_UV_0", 7)
		.addDef("DEBUG_UV_1", 8)
		.addDef("DEBUG_OCCLUSION", 9)
		.addDef("DEBUG_EMISSIVE", 10)
		.addDef("DEBUG_BASE_COLOR", 11)
		.addDef("DEBUG_ROUGHNESS", 12)
		.addDef("DEBUG_METALLIC", 13)
		.addDef("DEBUG_CLEARCOAT_FACTOR", 14)
		.addDef("DEBUG_CLEARCOAT_ROUGHNESS", 15)
		.addDef("DEBUG_CLEARCOAT_NORMAL", 16)
		.addDef("DEBUG_SHEEN_COLOR", 17)
		.addDef("DEBUG_SHEEN_ROUGHNESS", 18)
		.addDef("DEBUG_SPECULAR_FACTOR", 19)
		.addDef("DEBUG_SPECULAR_COLOR", 20)
		.addDef("DEBUG_TRANSMISSION_FACTOR", 21)
		.addDef("DEBUG_VOLUME_THICKNESS", 22)
		.addDef("DEBUG_DIFFUSE_TRANSMISSION_FACTOR", 23)
		.addDef("DEBUG_DIFFUSE_TRANSMISSION_COLOR_FACTOR", 24)
		.addDef("DEBUG_IRIDESCENCE_FACTOR", 25)
		.addDef("DEBUG_IRIDESCENCE_THICKNESS", 26)
		.addDef("DEBUG_ANISOTROPIC_STRENGTH", 27)
		.addDef("DEBUG_ANISOTROPIC_DIRECTION", 28)
		.addDef("MATERIAL_METALLICROUGHNESS");

	for (const auto& [flag, define] : FragDefines) {
		if (_conf.flags() >= flag) { code.addDef(define); }
	}

	code.addFile(PARENT / "../shaders/pbr.frag").compile(frag, "FRAG");

	attachShaders({vert, frag});

	ASSERT_MESG(
		link(), "vert:\n{}\nfrag\n:{}", concat(vert.sources()),
		concat(frag.sources()));
}

PbrGL& PbrGL::setLights(const std::span<Light>& lights) {
	ASSERT(lights.size() == _conf.lightCount());

	for (const auto& [i, e] : std::ranges::enumerate_view(lights)) {
		setUniformT(fmt::format("u_Lights[{}].direction", i), e.direction);
		setUniformT(fmt::format("u_Lights[{}].range", i), e.range);
		setUniformT(fmt::format("u_Lights[{}].color", i), e.color);
		setUniformT(fmt::format("u_Lights[{}].intensity", i), e.intensity);
		setUniformT(fmt::format("u_Lights[{}].position", i), e.position);
		setUniformT(
			fmt::format("u_Lights[{}].innerConeCos", i), e.innerConeCos);
		setUniformT(
			fmt::format("u_Lights[{}].outerConeCos", i), e.outerConeCos);
		setUniformT(fmt::format("u_Lights[{}].type", i), e.type);
	}

	return *this;
}
