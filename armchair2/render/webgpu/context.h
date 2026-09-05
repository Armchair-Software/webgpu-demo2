#pragma once

#include <webgpu/webgpu_cpp.h>
#include "canvas.h"
#include "logstorm/logstorm_forward.h"

//#define DEBUG_WEBGPU

namespace armchair::render::webgpu {

enum class states {
  uninitialised,
  ready_to_init,
  waiting_for_device,
  ready_to_configure,
  ready_to_draw,
  failed,
};

class context {
  logstorm::manager &logger;

public:
  wgpu::Instance instance{wgpu::CreateInstance()};
  wgpu::Surface surface;
  wgpu::Adapter adapter;
  wgpu::Device device;
  wgpu::Queue queue;

  wgpu::Texture depth_texture;
  wgpu::TextureView depth_texture_view;

  wgpu::TextureFormat surface_preferred_format{wgpu::TextureFormat::Undefined};
  static constexpr wgpu::TextureFormat depth_texture_format{wgpu::TextureFormat::Depth24Plus};

  canvas_state canvas;
  states state{states::uninitialised};

  context(logstorm::manager &logger);

  void init();
};

}
