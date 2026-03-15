#include <Magnum/Animation/Easing.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Time.h>
#include <Magnum/Platform/GlfwApplication.h>
#include <imgui.h>

#include <Magnum/ImGuiIntegration/Context.hpp>
#include <filesystem>

#include "camera.hpp"

struct AppOptions {
	std::string title = "Test";
	float fps = 60.0f;
	std::filesystem::path fontpath =
		"/usr/share/fonts/abattis-cantarell-fonts/Cantarell-Regular.otf";
	int fontSize = 20;
};

using Key = Magnum::Platform::Application::Key;
using KeyEvent = Magnum::Platform::Application::KeyEvent;
using Modifier = Magnum::Platform::Application::Modifier;
using PointerEvent = Magnum::Platform::Application::PointerEvent;
using PointerMoveEvent = Magnum::Platform::Application::PointerMoveEvent;

class MainBase {
	AppOptions options;

   public:
	explicit MainBase(AppOptions options) : options(std::move(options)) {}
	virtual ~MainBase() = default;

	MainBase(const MainBase&) = delete;
	MainBase(MainBase&&) = delete;
	MainBase& operator=(const MainBase&) = delete;
	MainBase& operator=(MainBase&&) = delete;

	[[nodiscard]] const auto& opts() const { return options; }

	void drawFPS() const {
		auto fps = ImGui::GetIO().Framerate;
		auto msPerFrame = 1000.0f / fps;
		auto factor = fps / options.fps;
		auto color = Magnum::Math::lerp(
			Magnum::Color4::red(), Magnum::Color4::green(),
			Magnum::Animation::Easing::exponentialInOut(factor));

		ImGui::TextColored(
			ImVec4(color.r(), color.g(), color.b(), color.a()),
			"Avg %.2f ms/frame (%.2f FPS)", msPerFrame, fps);
	}
	virtual void drawImgui() { drawFPS(); }
	virtual void draw(Magnum::ArcBall& /* camera */) {}
	virtual void reset() {}
	virtual bool keyPressEvent(KeyEvent& /* event */) { return false; }
	virtual void pointerPressEvent(
		PointerEvent& /* event */, Magnum::ArcBall& /* camera */) {}
	virtual void pointerReleaseEvent(
		PointerEvent& /* event */, Magnum::ArcBall& /* camera */) {
		// camera.screenCoordToWorld(event.position());
	}
	virtual void pointerMoveEvent(
		PointerMoveEvent& /* event */, Magnum::ArcBall& /* camera */) {}
};

template <typename T>
	requires std::derived_from<T, MainBase>
class App : public Magnum::Platform::Application {
	Magnum::ImGuiIntegration::Context _imgui{Magnum::NoCreate};
	Magnum::ArcBall _camera;
	std::optional<T> main;

	void drawEvent() override {
		Magnum::GL::defaultFramebuffer.clear(
			Magnum::GL::FramebufferClear::Color |
			Magnum::GL::FramebufferClear::Depth);

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
		Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::Blending);
		Magnum::GL::Renderer::enable(
			Magnum::GL::Renderer::Feature::ScissorTest);
		Magnum::GL::Renderer::disable(
			Magnum::GL::Renderer::Feature::FaceCulling);
		Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::DepthTest);

		_imgui.drawFrame();

		// /* Reset state. Only needed if you want to draw something else
		// with
		//    different state after. */
		Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::DepthTest);
		Magnum::GL::Renderer::enable(
			Magnum::GL::Renderer::Feature::FaceCulling);
		Magnum::GL::Renderer::disable(
			Magnum::GL::Renderer::Feature::ScissorTest);
		Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::Blending);

		swapBuffers();
		redraw();
	}

	void viewportEvent(ViewportEvent& event) override {
		Magnum::GL::defaultFramebuffer.setViewport(
			{{}, event.framebufferSize()});

		_imgui.relayout(
			Magnum::Vector2{event.windowSize()} / event.dpiScaling(),
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
		const Magnum::Float delta = event.offset().y();
		if (Magnum::Math::abs(delta) < 1.0e-2f) { return; }

		_camera.zoom(delta);

		event.setAccepted();
		redraw(); /* camera has changed, redraw! */
	}
	void textInputEvent(TextInputEvent& event) override {
		if (_imgui.handleTextInputEvent(event)) { return; }
	}

   public:
	explicit App(const Arguments& arguments, const AppOptions& opts)
		: Magnum::Platform::Application{arguments, Magnum::NoCreate} {
		/* Setup window */
		{
			const Magnum::Vector2 dpi = dpiScaling({});
			Configuration conf;
			conf.setTitle(opts.title)
				.setSize(conf.size(), dpi)
				.setWindowFlags(Configuration::WindowFlag::Resizable);
			GLConfiguration glConf;
			glConf.setSampleCount(dpi.max() < 2 ? 8 : 2);
			if (!tryCreate(conf, glConf)) {
				create(conf, glConf.setSampleCount(0));
			}
		}

		Magnum::GL::Renderer::enable(
			Magnum::GL::Renderer::Feature::DebugOutput);
		Magnum::GL::Renderer::enable(
			Magnum::GL::Renderer::Feature::DebugOutputSynchronous);
		Magnum::GL::DebugOutput::setDefaultCallback();

		Magnum::GL::DebugOutput::setEnabled(
			Magnum::GL::DebugOutput::Source::Api,
			Magnum::GL::DebugOutput::Type::Other, {131185}, false);

		const Magnum::Vector2 size =
			Magnum::Vector2{windowSize()} / dpiScaling();

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		if (std::filesystem::exists(opts.fontpath)) {
			io.Fonts->Clear();
			io.FontDefault = io.Fonts->AddFontFromFileTTF(
				opts.fontpath.c_str(),
				opts.fontSize * framebufferSize().x() / size.x());
			io.Fonts->Build();
		}

		_imgui = Magnum::ImGuiIntegration::Context(
			*ImGui::GetCurrentContext(), size, windowSize(), framebufferSize());

		/* Set up proper blending to be used by ImGui. There's a great
	   chance you'll need this exact behavior for the rest of your scene. If
	   not, set this only for the drawFrame() call. */
		Magnum::GL::Renderer::setBlendEquation(
			Magnum::GL::Renderer::BlendEquation::Add,
			Magnum::GL::Renderer::BlendEquation::Add);
		Magnum::GL::Renderer::setBlendFunction(
			Magnum::GL::Renderer::BlendFunction::SourceAlpha,
			Magnum::GL::Renderer::BlendFunction::OneMinusSourceAlpha);

		using namespace Magnum::Math::Literals;

		// Set Max FPS
		setMinimalLoopPeriod(1000.0_msec / opts.fps);

		// Setup Camera
		{
			const Magnum::Vector3 eye = Magnum::Vector3(-1, 0, 0), viewCenter,
								  up = Magnum::Vector3::zAxis();
			const auto fov = 58.5_degf;
			_camera = Magnum::ArcBall(eye, viewCenter, up, fov, windowSize());
			_camera.reshape(windowSize(), framebufferSize());
		}
		main.emplace(opts);
	}
};
