#pragma once

#include <functional>
#include <emscripten/em_types.h>
#include <webgpu/webgpu_cpp.h>
#include "armchair2/render/projection.h"
#include "logstorm/logstorm_forward.h"
#include "vectorstorm/vector/vector2.h"

namespace render {

class webgpu_renderer {
  /// Initialisation and run flow should be as follows:
  ///   - Construct the renderer,
  ///   - Start wait loop
  ///     - Wait, repeatedly checking until state == ready_to_configure,
  ///   - Call configure(),
  ///   - Initialise gui or anything else that needs the surface
  ///   - Start main loop
  ///     - Normal loop logic
  ///     - Call gui.draw() (state == ready_to_draw)
  ///     - Call renderer.draw()
  logstorm::manager &logger;

public:
  armchair::render::perspective_projection projection;

  struct webgpu_data {
    wgpu::Instance instance{wgpu::CreateInstance()};                            // the underlying WebGPU instance
    wgpu::Surface surface;                                                      // the canvas surface for rendering
    wgpu::Adapter adapter;                                                      // WebGPU adapter once it has been acquired
    wgpu::Device device;                                                        // WebGPU device once it has been acquired
    wgpu::Queue queue;                                                          // the queue for this device, once it has been acquired
    wgpu::BindGroupLayout bind_group_layout_default;                            // layout for the default uniform bind group
    wgpu::RenderPipeline pipeline;                                              // the render pipeline currently in use

    wgpu::Texture depth_texture;                                                // depth buffer
    wgpu::TextureView depth_texture_view;

    wgpu::TextureFormat surface_preferred_format{wgpu::TextureFormat::Undefined}; // preferred texture format for this surface
    static constexpr wgpu::TextureFormat depth_texture_format{wgpu::TextureFormat::Depth24Plus}; // what format to use for the depth texture

  private:
    webgpu_data() = default;
    friend class webgpu_renderer;
  };

  enum class states {
    uninitialised,
    ready_to_init,
    waiting_for_device,
    ready_to_configure,
    ready_to_draw,
    failed,
  } state{states::uninitialised};

private:
  webgpu_data webgpu;

  struct window_data {
    vec2d css_viewport_size;                                                    // canvas viewport size in CSS pixels
    vec2ui viewport_size;                                                       // our idea of the size of the viewport we render to, in real pixels
    double device_pixel_ratio{1.0};
  } window;

  std::function<void(webgpu_data const&)> postinit_callback;                    // the callback that is called once when init completes
  std::function<void()> main_loop_callback;                                     // the callback that is called repeatedly for the main loop after init

public:
  webgpu_renderer(logstorm::manager &logger);

  void init(std::function<void(webgpu_data const&)> &&postinit_callback, std::function<void()> &&main_loop_callback);

private:
  bool update_viewport_size();
  void configure_surface();
  void init_depth_texture();

  void wait_to_configure_loop();
  void configure();

public:
  void draw(vec2f const& rotation);
};

}
