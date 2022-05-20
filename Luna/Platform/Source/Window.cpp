#include <GLFW/glfw3.h>

#include <Luna/Platform/Window.hpp>
#include <Luna/Utility/Log.hpp>

namespace Luna {
void Window::CallbackError(int32_t error, const char* description) {
	Log::Error("Window", "GLFW Error {}: {}", error, description);
}

void Window::CallbackWindowClose(GLFWwindow* window) {
	Window::Get()->OnClosed();
}

void Window::CallbackWindowFocus(GLFWwindow* window, int32_t focused) {
	auto me      = Window::Get();
	me->_focused = focused == GLFW_TRUE;
	me->OnFocusChanged(me->_focused);
}

void Window::CallbackFramebufferSize(GLFWwindow* window, int32_t w, int32_t h) {
	auto me              = Window::Get();
	me->_framebufferSize = {w, h};
}

void Window::CallbackWindowIconify(GLFWwindow* window, int32_t iconified) {
	auto me        = Window::Get();
	me->_iconified = iconified == GLFW_TRUE;
	me->OnIconifiedChanged(me->_iconified);
}

void Window::CallbackWindowPosition(GLFWwindow* window, int32_t x, int32_t y) {
	auto me = Window::Get();
	if (me->_fullscreen) { return; }
	me->_position = {x, y};
	me->OnMoved(me->_position);
}

void Window::CallbackWindowSize(GLFWwindow* window, int32_t w, int32_t h) {
	if (w <= 0 || h <= 0) { return; }

	auto me = Window::Get();
	if (me->_fullscreen) {
		me->_sizeFullscreen = {w, h};
		me->OnResized(me->_sizeFullscreen);
	} else {
		me->_size = {w, h};
		me->OnResized(me->_size);
	}
}

Window::Window() : _size(1280, 720), _title("Luna") {
	glfwSetErrorCallback(CallbackError);

	if (glfwInit() == GLFW_FALSE) { throw std::runtime_error("Failed to initialize GLFW!"); }

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);
	glfwWindowHint(GLFW_STEREO, GLFW_FALSE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	const auto monitor   = glfwGetPrimaryMonitor();
	const auto videoMode = glfwGetVideoMode(monitor);

	_window = glfwCreateWindow(_size.x, _size.y, _title.c_str(), nullptr, nullptr);
	if (_window == nullptr) {
		glfwTerminate();
		throw std::runtime_error("Failed to create window!");
	}

	glfwSetWindowUserPointer(_window, this);
	glfwSetWindowAttrib(_window, GLFW_DECORATED, !_borderless);
	glfwSetWindowAttrib(_window, GLFW_FLOATING, _floating);
	glfwSetWindowAttrib(_window, GLFW_RESIZABLE, _resizable);

	_position = {(videoMode->width - _size.x) / 2, (videoMode->height - _size.y) / 2};
	glfwSetWindowPos(_window, _position.x, _position.y);

	if (_fullscreen) { SetFullscreen(true); }

	glfwShowWindow(_window);

	glfwSetFramebufferSizeCallback(_window, CallbackFramebufferSize);
	glfwSetWindowCloseCallback(_window, CallbackWindowClose);
	glfwSetWindowFocusCallback(_window, CallbackWindowFocus);
	glfwSetWindowIconifyCallback(_window, CallbackWindowIconify);
	glfwSetWindowPosCallback(_window, CallbackWindowPosition);
	glfwSetWindowSizeCallback(_window, CallbackWindowSize);

	int w, h;
	glfwGetWindowSize(_window, &w, &h);
	_size = {w, h};
	glfwGetFramebufferSize(_window, &w, &h);
	_framebufferSize = {w, h};
}

Window::~Window() noexcept {
	glfwDestroyWindow(_window);
	glfwTerminate();
}

void Window::Update() {
	glfwPollEvents();

	_maximized = glfwGetWindowAttrib(_window, GLFW_MAXIMIZED) == GLFW_TRUE;

	if (_titleDirty) {
		glfwSetWindowTitle(_window, _title.c_str());
		_titleDirty = false;
	}
}

std::vector<const char*> Window::GetRequiredInstanceExtensions() const {
	uint32_t extensionCount = 0;
	const auto extensions   = glfwGetRequiredInstanceExtensions(&extensionCount);
	return std::vector<const char*>(extensions, extensions + extensionCount);
}

VkSurfaceKHR Window::CreateSurface(VkInstance instance) const {
	VkSurfaceKHR surface  = VK_NULL_HANDLE;
	const VkResult result = glfwCreateWindowSurface(instance, _window, nullptr, &surface);
	if (result == VK_SUCCESS) {
		return surface;
	} else {
		throw std::runtime_error("Failed to create window surface!");
	}
}

void Window::Maximize() {
	glfwMaximizeWindow(_window);
}

void Window::SetBorderless(bool borderless) {
	if (borderless == _borderless) { return; }
	_borderless = borderless;
	glfwSetWindowAttrib(_window, GLFW_DECORATED, !_borderless);
	OnBorderlessChanged(_borderless);
}

void Window::SetFloating(bool floating) {
	if (floating == _floating) { return; }
	_floating = floating;
	glfwSetWindowAttrib(_window, GLFW_FLOATING, !_floating);
	OnFloatingChanged(_floating);
}

void Window::SetFullscreen(bool fullscreen) {
	if (fullscreen == _fullscreen) { return; }

	int monitorX, monitorY;
	const auto selected  = glfwGetPrimaryMonitor();
	const auto videoMode = glfwGetVideoMode(selected);
	glfwGetMonitorPos(selected, &monitorX, &monitorY);
	_fullscreen = fullscreen;

	if (_fullscreen) {
		_sizeFullscreen = {videoMode->width, videoMode->height};
		glfwSetWindowMonitor(_window, selected, 0, 0, _sizeFullscreen.x, _sizeFullscreen.y, GLFW_DONT_CARE);
	} else {
		_position = glm::uvec2((glm::ivec2(videoMode->width, videoMode->height) - glm::ivec2(_size)) / 2) +
		            glm::uvec2(monitorX, monitorY);
		glfwSetWindowMonitor(_window, nullptr, _position.x, _position.y, _size.x, _size.y, GLFW_DONT_CARE);
	}

	OnFullscreenChanged(_fullscreen);
}

void Window::SetIconified(bool iconified) {
	if (iconified == _iconified) { return; }
	_iconified = iconified;
	if (_iconified) {
		glfwIconifyWindow(_window);
	} else {
		glfwRestoreWindow(_window);
	}
}

void Window::SetPosition(const glm::uvec2& position) {
	if (position.x >= 0) { _position.x = position.x; }
	if (position.y >= 0) { _position.y = position.y; }
	glfwSetWindowPos(_window, _position.x, _position.y);
}

void Window::SetResizable(bool resizable) {
	if (resizable == _resizable) { return; }
	_resizable = resizable;
	glfwSetWindowAttrib(_window, GLFW_RESIZABLE, !_resizable);
	OnResizableChanged(_resizable);
}

void Window::SetSize(const glm::uvec2& size) {
	if (size.x >= 0) { _size.x = size.x; }
	if (size.y >= 0) { _size.y = size.y; }
	glfwSetWindowSize(_window, _size.x, _size.y);
}

void Window::SetTitle(const std::string& title) {
	_title      = title;
	_titleDirty = true;
	OnTitleChanged(_title);
}
}  // namespace Luna
