#pragma once

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
  ///     - Wait, repeatedly checking until webgpu.state == ready_to_configure,
  ///   - Call configure(),
  ///   - Initialise gui or anything else that needs the surface
  ///   - Start main loop
  ///     - Normal loop logic
  ///     - Call gui.draw() (renderer state == ready_to_draw)
  ///     - Call renderer.draw()
  logstorm::manager &logger;

public:
  armchair::render::perspective_projection projection;
  armchair::render::webgpu::context webgpu;

  enum class states {
    unconfigured,
    ready_to_draw,
  } state{states::unconfigured};

private:
  wgpu::BindGroupLayout bind_group_layout_default;
  wgpu::RenderPipeline pipeline;

public:
  webgpu_renderer(logstorm::manager &logger);

  void configure();

  void draw(vec2f const& rotation);
};

}
