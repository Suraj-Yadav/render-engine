#pragma once
#include <Corrade/Containers/BigEnumSet.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/Math/Vector3.h>

#include <filesystem>
#include <map>

#include "logging.hpp"

class CustomShader : public Magnum::GL::AbstractShaderProgram {
	std::map<std::string, int> uniforms;

   public:
	explicit CustomShader(
		const std::filesystem::path& vertPath,
		const std::filesystem::path& fragPath);

	template <typename T>
	CustomShader& setUniformT(const std::string& key, const T& v) {
		if (!uniforms.contains(key)) {
			auto loc = uniformLocation(key);
			ASSERT(loc != -1);
			uniforms[key] = loc;
		}
		setUniform(uniforms[key], v);
		return *this;
	}
	template <typename T>
	CustomShader& setTexture(
		const std::string& key, T& image, int textureUnit) {
		setUniformT(key, textureUnit);
		image.texture.bind(textureUnit);
		return *this;
	}
};

struct Light {
	Magnum::Vector3 direction, color, position;
	float range{}, intensity = 1, innerConeCos{}, outerConeCos{};
	int type = 1;
};

constexpr auto LIGHT_TYPES = "Directional\0Point\0Spot\0\0";

constexpr auto VIEW_OPTIONS =
	"DEBUG_NONE\0DEBUG_NORMAL_SHADING\0DEBUG_NORMAL_TEXTURE\0DEBUG_NORMAL_"
	"GEOMETRY\0DEBUG_TANGENT\0DEBUG_BITANGENT\0DEBUG_ALPHA\0DEBUG_UV_0\0DEBUG_"
	"UV_1\0DEBUG_OCCLUSION\0DEBUG_EMISSIVE\0DEBUG_BASE_COLOR\0DEBUG_"
	"ROUGHNESS\0DEBUG_METALLIC\0DEBUG_CLEARCOAT_FACTOR\0DEBUG_CLEARCOAT_"
	"ROUGHNESS\0DEBUG_CLEARCOAT_NORMAL\0DEBUG_SHEEN_COLOR\0DEBUG_SHEEN_"
	"ROUGHNESS\0DEBUG_SPECULAR_FACTOR\0DEBUG_SPECULAR_COLOR\0DEBUG_"
	"TRANSMISSION_FACTOR\0DEBUG_VOLUME_THICKNESS\0DEBUG_DIFFUSE_TRANSMISSION_"
	"FACTOR\0DEBUG_DIFFUSE_TRANSMISSION_COLOR_FACTOR\0DEBUG_IRIDESCENCE_"
	"FACTOR\0DEBUG_IRIDESCENCE_THICKNESS\0DEBUG_ANISOTROPIC_STRENGTH\0DEBUG_"
	"ANISOTROPIC_DIRECTION\0\0";

class PbrGL : public Magnum::GL::AbstractShaderProgram {
   public:
	enum class Flag : std::uint8_t {
		None = 0,
		MaterialClearCoat,
		MaterialSheen,
		MaterialTransmission,
		MaterialDiffuseTransmission,
		MaterialSpecular,
		MaterialVolume,
		MaterialIridescence,
		MaterialAnisotropy,
		MaterialEmissiveStrength,
		MaterialDispersion,
		MaterialIor,
		NormalTexture,
		NormalUVTransform,
		OcclusionTexture,
		OcclusionUVTransform,
		EmissiveTexture,
		EmissiveUVTransform,
		BaseColorTexture,
		BaseColorUVTransform,
		MetallicRoughnessTexture,
		MetallicRoughnessUVTransform,
		ClearcoatTexture,
		ClearcoatUVTransform,
		ClearcoatRoughnessTexture,
		ClearcoatRoughnessUVTransform,
		ClearcoatNormalTexture,
		ClearcoatNormalUVTransform,
		SheenColorTexture,
		SheenColorUVTransform,
		SheenRoughnessTexture,
		SheenRoughnessUVTransform,
		TransmissionTexture,
		TransmissionUVTransform,
		DiffuseTransmissionTexture,
		DiffuseTransmissionUVTransform,
		DiffuseTransmissionColorTexture,
		DiffuseTransmissionColorUVTransform,
		SpecularTexture,
		SpecularUVTransform,
		SpecularColorTexture,
		SpecularColorUVTransform,
		ThicknessTexture,
		ThicknessUVTransform,
		IridescenceTexture,
		IridescenceUVTransform,
		IridescenceThicknessTexture,
		IridescenceThicknessUVTransform,
		AnisotropyTexture,
		AnisotropyUVTransform,
		ImageBasedLighting,
		PunctualLights,
		Instancing,
		VertexColor3,
		VertexColor4
	};

	using Flags = Corrade::Containers::BigEnumSet<Flag>;

	class Configuration {
		Flags _flags;
		Magnum::UnsignedInt _lightCount = 1, _view = 0;

	   public:
		explicit Configuration() = default;

		[[nodiscard]] auto view() const { return _view; }
		Configuration& setView(Magnum::UnsignedInt view) {
			_view = view;
			return *this;
		}
		[[nodiscard]] auto lightCount() const { return _lightCount; }
		Configuration& setLightCount(Magnum::UnsignedInt count) {
			_lightCount = count;
			return *this;
		}
		[[nodiscard]] auto flags() const { return _flags; }
		Configuration& setFlags(Flags flags) {
			_flags = flags;
			return *this;
		}
	};

   private:
	std::map<std::string, int> uniforms;

	int getLoc(const std::string& key) {
		if (!uniforms.contains(key)) {
			auto loc = uniformLocation(key);
			// if (loc == -1) { throw std::runtime_error("missing location"); }
			uniforms[key] = loc;
		}
		return uniforms[key];
	}
	Configuration _conf;

   public:
	PbrGL() = default;
	PbrGL(const Configuration& conf) : _conf(conf) { compile(); }

	void compile();

	template <typename T>
	PbrGL& setUniformT(const std::string& key, const T& v) {
		if (auto loc = getLoc(key); loc != -1) { setUniform(loc, v); }
		return *this;
	}
	template <typename T>
	PbrGL& setTexture(const std::string& key, T& image, int textureUnit) {
		if (auto loc = getLoc(key); loc != -1) {
			setUniform(loc, textureUnit);
			image.texture.bind(textureUnit);
		}
		return *this;
	}

	PbrGL& setLights(const std::span<Light>& lights);
};
