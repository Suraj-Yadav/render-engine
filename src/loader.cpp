#include "loader.hpp"

#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/String.h>
#include <Magnum/GL/CubeMapTexture.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Primitives/Square.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/PbrClearCoatMaterialData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/Trade.h>

#include <stdexcept>

using namespace Magnum;

bool check(Trade::MaterialData& data, const int& layer, auto attrib) {
	return layer < data.layerCount() && data.hasAttribute(layer, attrib);
}

bool check(Trade::MaterialData& data, const auto& layer, auto attrib) {
	return data.hasLayer(layer) && data.hasAttribute(layer, attrib);
}

template <typename T>
void get(
	Trade::MaterialData& data, std::vector<MaterialInfo::Data<T>>& vec,
	PbrGL::Flags& flags, const auto& layer, auto attrib,
	std::string_view uniform, PbrGL::Flag flag = PbrGL::Flag::None) {
	if (!check(data, layer, attrib)) { return; }
	vec.push_back({uniform, data.attribute<T>(layer, attrib)});
	flags |= flag;
}

template <typename T>
void get(
	Trade::MaterialData& data, std::vector<MaterialInfo::Data<T>>& vec,
	PbrGL::Flags& flags, const auto& layer, auto attrib,
	std::string_view uniform, const T& val,
	PbrGL::Flag flag = PbrGL::Flag::None) {
	if (!check(data, layer, attrib)) {
		vec.push_back({uniform, val});
		return;
	}
	vec.push_back({uniform, data.attributeOr<T>(layer, attrib, val)});
	flags |= flag;
}

void parseMaterial(Trade::MaterialData& data, DrawInfo& info) {
	using Att = Trade::MaterialAttribute;
	using Flag = PbrGL::Flag;
	MaterialInfo m;

	m.doubleSided = data.isDoubleSided();

	m.baseColor = {
		"u_BaseColorFactor", data.attributeOr(Att::BaseColor, Color4(1))};
	m.metallic = {"u_MetallicFactor", data.attributeOr(Att::Metalness, 1.0f)};
	m.roughness = {"u_RoughnessFactor", data.attributeOr(Att::Roughness, 1.0f)};

#define getTexture(F, A_TEX, A_TRANS)                                   \
	get(data, m.textures, m.flags, l, A_TEX, "u_" #F "Sampler",         \
		Flag::F##Texture);                                              \
	get(data, m.transforms, m.flags, l, A_TRANS, "u_" #F "UVTransform", \
		Flag::F##UVTransform)
#define getFloat(A, U) get(data, m.floats, m.flags, l, A, U)
#define getColor(A, U) get(data, m.colors, m.flags, l, A, U)

	{
		constexpr auto l = 0;

		// Normal Stuff
		getTexture(Normal, Att::NormalTexture, Att::NormalTextureMatrix);
		if (check(data, l, Att::NormalTexture)) {
			get(data, m.floats, m.flags, l, Att::NormalTextureScale,
				"u_NormalScale", 1.0f);
		}

		// Occlusion Stuff
		getTexture(
			Occlusion, Att::OcclusionTexture, Att::OcclusionTextureMatrix);
		if (check(data, l, Att::OcclusionTexture)) {
			get(data, m.floats, m.flags, l, Att::OcclusionTextureStrength,
				"u_OcclusionStrength", 1.0f);
		}

		// Emissive Stuff
		getColor(Att::EmissiveColor, "u_EmissiveFactor");
		getTexture(Emissive, Att::EmissiveTexture, Att::EmissiveTextureMatrix);

		// PbrMetallicRoughness stuff
		getTexture(
			BaseColor, Att::BaseColorTexture, Att::BaseColorTextureMatrix);

		getTexture(
			MetallicRoughness, Att::NoneRoughnessMetallicTexture,
			Att::MetalnessTextureMatrix);
	}

	if (constexpr const auto l = Trade::MaterialLayer::ClearCoat;
		data.hasLayer(l)) {
		m.flags |= Flag ::MaterialClearCoat;

		getFloat(Att::LayerFactor, "u_ClearcoatFactor");
		getTexture(
			Clearcoat, Att::LayerFactorTexture, Att::LayerFactorTextureMatrix);

		getFloat(Att::Roughness, "u_ClearcoatRoughnessFactor");
		getTexture(
			ClearcoatRoughness, Att::RoughnessTexture,
			Att::RoughnessTextureMatrix);

		getTexture(
			ClearcoatNormal, Att::NormalTexture, Att::NormalTextureMatrix);
		if (check(data, l, Att::NormalTexture)) {
			get(data, m.floats, m.flags, l, Att::NormalTextureScale,
				"u_ClearcoatNormalScale", 1.0f);
		}
	}

	if (constexpr auto l = "#KHR_materials_sheen"; data.hasLayer(l)) {
		m.flags |= Flag ::MaterialSheen;
		getColor("sheenColorFactor", "u_SheenColorFactor");
		getTexture(SheenColor, "sheenColorTexture", "sheenColorTextureMatrix");
		getFloat("sheenRoughnessFactor", "u_SheenRoughnessFactor");
		getTexture(
			SheenRoughness, "sheenRoughnessTexture",
			"sheenRoughnessTextureMatrix");
	}

	if (constexpr auto l = "#KHR_materials_transmission"; data.hasLayer(l)) {
		m.flags |= Flag ::MaterialTransmission;
		m.opaque = false;
		getFloat("transmissionFactor", "u_TransmissionFactor");
		getTexture(
			Transmission, "transmissionTexture", "transmissionTextureMatrix");
	}

	if (constexpr auto l = "#KHR_materials_diffuse_transmission";
		data.hasLayer(l)) {
		m.flags |= Flag ::MaterialDiffuseTransmission;

		getFloat("diffuseTransmissionFactor", "u_DiffuseTransmissionFactor");
		getColor(
			"diffuseTransmissionColorFactor",
			"u_DiffuseTransmissionColorFactor");
		getTexture(
			DiffuseTransmission, "diffuseTransmissionTexture",
			"diffuseTransmissionTextureMatrix");
		getTexture(
			DiffuseTransmissionColor, "diffuseTransmissionColorTexture",
			"diffuseTransmissionColorTextureMatrix");
	}

	if (constexpr auto l = "#KHR_materials_specular"; data.hasLayer(l)) {
		m.flags |= Flag ::MaterialSpecular;

		getFloat("specularFactor", "u_KHR_materials_specular_specularFactor");
		getColor(
			"specularColorFactor",
			"u_KHR_materials_specular_specularColorFactor");
		getTexture(Specular, "specularTexture", "specularTextureMatrix");
		getTexture(
			SpecularColor, "specularColorTexture",
			"specularColorTextureMatrix");
	}

	if (constexpr auto l = "#KHR_materials_volume"; data.hasLayer(l)) {
		m.flags |= Flag ::MaterialVolume;

		getFloat("thicknessFactor", "u_ThicknessFactor");
		getFloat("attenuationDistance", "u_AttenuationDistance");
		getColor("attenuationColor", "u_AttenuationColor");
		getTexture(Thickness, "thicknessTexture", "thicknessTextureMatrix");
	}

	if (constexpr auto l = "#KHR_materials_iridescence"; data.hasLayer(l)) {
		m.flags |= Flag ::MaterialIridescence;

		getFloat("iridescenceFactor", "u_IridescenceFactor");
		getFloat("iridescenceIor", "u_IridescenceIor");
		getFloat(
			"iridescenceThicknessMinimum", "u_IridescenceThicknessMinimum");
		getFloat(
			"iridescenceThicknessMaximum", "u_IridescenceThicknessMaximum");
		getTexture(
			Iridescence, "iridescenceTexture", "iridescenceTextureMatrix");
		getTexture(
			IridescenceThickness, "iridescenceThicknessTexture",
			"iridescenceThicknessTextureMatrix");
	}

	if (constexpr auto l = "#KHR_materials_anisotropy"; data.hasLayer(l)) {
		m.flags |= Flag ::MaterialAnisotropy;

		if (check(data, l, "anisotropyStrength")) {
			auto factor = data.attributeOr(l, "anisotropyStrength", 0.0f);
			auto rotation = data.attributeOr(l, "anisotropyRotation", 0.0f);
			m.colors.push_back(
				{"u_Anisotropy",
				 Vector3(std::cos(rotation), std::sin(rotation), factor)});
		}
		getTexture(Anisotropy, "anisotropyTexture", "anisotropyTextureMatrix");
	}

	if (constexpr auto l = "#KHR_materials_emissive_strength";
		data.hasLayer(l)) {
		m.flags |= Flag ::MaterialEmissiveStrength;

		getFloat("emissiveStrength", "u_EmissiveStrength");
	}

	if (constexpr auto l = "#KHR_materials_dispersion"; data.hasLayer(l)) {
		m.flags |= Flag ::MaterialDispersion;

		getFloat("dispersion", "u_Dispersion");
	}

	if (constexpr auto l = "#KHR_materials_ior"; data.hasLayer(l)) {
		m.flags |= Flag ::MaterialIor;

		getFloat("ior", "u_Ior");
	}

#undef getFloat
#undef getColor
#undef getTexture
#undef getTransform

	info.materials.push_back(m);
}

DrawInfo loadMesh(const std::filesystem::path& path) {
	static Magnum::PluginManager::Manager<Trade::AbstractImporter> manager;
	static Containers::Pointer<Trade::AbstractImporter> importer(
		manager.loadAndInstantiate("AnySceneImporter"));

	if (!importer) {
		throw std::runtime_error("Unable to initialize mesh importer");
	}

	if (!importer->openFile(path.string())) {
		throw std::runtime_error("Unable to load mesh");
	}

	DrawInfo info;

	for (auto i = 0u; i < importer->textureCount(); ++i) {
		auto textureData = importer->texture(i);
		using Corrade::Utility::format;

		if (!textureData ||
			textureData->type() != Trade::TextureType::Texture2D) {
			throw std::runtime_error(
				format("Invalid Texture: {}: {}", i, importer->textureName(i)));
		}
		auto data = importer->image2D(textureData->image());

		if (!data || data->isCompressed()) {
			throw std::runtime_error(format(
				"Cannot load image: {}: {}", textureData->image(),
				importer->image2DName(textureData->image())));
		}

		info.textures.emplace_back(
			data->size(), data->format(), Math::log2(data->size().max()) + 1);
		info.textures.back()
			.setStorage()
			.setMagnificationFilter(textureData->magnificationFilter())
			.setMinificationFilter(
				textureData->minificationFilter(), textureData->mipmapFilter())
			.setWrapping(textureData->wrapping().xy())
			.setSubImage(0, {}, *data)
			.generateMipmap();
	}

	for (auto i = 0u; i < importer->materialCount(); ++i) {
		auto name = importer->materialName(i);
		if (!importer->material(i)) {
			throw std::runtime_error(
				fmt::format("Cannot load Material: {}", name));
		}
		auto data = *importer->material(i);
		parseMaterial(data, info);
	}

	for (auto i = 0u; i != importer->meshCount(); ++i) {
		auto meshData = importer->mesh(i);
		if (!meshData) {
			throw std::runtime_error(
				fmt::format(
					"Cannot load image: {}: {}", i, importer->meshName(i)));
		}
		MeshTools::CompileFlags flags;
		if (!meshData->hasAttribute(Trade::MeshAttribute::Normal)) {
			flags |= MeshTools::CompileFlag::GenerateFlatNormals;
		}
		info.meshs.emplace_back(MeshTools::compile(*meshData, flags));
	}

	auto scene = importer->scene(importer->defaultScene());
	if (!scene || !scene->is3D()) {
		throw std::runtime_error(
			fmt::format(
				"Cannot load image: {}: {}", importer->defaultScene(),
				importer->sceneName(importer->defaultScene())));
	}
	for (const auto& elem : scene->meshesMaterialsAsArray()) {
		auto obj = Containers::Optional<Long>(elem.first());
		auto mesh = elem.second().first();
		auto mat = elem.second().second();

		auto transform = Magnum::Matrix4(Math::IdentityInit);

		for (auto n = obj; n && n != -1; n = scene->parentFor(*n)) {
			if (scene->transformation3DFor(*n)) {
				transform = *scene->transformation3DFor(*n) * transform;
			}
		}

		info.nodes.emplace_back(mesh, mat, transform);
	}
	return info;
}
