#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

using Entity = std::uint32_t;

constexpr std::size_t kMaxEntities = 5000;
constexpr Entity kInvalidEntity = std::numeric_limits<Entity>::max();
