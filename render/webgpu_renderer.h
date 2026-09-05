#pragma once

#include <functional>
#include <emscripten/em_types.h>
#include <webgpu/webgpu_cpp.h>
#include "armchair2/render/projection.h"
#include "armchair2/render/webgpu/context.h"
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

  enum class states {
    uninitialised,
    ready_to_init,
    waiting_for_device,
    ready_to_configure,
    ready_to_draw,
    failed,
  } state{states::uninitialised};

private:
  armchair::render::webgpu::context webgpu;

  wgpu::BindGroupLayout bind_group_layout_default;
  wgpu::RenderPipeline pipeline;

  std::function<void(armchair::render::webgpu::context const&)> postinit_callback; // the callback that is called once when init completes
  std::function<void()> main_loop_callback;                                     // the callback that is called repeatedly for the main loop after init

public:
  webgpu_renderer(logstorm::manager &logger);

  void init(std::function<void(armchair::render::webgpu::context const&)> &&postinit_callback, std::function<void()> &&main_loop_callback);

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
