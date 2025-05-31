#pragma once

#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/GL/AbstractShaderProgram.h>

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

class PbrMetallicRoughness : public Magnum::GL::AbstractShaderProgram {
   public:
	enum class Flag : Magnum::UnsignedByte {
		BaseColorTexture = 1 << 0,
		MetalnessTexture = 1 << 1,
		RoughnessTexture = 1 << 2,
		NormalTexture = 1 << 3,
		AoTexture = 1 << 4,
		EmissiveTexture = 1 << 5,
		IBL = 1 << 6,
	};

	using Flags = Corrade::Containers::EnumSet<Flag>;

	class Configuration {
		Flags _flags;
		Magnum::UnsignedInt _lightCount = 1;

	   public:
		explicit Configuration() = default;

		[[nodiscard]] auto flags() const { return _flags; }
		[[nodiscard]] auto lightCount() const { return _lightCount; }
		Configuration& setFlags(Flags flags) {
			_flags = flags;
			return *this;
		}
		Configuration& setLightCount(Magnum::UnsignedInt count) {
			_lightCount = count;
			return *this;
		}
	};

   private:
	std::map<std::string, int> uniforms;

	int getLoc(const std::string& key) {
		if (!uniforms.contains(key)) {
			auto loc = uniformLocation(key);
			if (loc == -1) { throw std::runtime_error("missing location"); }
			uniforms[key] = loc;
		}
		return uniforms[key];
	}
	Configuration _conf;

   public:
	PbrMetallicRoughness() = default;
	PbrMetallicRoughness(const Configuration& conf) : _conf(conf) { compile(); }

	void compile();

	template <typename T>
	PbrMetallicRoughness& setUniformT(const std::string& key, const T& v) {
		setUniform(getLoc(key), v);
		return *this;
	}
	template <typename T>
	PbrMetallicRoughness& setTexture(
		const std::string& key, T& image, int textureUnit) {
		setUniformT(key, textureUnit);
		image.texture.bind(textureUnit);
		return *this;
	}

	PbrMetallicRoughness& setLightPositions(
		const std::initializer_list<Magnum::Vector3>& lightPositions);

	PbrMetallicRoughness& setLightColors(
		const std::initializer_list<Magnum::Vector3>& lightColors);
};
