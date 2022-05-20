#pragma once

namespace Luna {
namespace Utility {
class NonCopyable {
 public:
	NonCopyable()                   = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable(NonCopyable&&)      = default;
	NonCopyable& operator=(const NonCopyable&) = delete;
	NonCopyable& operator=(NonCopyable&&) = default;
	~NonCopyable() noexcept               = default;
};
}  // namespace Utility
}  // namespace Luna
