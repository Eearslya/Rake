#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>

namespace Luna {
namespace Utility {
void* AlignedAlloc(size_t size, size_t alignment);
void* AlignedCalloc(size_t size, size_t alignment);
void AlignedFree(void* ptr);
}  // namespace Utility
}  // namespace Luna
