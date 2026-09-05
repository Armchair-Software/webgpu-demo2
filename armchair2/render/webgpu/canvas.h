#pragma once

#include "vectorstorm/vector/vector2.h"

namespace armchair::render::webgpu {

class canvas_state {
public:
  using resize_callback = void (*)(canvas_state const&, void*);

  vec2d css_size;
  vec2ui framebuffer_size;
  double device_pixel_ratio{1.0};

  void observe(resize_callback callback, void *callback_data);
  bool update_size();

private:
  resize_callback callback{nullptr};
  void *callback_data{nullptr};

  void notify_resize();
};

}
