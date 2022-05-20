#include <Luna/Graphics/Vulkan/Cookie.hpp>
#include <Luna/Graphics/Vulkan/Device.hpp>

namespace Luna {
namespace Vulkan {
Cookie::Cookie(Device& device) : _cookie(device.AllocateCookie({})) {}
}  // namespace Vulkan
}  // namespace Luna
