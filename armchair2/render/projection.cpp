#include "projection.h"
#include <cmath>
#include <utility>

namespace armchair::render {

mat4f perspective_projection::matrix(vec2f const &viewport_size) const {
  /// Set up a projection matrix based on the field of view and clipping planes
  float const field_of_view_radians{deg2rad(field_of_view_degrees)};

  #pragma GCC diagnostic push
  #pragma GCC diagnostic error "-Wswitch"                                      // enforce exhaustive switch here
  switch(mode) {
  #pragma GCC diagnostic pop
  case fov_mode::horizontal:
    {
      float const aspect_ratio{viewport_size.y / viewport_size.x};
      float const right{std::tan(field_of_view_radians * 0.5f) * near_plane};
      float const top{right * aspect_ratio};
      return mat4f::create_frustum(-right, right, -top, top, near_plane, far_plane);
    }
  case fov_mode::vertical:
    {
      float const aspect_ratio{viewport_size.x / viewport_size.y};
      float const top{std::tan(field_of_view_radians * 0.5f) * near_plane};
      float const right{top * aspect_ratio};
      return mat4f::create_frustum(-right, right, -top, top, near_plane, far_plane);
    }
  case fov_mode::diagonal:
    {
      float const diagonal{std::tan(field_of_view_radians * 0.5f) * near_plane};
      float const viewport_diagonal{viewport_size.length()};
      float const right{diagonal * (viewport_size.x / viewport_diagonal)};
      float const top{diagonal * (viewport_size.y / viewport_diagonal)};
      return mat4f::create_frustum(-right, right, -top, top, near_plane, far_plane);
    }
    // no default case, to enforce exhaustive switch
  }
  std::unreachable();
}

} // namespace armchair::render
