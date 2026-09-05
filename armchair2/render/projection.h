#pragma once

#include "vectorstorm/matrix/matrix4.h"
#include "vectorstorm/vector/vector2.h"

namespace armchair::render {

enum class fov_mode {
  horizontal,
  vertical,
  diagonal,
};

struct perspective_projection {
  fov_mode mode;
  float field_of_view_degrees;
  float near_plane;
  float far_plane;

  [[nodiscard]] mat4f matrix(vec2f const &viewport_size) const;
};

} // namespace armchair::render
