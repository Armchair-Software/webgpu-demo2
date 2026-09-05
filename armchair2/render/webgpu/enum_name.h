#pragma once

#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <magic_enum/magic_enum.hpp>

namespace armchair::render::webgpu {

template<typename Tcpp, typename Tc>
  requires (!std::is_same_v<Tcpp, Tc>)
[[nodiscard]] std::string enum_name(Tc enum_in) {
  /// Attempt to interpret an enum into its most human-readable form, with fallbacks for unknown types
  /// Tc is the C API enum (WGPU...), Tcpp is the C++ API enum equivalent (wgpu::...)
  using value_type = std::underlying_type_t<Tc>;
  auto const enum_value{static_cast<value_type>(enum_in)};

  if(auto enum_out_opt{magic_enum::enum_cast<Tcpp>(enum_value)}; enum_out_opt.has_value()) {
    return std::string{magic_enum::enum_name(*enum_out_opt)};
  }

  if(auto enum_out_opt{magic_enum::enum_cast<Tc>(enum_value)}; enum_out_opt.has_value()) {
    return std::string{magic_enum::enum_name(*enum_out_opt)} + " (C binding only)";
  }

  std::ostringstream oss;
  oss << "unknown enum 0x" << std::hex << enum_value;
  return oss.str();
}

template<typename Tenum>
[[nodiscard]] std::string enum_name(Tenum enum_in) {
  /// Interpret a C++ WebGPU enum directly
  if(auto const enum_name{magic_enum::enum_name(enum_in)}; !enum_name.empty()) {
    return std::string{enum_name};
  }

  std::ostringstream oss;
  oss << "unknown enum 0x" << std::hex << static_cast<std::underlying_type_t<Tenum>>(enum_in);
  return oss.str();
}

}
