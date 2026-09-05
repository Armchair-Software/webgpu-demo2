#include "canvas.h"
#include <stdexcept>
#include <emscripten.h>
#include <emscripten/html5.h>

namespace armchair::render::webgpu {

void canvas_state::observe(resize_callback this_callback, void *this_callback_data) {
  /// Keep the canvas backing size synchronized with its browser device-pixel content box
  callback = this_callback;
  callback_data = this_callback_data;

  auto const browser_resize_callback{+[](canvas_state *canvas) {
    canvas->notify_resize();
  }};
  EM_ASM({
    const canvas = Module["canvas"];
    if(!canvas.__webgpu_device_pixel_resize_observer) {
      const resize_callback = wasmTable.get($0);
      const set_canvas_size = (width, height) => {
        width = Math.max(1, Math.round(width));
        height = Math.max(1, Math.round(height));
        if(canvas.width !== width) canvas.width = width;
        if(canvas.height !== height) canvas.height = height;
      };
      const set_approximate_canvas_size = () => {
        const rect = canvas.getBoundingClientRect();
        set_canvas_size(rect.width * window.devicePixelRatio, rect.height * window.devicePixelRatio);
      };
      set_approximate_canvas_size();
      if(typeof ResizeObserver !== "undefined") {
        const has_device_pixel_content_box = typeof ResizeObserverEntry !== "undefined" && "devicePixelContentBoxSize" in ResizeObserverEntry.prototype;
        const observer = new ResizeObserver((entries) => {
          const entry = entries[0];
          const device_sizes = has_device_pixel_content_box ? entry.devicePixelContentBoxSize : null;
          const device_size = device_sizes && device_sizes.length ? device_sizes[0] : device_sizes;
          if(device_size) set_canvas_size(device_size.inlineSize, device_size.blockSize);
          else set_approximate_canvas_size();
          resize_callback($1);
        });
        observer.observe(canvas, {box: has_device_pixel_content_box ? "device-pixel-content-box" : "content-box"});
        canvas.__webgpu_device_pixel_resize_observer = observer;
        if(!has_device_pixel_content_box) console.warn("ResizeObserver device-pixel-content-box is unavailable; canvas sizing will approximate using devicePixelRatio.");
      } else {
        console.warn("ResizeObserver is unavailable; canvas sizing will approximate using devicePixelRatio.");
        window.addEventListener("resize", () => {
          set_approximate_canvas_size();
          resize_callback($1);
        });
        canvas.__webgpu_device_pixel_resize_observer = true;
      }
    }
  }, browser_resize_callback, this);

  update_size();
}

bool canvas_state::update_size() {
  /// Refresh the CSS and device-pixel canvas sizes, and return whether the framebuffer size changed
  int framebuffer_width{0};
  int framebuffer_height{0};
  if(emscripten_get_canvas_element_size("#canvas", &framebuffer_width, &framebuffer_height) != EMSCRIPTEN_RESULT_SUCCESS) {
    throw std::runtime_error{"Could not read canvas framebuffer size"};
  }
  vec2ui const new_framebuffer_size{static_cast<unsigned int>(framebuffer_width), static_cast<unsigned int>(framebuffer_height)};
  if(new_framebuffer_size.x == 0 || new_framebuffer_size.y == 0) return false;

  double css_width{0.0};
  double css_height{0.0};
  if(emscripten_get_element_css_size("#canvas", &css_width, &css_height) != EMSCRIPTEN_RESULT_SUCCESS) {
    throw std::runtime_error{"Could not read canvas CSS size"};
  }

  bool const framebuffer_size_changed{new_framebuffer_size != framebuffer_size};
  css_size.assign(css_width, css_height);
  device_pixel_ratio = emscripten_get_device_pixel_ratio();
  framebuffer_size = new_framebuffer_size;
  return framebuffer_size_changed;
}

void canvas_state::notify_resize() {
  if(update_size() && callback) callback(*this, callback_data);
}

}
