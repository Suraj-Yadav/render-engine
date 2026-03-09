#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Time.h>
#include <Magnum/Platform/GlfwApplication.h>

#include <Magnum/ImGuiIntegration/Context.hpp>
#include <filesystem>

#include "camera.hpp"

namespace Magnum {
	template <typename T> class App : public Platform::Application {
		ImGuiIntegration::Context _imgui{NoCreate};
		ArcBall _camera;

		std::optional<T> main;

		void drawEvent() override {
			GL::defaultFramebuffer.clear(
				GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

			// Imgui Stuff
			_imgui.newFrame();
			/* Enable text input, if needed */
			if (ImGui::GetIO().WantTextInput && !isTextInputActive()) {
				startTextInput();
			} else if (!ImGui::GetIO().WantTextInput && isTextInputActive()) {
				stopTextInput();
			}

			// Draw normal stuff
			_camera.updateTransformation();
			main->draw(_camera);

			main->drawImgui();

			/* Update application cursor */
			_imgui.updateApplicationCursor(*this);

			/* Set appropriate states. If you only draw ImGui, it is
			sufficient
			   to just enable blending and scissor test in the constructor.
			   */
			GL::Renderer::enable(GL::Renderer::Feature::Blending);
			GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
			GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
			GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);

			_imgui.drawFrame();

			// /* Reset state. Only needed if you want to draw something else
			// with
			//    different state after. */
			GL::Renderer::disable(GL::Renderer::Feature::ScissorTest);
			GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
			GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
			GL::Renderer::disable(GL::Renderer::Feature::Blending);

			swapBuffers();
			redraw();
		}

		void viewportEvent(ViewportEvent& event) override {
			GL::defaultFramebuffer.setViewport({{}, event.framebufferSize()});

			_imgui.relayout(
				Vector2{event.windowSize()} / event.dpiScaling(),
				event.windowSize(), event.framebufferSize());

			_camera.reshape(event.windowSize(), event.framebufferSize());
		}

		void keyPressEvent(KeyEvent& event) override {
			if (_imgui.handleKeyPressEvent(event)) { return; }
			if (event.key() == Key::R) {
				_camera.reset();
				main->reset();
			} else if (event.key() == Key::Esc) {
				exit();
			} else if (!main->keyPressEvent(event)) {
				return;
			}
			event.setAccepted();
			redraw();
		}

		void keyReleaseEvent(KeyEvent& event) override {
			if (_imgui.handleKeyReleaseEvent(event)) { return; }
		}

		bool isCameraMove(auto& event, const auto& pointers) const {
			return isCameraRotate(event, pointers) &&
				   (event.modifiers() & Modifier::Shift);
		}

		bool isCameraRotate(auto& event, const auto& pointers) const {
			return (event.isPrimary() && (pointers & Pointer::MouseMiddle)) ||
				   (event.isPrimary() && (pointers & Pointer::MouseLeft) &&
					(event.modifiers() & Modifier::Alt));
		}

		bool isClick(auto& event, const auto& pointers) const {
			return event.isPrimary() && (pointers & Pointer::MouseLeft);
		}

		void pointerPressEvent(PointerEvent& event) override {
			if (_imgui.handlePointerPressEvent(event)) { return; }
			if (isCameraRotate(event, event.pointer()) ||
				isCameraMove(event, event.pointer())) {
				_camera.initTransformation(event.position());
			} else if (isClick(event, event.pointer())) {
				main->pointerPressEvent(event, _camera);
			} else {
				return;
			}

			event.setAccepted();
			redraw(); /* camera has changed, redraw! */
		}

		void pointerReleaseEvent(PointerEvent& event) override {
			if (_imgui.handlePointerReleaseEvent(event)) { return; }
			if (isCameraMove(event, event.pointer()) ||
				isCameraRotate(event, event.pointer())) {
			} else if (isClick(event, event.pointer())) {
				main->pointerPressEvent(event, _camera);
			}
		}

		void pointerMoveEvent(PointerMoveEvent& event) override {
			if (_imgui.handlePointerMoveEvent(event)) { return; }
			if (isCameraMove(event, event.pointers())) {
				_camera.translate(event.position());
			} else if (isCameraRotate(event, event.pointers())) {
				_camera.rotate(event.position());
			} else if (isClick(event, event.pointers())) {
				main->pointerMoveEvent(event, _camera);
			}

			event.setAccepted();
			redraw(); /* camera has changed, redraw! */
		}

		void scrollEvent(ScrollEvent& event) override {
			if (_imgui.handleScrollEvent(event)) {
				/* Prevent scrolling the page */
				event.setAccepted();
				return;
			}
			const Float delta = event.offset().y();
			if (Math::abs(delta) < 1.0e-2f) { return; }

			_camera.zoom(delta);

			event.setAccepted();
			redraw(); /* camera has changed, redraw! */
		}
		void textInputEvent(TextInputEvent& event) override {
			if (_imgui.handleTextInputEvent(event)) { return; }
		}

	   public:
		explicit App(const Arguments& arguments, const std::string& title)
			: Platform::Application{arguments, NoCreate} {
			/* Setup window */
			{
				const Vector2 dpi = dpiScaling({});
				Configuration conf;
				conf.setTitle(title)
					.setSize(conf.size(), dpi)
					.setWindowFlags(Configuration::WindowFlag::Resizable);
				GLConfiguration glConf;
				glConf.setSampleCount(dpi.max() < 2 ? 8 : 2);
				if (!tryCreate(conf, glConf)) {
					create(conf, glConf.setSampleCount(0));
				}
			}

			GL::Renderer::enable(GL::Renderer::Feature::DebugOutput);
			GL::Renderer::enable(GL::Renderer::Feature::DebugOutputSynchronous);
			GL::DebugOutput::setDefaultCallback();

			GL::DebugOutput::setEnabled(
				GL::DebugOutput::Source::Api, GL::DebugOutput::Type::Other,
				{131185}, false);

			const Vector2 size = Vector2{windowSize()} / dpiScaling();

			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.IniFilename = nullptr;
			if (constexpr auto font =
					"/usr/share/fonts/abattis-cantarell-fonts/"
					"Cantarell-Regular.otf";
				std::filesystem::exists(font)) {
				io.Fonts->Clear();
				io.FontDefault = io.Fonts->AddFontFromFileTTF(
					font, 20 * framebufferSize().x() / size.x());
				io.Fonts->Build();
			}

			_imgui = ImGuiIntegration::Context(
				*ImGui::GetCurrentContext(), size, windowSize(),
				framebufferSize());

			/* Set up proper blending to be used by ImGui. There's a great
		   chance you'll need this exact behavior for the rest of your scene. If
		   not, set this only for the drawFrame() call. */
			GL::Renderer::setBlendEquation(
				GL::Renderer::BlendEquation::Add,
				GL::Renderer::BlendEquation::Add);
			GL::Renderer::setBlendFunction(
				GL::Renderer::BlendFunction::SourceAlpha,
				GL::Renderer::BlendFunction::OneMinusSourceAlpha);

			using namespace Magnum::Math::Literals;

			/* Loop at 60 Hz max */
			setSwapInterval(1);
			setMinimalLoopPeriod(16.0_msec);

			// Setup Camera
			{
				const Vector3 eye = Vector3(-1, 0, 0), viewCenter,
							  up = Vector3::zAxis();
				const auto fov = 58.5_degf;
				_camera = ArcBall(eye, viewCenter, up, fov, windowSize());
				_camera.reshape(windowSize(), framebufferSize());
			}
			main.emplace();
		}
	};
}  // namespace Magnum
using KeyEvent = Magnum::Platform::Application::KeyEvent;
using PointerEvent = Magnum::Platform::Application::PointerEvent;
using PointerMoveEvent = Magnum::Platform::Application::PointerMoveEvent;
