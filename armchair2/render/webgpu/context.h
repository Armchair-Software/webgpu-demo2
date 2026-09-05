#pragma once

#include <webgpu/webgpu_cpp.h>
#include "vectorstorm/vector/vector2.h"

namespace armchair::render::webgpu {

struct context {
  wgpu::Instance instance{wgpu::CreateInstance()};
  wgpu::Surface surface;
  wgpu::Adapter adapter;
  wgpu::Device device;
  wgpu::Queue queue;

  wgpu::Texture depth_texture;
  wgpu::TextureView depth_texture_view;

  wgpu::TextureFormat surface_preferred_format{wgpu::TextureFormat::Undefined};
  static constexpr wgpu::TextureFormat depth_texture_format{wgpu::TextureFormat::Depth24Plus};

  struct canvas_state {
    vec2d css_size;
    vec2ui framebuffer_size;
    double device_pixel_ratio{1.0};
  } canvas;
};

}
