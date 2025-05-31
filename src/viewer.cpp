#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Primitives/UVSphere.h>
#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Trade/MeshData.h>
#include <imgui.h>
#include <tinyfiledialogs.h>

#include "env.hpp"
#include "loader.hpp"
#include "shader.hpp"
#include "window.hpp"

std::optional<std::filesystem::path> openFile() {
	auto* selection =
		tinyfd_openFileDialog("Open File", nullptr, 0, nullptr, nullptr, 0);
	if (selection == nullptr) { return {}; }
	return selection;
}

using namespace Magnum;

struct Main {
	EnvMap environment;
	DrawInfo drawInfo;

	Vector4 input = Vector4(0, 0, 0, 1);
	Matrix4 model;

	std::vector<PbrMetallicRoughness> shaders;

	Main() : environment("./images/abandoned_garage_4k.hdr") {}

	void updateModel() {
		model = Matrix4::rotationZ(Math::Deg(input.z())) *
				Matrix4::rotationY(Math::Deg(input.y())) *
				Matrix4::rotationX(Math::Deg(input.x())) *
				Matrix4::scaling(Vector3(input.w()));
	}

	void updateShaders() {
		shaders.clear();

		for (auto& mat : drawInfo.materials) {
			PbrMetallicRoughness::Flags flags = PbrMetallicRoughness::Flag::IBL;

#define assign(T, F) \
	if (mat.T != -1) { flags |= PbrMetallicRoughness::Flag::F; }
			assign(baseColorTexture, BaseColorTexture);
			assign(aoTexture, AoTexture);
			assign(metalnessTexture, MetalnessTexture);
			assign(roughnessTexture, RoughnessTexture);
			assign(emissionTexture, EmissiveTexture);
			assign(normalTexture, NormalTexture);
#undef assign

			shaders.emplace_back(
				PbrMetallicRoughness::Configuration{}.setLightCount(4).setFlags(
					flags));

			shaders.back().setLightPositions({
				Vector3(-10.0f, 10.0f, 10.0f),
				Vector3(10.0f, 10.0f, 10.0f),
				Vector3(-10.0f, -10.0f, 10.0f),
				Vector3(10.0f, -10.0f, 10.0f),
			});
			shaders.back().setLightColors({
				// Vector3(1, 0, 0) * 300,
				// Vector3(0, 1, 0) * 300,
				// Vector3(0, 0, 1) * 300,
				Vector3(1, 1, 1) * 300,
				Vector3(1, 1, 1) * 300,
				Vector3(1, 1, 1) * 300,
				Vector3(1, 1, 1) * 300,
			});
		}
	}

	void load() {
		auto path = openFile();
		if (!path) { return; }

		drawInfo = loadMesh(*path);

		updateShaders();
		updateModel();
	}

	void drawImgui() {
		using namespace ImGui;
		if (Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			Text(
				"Average %.3f ms/frame (%.1f FPS)",
				1000.0 / Double(GetIO().Framerate), Double(GetIO().Framerate));

			{
				bool changed = DragFloat3("Rotation", input.data(), 15, 0, 360);
				changed |= SliderFloat(
					"Scale", &input.w(), 0.0001, 10, "%.3f",
					ImGuiSliderFlags_Logarithmic);
				if (Button("Reset Model")) {
					changed = true;
					input = Vector4(0, 0, 0, 1);
				}
				if (changed) { updateModel(); }
			}
			if (Button("Change Background")) {
				auto path = openFile();
				if (path) { environment = EnvMap(*path); }
			}
		}

		End();
	}

	void draw(Magnum::ArcBall& camera) {
		for (auto& [meshId, matId] : drawInfo.meshMatPairs) {
			auto& shader = shaders[matId];

			auto& mesh = drawInfo.meshs[meshId];
			auto& mat = drawInfo.materials[matId];

			// Set camera Stuff
			shader.setUniformT("u_Camera", camera.position());
			shader.setUniformT("u_projection", camera.projectionMatrix());
			shader.setUniformT("u_view", camera.viewMatrix());

			// Set colors
			shader.setUniformT("u_baseColor", mat.baseColor);
			shader.setUniformT("u_metalness", mat.metalness);
			shader.setUniformT("u_roughness", mat.roughness);
			shader.setUniformT("u_ao", mat.ao);
			shader.setUniformT("u_emissive", mat.emission);

#define ASSIGN_TEXTURES(U, T, I) \
	if (mat.T != -1) { shader.setTexture(U, drawInfo.textures[mat.T], I); }

			ASSIGN_TEXTURES("u_baseColorTexture", baseColorTexture, 0);
			ASSIGN_TEXTURES("u_metalnessTexture", metalnessTexture, 1);
			ASSIGN_TEXTURES("u_roughnessTexture", roughnessTexture, 2);
			ASSIGN_TEXTURES("u_aoTexture", aoTexture, 3);
			ASSIGN_TEXTURES("u_normalTexture", normalTexture, 4);
			ASSIGN_TEXTURES("u_emissiveTexture", emissionTexture, 5);
#undef ASSIGN_TEXTURES

			shader.setTexture("u_irradianceMap", environment.irradiance(), 6);
			shader.setTexture("u_prefilterMap", environment.prefilter(), 7);
			shader.setTexture("u_brdfLUT", environment.brdfLUT(), 8);

			shader.setUniformT("u_model", model)
				.setUniformT("u_normal", model.normalMatrix());

			shader.draw(mesh);
		}

		environment.draw(camera);
	}

	void reset() {}

	bool keyPressEvent(KeyEvent& event) {
		using Key = Magnum::Platform::Application::Key;
		using Modifier = Magnum::Platform::Application::Modifier;

		if (event.key() == Key::O && (event.modifiers() & Modifier::Ctrl)) {
			load();
			return true;
		}
		if (event.key() == Key::Space) {}
		return false;
	}
	void pointerPressEvent(PointerEvent& event, Magnum::ArcBall& _camera) {}

	void pointerReleaseEvent(PointerEvent& event, Magnum::ArcBall& _camera) {
		// _camera.screenCoordToWorld(event.position());
		using Modifier = Magnum::Platform::Application::Modifier;

		if (event.modifiers() & Modifier::Alt) {}
	}

	void pointerMoveEvent(PointerMoveEvent& event, Magnum::ArcBall& _camera) {}
};

int main(int argc, char** argv) {
	spdlog::cfg::load_env_levels();

	CPPTRACE_TRY {
		// test();
		Magnum::App<Main> app({argc, argv}, "GLTF/GLB Viewer");
		return app.exec();
	}
	CPPTRACE_CATCH(const std::exception& e) {
		SPDLOG_CRITICAL("Exception: {}", e.what());
		cpptrace::from_current_exception().print();
	}
}
