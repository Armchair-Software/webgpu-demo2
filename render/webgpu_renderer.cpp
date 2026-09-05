#include "webgpu_renderer.h"
#include "logstorm/manager.h"
#include <array>
#include <vector>
#include <imgui/imgui_impl_wgpu.h>
#include "armchair2/render/webgpu/enum_name.h"
#include "vertex.h"
#include "triangle_index.h"
#include "uniforms.h"
#include "shaders/default.wgsl.h"

namespace render {

using armchair::render::webgpu::enum_name;
using webgpu_states = armchair::render::webgpu::states;

webgpu_renderer::webgpu_renderer(logstorm::manager &this_logger)
  : logger{this_logger},
    projection{
      .mode{armchair::render::fov_mode::diagonal},
      .field_of_view_degrees{110.0f},
      .near_plane{1.0f},
      .far_plane{100'000.0f},
    },
    webgpu{logger} {
  /// Construct a WebGPU renderer and populate those members that don't require delayed init
  webgpu.init();
}

void webgpu_renderer::configure() {
  /// When the device is ready, configure the WebGPU system
  assert(state == states::unconfigured && "webgpu_renderer::configure requires an unconfigured renderer");
  assert(webgpu.state == webgpu_states::ready_to_configure && "webgpu_renderer::configure requires WebGPU context ready_to_configure");
  logger << "WebGPU device ready, configuring surface";
  webgpu.canvas.update_size();                                                  // adapter/device acquisition is asynchronous, so refresh the browser dimensions immediately before configuration as they may have changed since construction.
  webgpu.configure_surface();

  logger << "WebGPU assembling shaders";
  {
    wgpu::ShaderSourceWGSL shader_module_wgsl_decriptor;
    shader_module_wgsl_decriptor.code = render::shaders::default_wgsl;
    wgpu::ShaderModuleDescriptor shader_module_descriptor{
      .nextInChain{&shader_module_wgsl_decriptor},
      .label{"Shader module 1"},
    };
    wgpu::ShaderModule shader_module{webgpu.device.CreateShaderModule(&shader_module_descriptor)};

    logger << "WebGPU configuring pipeline";

    std::array vertex_attributes{
      wgpu::VertexAttribute{
        .format{wgpu::VertexFormat::Float32x3},
        .offset{offsetof(vertex, position)},
        .shaderLocation{0},
      },
      wgpu::VertexAttribute{
        .format{wgpu::VertexFormat::Float32x3},
        .offset{offsetof(vertex, normal)},
        .shaderLocation{1},
      },
      wgpu::VertexAttribute{
        .format{wgpu::VertexFormat::Float32x4},
        .offset{offsetof(vertex, colour)},
        .shaderLocation{2},
      },
    };
    wgpu::VertexBufferLayout vertex_buffer_layout{
      .arrayStride{sizeof(vertex)},
      .attributeCount{vertex_attributes.size()},
      .attributes{vertex_attributes.data()},
    };

    wgpu::BlendState blend_state{
      .color{                                                                   // BlendComponent
        .operation{wgpu::BlendOperation::Add},                                  // initial values from https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/hello-triangle.html
        .srcFactor{wgpu::BlendFactor::SrcAlpha},
        .dstFactor{wgpu::BlendFactor::OneMinusSrcAlpha},
      },
      .alpha{                                                                   // BlendComponent
        .operation{wgpu::BlendOperation::Add},                                  // these differ from defaults
        .srcFactor{wgpu::BlendFactor::Zero},
        .dstFactor{wgpu::BlendFactor::One},
        // TODO: compare with defaults
      },
    };
    wgpu::ColorTargetState colour_target_state{
      .format{webgpu.surface_preferred_format},
      .blend{&blend_state},
    };
    wgpu::FragmentState fragment_state{
      .module{shader_module},
      .entryPoint{"fs_main"},
      .constantCount{0},
      .constants{nullptr},
      .targetCount{1},
      .targets{&colour_target_state},
    };

    wgpu::DepthStencilState depth_stencil_state{
      .format{webgpu.depth_texture_format},
      .depthWriteEnabled{true},
      .depthCompare{wgpu::CompareFunction::Less},
      .stencilFront{},                                                          // StencilFaceState
      .stencilBack{},                                                           // StencilFaceState
      .stencilReadMask{0},
      .stencilWriteMask{0},
      // TODO: tweak depth bias settings
    };

    wgpu::BindGroupLayoutEntry binding_layout{
      .binding{0},                                                              // binding index as used in the @binding attribute in the shader
      .visibility{wgpu::ShaderStage::Vertex},
      .buffer{                                                                  // BufferBindingLayout
        .type{wgpu::BufferBindingType::Uniform},
        .minBindingSize{sizeof(uniforms)},
      },
    };
    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor{
      .label{"Bind group layout 1"},
      .entryCount{1},
      .entries{&binding_layout},
    };
    bind_group_layout_default = webgpu.device.CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor{
      .label{"Pipeline layout 1"},
      .bindGroupLayoutCount{1},
      .bindGroupLayouts{&bind_group_layout_default},
    };
    wgpu::PipelineLayout pipeline_layout{webgpu.device.CreatePipelineLayout(&pipeline_layout_descriptor)};

    wgpu::RenderPipelineDescriptor render_pipeline_descriptor{
      .label{"Render pipeline 1"},
      .layout{std::move(pipeline_layout)},
      .vertex{                                                                  // VertexState
        .module{shader_module},
        .entryPoint{"vs_main"},
        .constantCount{0},
        .constants{nullptr},
        .bufferCount{1},
        .buffers{&vertex_buffer_layout},
      },
      .primitive{                                                               // PrimitiveState
        .cullMode{wgpu::CullMode::Back},
      },
      .depthStencil{&depth_stencil_state},
      .multisample{},
      .fragment{&fragment_state},
    };
    pipeline = webgpu.device.CreateRenderPipeline(&render_pipeline_descriptor);
  }

  logger << "WebGPU creating depth texture";
  webgpu.init_depth_texture();

  state = states::ready_to_draw;
}

void webgpu_renderer::draw(vec2f const& rotation) {
  /// Draw a frame
  assert(state == states::ready_to_draw && "webgpu_renderer::draw requires state ready_to_draw");
  assert(webgpu.state == webgpu_states::ready && "webgpu_renderer::draw requires a ready WebGPU context");
  wgpu::SurfaceTexture surface_texture;
  webgpu.surface.GetCurrentTexture(&surface_texture);
  if(surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal
     && surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
    throw std::runtime_error{"Could not get current texture from surface"};
  }

  wgpu::TextureView texture_view{surface_texture.texture.CreateView()};
  if(!texture_view) throw std::runtime_error{"Could not get current texture view from surface"};

  wgpu::CommandEncoderDescriptor command_encoder_descriptor{
    .label = "Command encoder 1"
  };
  wgpu::CommandEncoder command_encoder{webgpu.device.CreateCommandEncoder(&command_encoder_descriptor)};

  {
    // set up render pass
    wgpu::RenderPassColorAttachment render_pass_colour_attachment{
      .view{texture_view},
      .loadOp{wgpu::LoadOp::Clear},
      .storeOp{wgpu::StoreOp::Store},
      .clearValue{wgpu::Color{0, 0.5, 0.5, 1.0}},
    };

    wgpu::RenderPassDepthStencilAttachment render_pass_depth_stencil_attachment{
      .view{webgpu.depth_texture_view},
      .depthLoadOp{wgpu::LoadOp::Clear},
      .depthStoreOp{wgpu::StoreOp::Store},
      .depthClearValue{1.0f},
    };
    wgpu::RenderPassDescriptor render_pass_descriptor{
      .label{"Render pass 1"},
      .colorAttachmentCount{1},
      .colorAttachments{&render_pass_colour_attachment},
      .depthStencilAttachment{&render_pass_depth_stencil_attachment},
    };
    wgpu::RenderPassEncoder render_pass_encoder{command_encoder.BeginRenderPass(&render_pass_descriptor)};

    render_pass_encoder.SetPipeline(pipeline);                                  // select which render pipeline to use

    // set up test buffers
    std::vector<vertex> vertex_data{
      {{-1.0f, -1.0f, -1.0f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // bottom face normal & colour
      {{+1.0f, -1.0f, -1.0f}, {+1.0f,  0.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // right face normal & colour
      {{+1.0f, +1.0f, -1.0f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // front face normal & colour
      {{-1.0f, +1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // left face normal & colour
      {{-1.0f, -1.0f, +1.0f}, { 0.0f,  0.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // normal & colour not used
      {{+1.0f, -1.0f, +1.0f}, { 0.0f,  0.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // normal & colour not used
      {{+1.0f, +1.0f, +1.0f}, { 0.0f, +1.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // top face normal & colour
      {{-1.0f, +1.0f, +1.0f}, { 0.0f,  0.0f, +1.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // back face normal & colour
    };
    std::vector<triangle_index> index_data{
      {0, 1, 5}, {0, 5, 4},                                                     // bottom face (y = -1)
      {1, 6, 5}, {1, 2, 6},                                                     // right face (x = +1)
      {2, 1, 0}, {2, 0, 3},                                                     // front face (z = -1)
      {3, 0, 4}, {3, 4, 7},                                                     // left face (x = -1)
      {6, 3, 7}, {6, 2, 3},                                                     // top face (y = +1)
      {7, 4, 5}, {7, 5, 6},                                                     // back face (z = +1)
    };

    // set up matrices
    static vec2f angles;
    angles += rotation;
    angles.x += 0.01f;                                                          // constant slow spin
    quatf model_rotation{quatf::from_euler_angles_rad(0.0, angles.x, 0.0)};

    vec3f camera_pos{0.0f, 2.0f, -5.0f};
    camera_pos.rotate_rad_x(angles.y);

      mat4f projection_matrix{projection.matrix(static_cast<vec2f>(webgpu.canvas.framebuffer_size))};
    mat4f look_at{mat4f::create_look_at(
      camera_pos,                                                               // eye pos
      {0.0f, 0.0f, 0.0f},                                                       // target pos
      {0.0f, 1.0f, 0.0f}                                                        // up dir
    )};

    uniforms uniform_data{
        projection_matrix * look_at * model_rotation.transform(),
      mat3fwgpu{model_rotation.rotmatrix()},
    };

    // vertex buffer
    wgpu::BufferDescriptor vertex_buffer_descriptor{
      .label{"Vertex buffer 1"},
      .usage{wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex},
      .size{vertex_data.size() * sizeof(vertex_data[0])},
    };
    wgpu::Buffer vertex_buffer{webgpu.device.CreateBuffer(&vertex_buffer_descriptor)};
    webgpu.queue.WriteBuffer(
      vertex_buffer,                                                            // buffer
      0,                                                                        // offset
      vertex_data.data(),                                                       // data
      vertex_data.size() * sizeof(vertex_data[0])                               // size
    );

    // index buffer
    wgpu::BufferDescriptor index_buffer_descriptor{
      .label{"Index buffer 1"},
      .usage{wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index},
      .size{index_data.size() * sizeof(index_data[0])},
    };
    wgpu::Buffer index_buffer{webgpu.device.CreateBuffer(&index_buffer_descriptor)};
    webgpu.queue.WriteBuffer(
      index_buffer,                                                             // buffer
      0,                                                                        // offset
      index_data.data(),                                                        // data
      index_data.size() * sizeof(index_data[0])                                 // size
    );

    // uniform buffer
    wgpu::BufferDescriptor uniform_buffer_desecriptor{
      .label{"Uniform buffer 1"},
      .usage{wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform},
      .size{sizeof(uniform_data)},
    };
    wgpu::Buffer uniform_buffer{webgpu.device.CreateBuffer(&uniform_buffer_desecriptor)};
    webgpu.queue.WriteBuffer(
      uniform_buffer,                                                           // buffer
      0,                                                                        // offset
      &uniform_data,                                                            // data
      sizeof(uniform_data)                                                      // size
    );

    // uniform bind group
    wgpu::BindGroupEntry bind_group_entry{
      .binding{0},
      .buffer{uniform_buffer},
      .size{sizeof(uniforms)},
    };
    wgpu::BindGroupDescriptor bind_group_descriptor{
      .label{"Bind group 1"},
      .layout{bind_group_layout_default},
      .entryCount{1},                                                           // must correspond to layout
      .entries{&bind_group_entry},
    };
    wgpu::BindGroup bind_group{webgpu.device.CreateBindGroup(&bind_group_descriptor)};

    render_pass_encoder.SetVertexBuffer(0, vertex_buffer, 0, vertex_buffer.GetSize()); // slot, buffer, offset, size
    render_pass_encoder.SetIndexBuffer(index_buffer, wgpu::IndexFormat::Uint16, 0, index_buffer.GetSize()); // buffer, format, offset, size
    render_pass_encoder.SetBindGroup(0, bind_group);                            // groupIndex, group, dynamicOffsetCount = 0, dynamicOffsets = nullptr
    render_pass_encoder.DrawIndexed(index_data.size() * decltype(index_data)::value_type::size()); // indexCount, instanceCount = 1, firstIndex = 0, baseVertex = 0, firstInstance = 0

    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), render_pass_encoder.Get()); // render the outstanding GUI draw data

    // TODO: add timestamp query: https://eliemichel.github.io/LearnWebGPU/advanced-techniques/benchmarking/time.html
    render_pass_encoder.End();
  }

  command_encoder.InsertDebugMarker("Debug marker 1");

  wgpu::CommandBufferDescriptor command_buffer_descriptor {
    .label = "Command buffer 1"
  };
  wgpu::CommandBuffer command_buffer{command_encoder.Finish(&command_buffer_descriptor)};

  //webgpu.queue.OnSubmittedWorkDone(
  //  [](WGPUQueueWorkDoneStatus status_c, void *data){
  //    /// Submitted work done callback - note, this only fires for the subsequent submit
  //    auto &renderer{*static_cast<webgpu_renderer*>(data)};
  //    auto &logger{renderer.logger};
  //    if(auto const status{static_cast<wgpu::QueueWorkDoneStatus>(status_c)}; status != wgpu::QueueWorkDoneStatus::Success) {
  //      logger << "ERROR: WebGPU queue submitted work failure, status: " << enum_name<wgpu::QueueWorkDoneStatus>(status_c);
  //    }
  //    logger << "DEBUG: WebGPU queue submitted work done";
  //  },
  //  this
  //);

  webgpu.queue.Submit(1, &command_buffer);
}

}
