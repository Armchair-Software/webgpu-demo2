#pragma once

#include <string_view>
#include <webgpu/webgpu_cpp.h>

namespace armchair::render::webgpu {

[[nodiscard]] inline std::string_view dawn_to_string_view(wgpu::StringView string_in) {
  return static_cast<std::string_view>(string_in);
}

}
