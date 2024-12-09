#pragma once
#include "logstorm/logstorm_forward.h"
#include "clipboard.h"

class ImGui_ImplWGPU_InitInfo;

namespace gui {

class gui_renderer {
  logstorm::manager &logger;

  clipboard clipboard;

public:
  gui_renderer(logstorm::manager &logger);

  void init(ImGui_ImplWGPU_InitInfo &wgpu_info);

  void draw() const;
};

}
