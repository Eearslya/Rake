#pragma once

#include <Luna/Utility/Delegate.hpp>
#include <Luna/Utility/Module.hpp>
#include <glm/glm.hpp>

struct GLFWwindow;
typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;

namespace Luna {
class Window : public Utility::Module::Registrar<Window> {
	static inline const bool Registered = Register("Window", Stage::Pre);

 public:
	Window();
	~Window() noexcept;

	virtual void Update() override;

	GLFWwindow* GetWindow() const {
		return _window;
	}
	float GetAspectRatio() const {
		return static_cast<float>(_size.x) / static_cast<float>(_size.y);
	}
	const glm::uvec2& GetFramebufferSize() const {
		return _framebufferSize;
	}
	const glm::uvec2& GetPosition() const {
		return _position;
	}
	const glm::uvec2& GetSize(bool checkFullscreen = true) const {
		return (_fullscreen && checkFullscreen) ? _sizeFullscreen : _size;
	}
	const std::string& GetTitle() const {
		return _title;
	}
	bool IsBorderless() const {
		return _borderless;
	}
	bool IsFloating() const {
		return _floating;
	}
	bool IsFocused() const {
		return _focused;
	}
	bool IsFullscreen() const {
		return _fullscreen;
	}
	bool IsIconified() const {
		return _iconified;
	}
	bool IsMaximized() const {
		return _maximized;
	}
	bool IsResizable() const {
		return _resizable;
	}

	std::vector<const char*> GetRequiredInstanceExtensions() const;
	VkSurfaceKHR CreateSurface(VkInstance instance) const;

	void Maximize();
	void SetBorderless(bool borderless);
	void SetFloating(bool floating);
	void SetFullscreen(bool fullscreen);
	void SetIconified(bool iconified);
	void SetPosition(const glm::uvec2& position);
	void SetResizable(bool resizable);
	void SetSize(const glm::uvec2& size);
	void SetTitle(const std::string& title);

	Utility::Delegate<void(bool)> OnBorderlessChanged;
	Utility::Delegate<void()> OnClosed;
	Utility::Delegate<void(bool)> OnFocusChanged;
	Utility::Delegate<void(bool)> OnFloatingChanged;
	Utility::Delegate<void(bool)> OnFullscreenChanged;
	Utility::Delegate<void(bool)> OnIconifiedChanged;
	Utility::Delegate<void(glm::uvec2)> OnMoved;
	Utility::Delegate<void(bool)> OnResizableChanged;
	Utility::Delegate<void(glm::uvec2)> OnResized;
	Utility::Delegate<void(std::string)> OnTitleChanged;

 private:
	static void CallbackError(int32_t error, const char* description);
	static void CallbackWindowClose(GLFWwindow* window);
	static void CallbackWindowFocus(GLFWwindow* window, int32_t focused);
	static void CallbackFramebufferSize(GLFWwindow* window, int32_t w, int32_t h);
	static void CallbackWindowIconify(GLFWwindow* window, int32_t iconified);
	static void CallbackWindowPosition(GLFWwindow* window, int32_t x, int32_t y);
	static void CallbackWindowSize(GLFWwindow* window, int32_t w, int32_t h);

	GLFWwindow* _window = nullptr;

	bool _borderless = false;
	bool _floating   = false;
	bool _focused    = true;
	bool _fullscreen = false;
	bool _iconified  = false;
	bool _maximized  = false;
	bool _resizable  = true;
	bool _titleDirty = false;

	glm::uvec2 _position;
	glm::uvec2 _sizeFullscreen;
	glm::uvec2 _size;
	glm::uvec2 _framebufferSize;
	std::string _title;
};
}  // namespace Luna
