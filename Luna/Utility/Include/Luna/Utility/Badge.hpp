#pragma once

namespace Luna {
namespace Utility {
template <typename T>
class Badge final {
	friend T;

 private:
	Badge() = default;
};
}  // namespace Utility
}  // namespace Luna
