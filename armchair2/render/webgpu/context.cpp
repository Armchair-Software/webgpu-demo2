#include "context.h"
#include <cassert>
#include <cmath>
#include <iomanip>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <magic_enum/magic_enum.hpp>
#include "dawn_to_string_view.h"
#include "enum_name.h"
#include "logstorm/manager.h"

namespace armchair::render::webgpu {

context::context(logstorm::manager &this_logger)
  : logger{this_logger} {
  if(!instance) throw std::runtime_error{"Could not initialize WebGPU"};
}

void context::init() {
  /// Initialise the universal WebGPU context and asynchronously acquire its adapter and device
  assert(state == states::uninitialised && "WebGPU context init requires state uninitialised");

  logger << "WebGPU: Viewport size: " << canvas.css_size << " (nominal device pixels: approx " << canvas.css_size * canvas.device_pixel_ratio << ", framebuffer " << canvas.framebuffer_size << ")";
  logger << "WebGPU: CSS viewport size: " << canvas.css_size << " CSS pixels (framebuffer: " << canvas.framebuffer_size << " device pixels)";
  logger << "WebGPU: Device pixel ratio: " << canvas.device_pixel_ratio << " device pixels per CSS pixel (" << static_cast<unsigned int>(std::round(100.0 * canvas.device_pixel_ratio)) << "% scale)";

  {
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector surface_descriptor_from_canvas;
    surface_descriptor_from_canvas.selector = "#canvas";

    wgpu::SurfaceDescriptor surface_descriptor{
      .nextInChain{&surface_descriptor_from_canvas},
      .label{"Canvas surface"},
    };
    surface = instance.CreateSurface(&surface_descriptor);
  }
  if(!surface) throw std::runtime_error{"Could not create WebGPU surface"};
  state = states::ready_to_init;

  wgpu::RequestAdapterOptions adapter_request_options{
    .powerPreference{wgpu::PowerPreference::HighPerformance},
    .compatibleSurface{surface},
  };

  state = states::waiting_for_device;
  instance.RequestAdapter(
    &adapter_request_options,
    wgpu::CallbackMode::AllowSpontaneous,
    [](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter_in, wgpu::StringView message, context *webgpu_ptr){
      /// Request adapter callback
      auto &webgpu{*webgpu_ptr};
      auto &logger{webgpu.logger};
      if(message.length) logger << "WebGPU: Request adapter callback message: " << dawn_to_string_view(message);
      if(status != wgpu::RequestAdapterStatus::Success) {
        logger << "ERROR: WebGPU adapter request failure, status " << enum_name<wgpu::RequestAdapterStatus>(status);
        throw std::runtime_error{"WebGPU: Could not get adapter"};
      }

      auto &adapter{webgpu.adapter};
      adapter = std::move(adapter_in);
      if(!adapter) throw std::runtime_error{"WebGPU: Could not acquire adapter"};

      wgpu::SurfaceCapabilities surface_capabilities;
      webgpu.surface.GetCapabilities(adapter, &surface_capabilities);

      // report surface and adapter capabilities
      #ifdef DEBUG_WEBGPU
        {
          for(size_t i{0}; i != surface_capabilities.formatCount; ++i) {
            logger << "DEBUG: WebGPU surface capabilities: texture formats: " << magic_enum::enum_name(surface_capabilities.formats[i]);
          }
          for(size_t i{0}; i != surface_capabilities.presentModeCount; ++i) {
            logger << "DEBUG: WebGPU surface capabilities: present modes: " << magic_enum::enum_name(surface_capabilities.presentModes[i]);
          }
          for(size_t i{0}; i != surface_capabilities.alphaModeCount; ++i) {
            logger << "DEBUG: WebGPU surface capabilities: alpha modes: " << magic_enum::enum_name(surface_capabilities.alphaModes[i]);
          }
        }
      #endif // DEBUG_WEBGPU

      if(surface_capabilities.formatCount != 0) {
        webgpu.surface_preferred_format = surface_capabilities.formats[0];
      }
      logger << "WebGPU surface preferred format for this adapter: " << magic_enum::enum_name(webgpu.surface_preferred_format);
      if(webgpu.surface_preferred_format == wgpu::TextureFormat::Undefined) {
        webgpu.surface_preferred_format = wgpu::TextureFormat::RGBA8Unorm;
        logger << "WebGPU manually specifying preferred format: " << magic_enum::enum_name(webgpu.surface_preferred_format);
      }

      {
        wgpu::AdapterInfo adapter_info;
        adapter.GetInfo(&adapter_info);
        #ifdef DEBUG_WEBGPU
          logger << "DEBUG: WebGPU adapter info: vendor: " << dawn_to_string_view(adapter_info.vendor);
          logger << "DEBUG: WebGPU adapter info: architecture: " << dawn_to_string_view(adapter_info.architecture);
          logger << "DEBUG: WebGPU adapter info: device: " << dawn_to_string_view(adapter_info.device);
          logger << "DEBUG: WebGPU adapter info: description: " << dawn_to_string_view(adapter_info.description);
          logger << "DEBUG: WebGPU adapter info: vendorID:deviceID: " << adapter_info.vendorID << ":" << adapter_info.deviceID;
          logger << "DEBUG: WebGPU adapter info: backendType: " << magic_enum::enum_name(adapter_info.backendType);
          logger << "DEBUG: WebGPU adapter info: adapterType: " << magic_enum::enum_name(adapter_info.adapterType);
        #endif // DEBUG_WEBGPU
        logger << "WebGPU adapter info: " << dawn_to_string_view(adapter_info.description) << " (" << magic_enum::enum_name(adapter_info.backendType) << ", " << dawn_to_string_view(adapter_info.vendor) << ", " << dawn_to_string_view(adapter_info.architecture) << ")";
      }

      std::set<wgpu::FeatureName> adapter_features;
      {
        // see https://developer.mozilla.org/en-US/docs/Web/API/GPUSupportedFeatures and https://www.w3.org/TR/webgpu/#feature-index
        wgpu::SupportedFeatures adapter_supported_features;
        adapter.GetFeatures(&adapter_supported_features);
        #ifdef DEBUG_WEBGPU
          logger << "DEBUG: WebGPU adapter features count: " << adapter_supported_features.featureCount;
        #endif // DEBUG_WEBGPU
        for(size_t i{0}; i != adapter_supported_features.featureCount; ++i) {
          adapter_features.emplace(adapter_supported_features.features[i]);
        }
      }
      #ifdef DEBUG_WEBGPU
        for(auto const feature : adapter_features) {
          logger << "DEBUG: WebGPU adapter features: " << enum_name(feature);
        }
      #endif // DEBUG_WEBGPU

      wgpu::Limits adapter_limits;
      bool const result{adapter.GetLimits(&adapter_limits)};
      if(!result) throw std::runtime_error{"WebGPU: Could not query adapter limits"};
      #ifdef DEBUG_WEBGPU
        logger << "DEBUG: WebGPU adapter limits result: " << std::boolalpha << result;
        logger << "DEBUG: WebGPU adapter limits nextInChain: " << adapter_limits.nextInChain;
        logger << "DEBUG: WebGPU adapter limits maxTextureDimension1D: " << adapter_limits.maxTextureDimension1D;
        logger << "DEBUG: WebGPU adapter limits maxTextureDimension2D: " << adapter_limits.maxTextureDimension2D;
        logger << "DEBUG: WebGPU adapter limits maxTextureDimension3D: " << adapter_limits.maxTextureDimension3D;
        logger << "DEBUG: WebGPU adapter limits maxTextureArrayLayers: " << adapter_limits.maxTextureArrayLayers;
        logger << "DEBUG: WebGPU adapter limits maxBindGroups: " << adapter_limits.maxBindGroups;
        logger << "DEBUG: WebGPU adapter limits maxBindGroupsPlusVertexBuffers: " << adapter_limits.maxBindGroupsPlusVertexBuffers;
        logger << "DEBUG: WebGPU adapter limits maxBindingsPerBindGroup: " << adapter_limits.maxBindingsPerBindGroup;
        logger << "DEBUG: WebGPU adapter limits maxDynamicUniformBuffersPerPipelineLayout: " << adapter_limits.maxDynamicUniformBuffersPerPipelineLayout;
        logger << "DEBUG: WebGPU adapter limits maxDynamicStorageBuffersPerPipelineLayout: " << adapter_limits.maxDynamicStorageBuffersPerPipelineLayout;
        logger << "DEBUG: WebGPU adapter limits maxSamplersPerShaderStage: " << adapter_limits.maxSamplersPerShaderStage;
        logger << "DEBUG: WebGPU adapter limits maxStorageBuffersPerShaderStage: " << adapter_limits.maxStorageBuffersPerShaderStage;
        logger << "DEBUG: WebGPU adapter limits maxStorageTexturesPerShaderStage: " << adapter_limits.maxStorageTexturesPerShaderStage;
        logger << "DEBUG: WebGPU adapter limits maxUniformBuffersPerShaderStage: " << adapter_limits.maxUniformBuffersPerShaderStage;
        logger << "DEBUG: WebGPU adapter limits maxUniformBufferBindingSize: " << adapter_limits.maxUniformBufferBindingSize;
        logger << "DEBUG: WebGPU adapter limits maxStorageBufferBindingSize: " << adapter_limits.maxStorageBufferBindingSize;
        logger << "DEBUG: WebGPU adapter limits minUniformBufferOffsetAlignment: " << adapter_limits.minUniformBufferOffsetAlignment;
        logger << "DEBUG: WebGPU adapter limits minStorageBufferOffsetAlignment: " << adapter_limits.minStorageBufferOffsetAlignment;
        logger << "DEBUG: WebGPU adapter limits maxVertexBuffers: " << adapter_limits.maxVertexBuffers;
        logger << "DEBUG: WebGPU adapter limits maxBufferSize: " << adapter_limits.maxBufferSize;
        logger << "DEBUG: WebGPU adapter limits maxVertexAttributes: " << adapter_limits.maxVertexAttributes;
        logger << "DEBUG: WebGPU adapter limits maxVertexBufferArrayStride: " << adapter_limits.maxVertexBufferArrayStride;
        logger << "DEBUG: WebGPU adapter limits maxInterStageShaderVariables: " << adapter_limits.maxInterStageShaderVariables;
        logger << "DEBUG: WebGPU adapter limits maxColorAttachments: " << adapter_limits.maxColorAttachments;
        logger << "DEBUG: WebGPU adapter limits maxColorAttachmentBytesPerSample: " << adapter_limits.maxColorAttachmentBytesPerSample;
        logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupStorageSize: " << adapter_limits.maxComputeWorkgroupStorageSize;
        logger << "DEBUG: WebGPU adapter limits maxComputeInvocationsPerWorkgroup: " << adapter_limits.maxComputeInvocationsPerWorkgroup;
        logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupSizeX: " << adapter_limits.maxComputeWorkgroupSizeX;
        logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupSizeY: " << adapter_limits.maxComputeWorkgroupSizeY;
        logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupSizeZ: " << adapter_limits.maxComputeWorkgroupSizeZ;
        logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupsPerDimension: " << adapter_limits.maxComputeWorkgroupsPerDimension;
      #endif // DEBUG_WEBGPU

      // specify required features for the device
      std::set<wgpu::FeatureName> required_features{
        wgpu::FeatureName::Depth32FloatStencil8,
        #ifndef NDEBUG
          wgpu::FeatureName::TimestampQuery,
        #endif // NDEBUG
        wgpu::FeatureName::TextureCompressionBC,
        wgpu::FeatureName::IndirectFirstInstance,
      };
      std::set<wgpu::FeatureName> desired_features{
        wgpu::FeatureName::ShaderF16,
        wgpu::FeatureName::Float32Filterable,
      };

      std::vector<wgpu::FeatureName> required_features_arr;
      for(auto const feature : required_features) {
        if(!adapter_features.contains(feature)) {
          logger << "WebGPU: Required adapter feature " << magic_enum::enum_name(feature) << " unavailable, cannot continue";
          throw std::runtime_error{"WebGPU: Required adapter feature " + std::string{magic_enum::enum_name(feature)} + " not available"};
        }
        logger << "WebGPU: Required adapter feature: " << magic_enum::enum_name(feature) << " requested";
        required_features_arr.emplace_back(feature);
      }
      for(auto const feature : desired_features) {
        if(!adapter_features.contains(feature)) {
          logger << "WebGPU: Desired adapter feature " << magic_enum::enum_name(feature) << " unavailable, continuing without it";
          continue;
        }
        logger << "WebGPU: Desired adapter feature " << magic_enum::enum_name(feature) << " requested";
        required_features_arr.emplace_back(feature);
      }

      // request a device
      wgpu::DeviceDescriptor device_descriptor;
      device_descriptor.requiredFeatureCount = required_features_arr.size();
      device_descriptor.requiredFeatures = required_features_arr.data();
      // The browser-facing requestDevice() path now validates requiredLimits directly.
      // We currently don't need any limits above the WebGPU defaults, so don't request
      // the stale sample values that were previously here.
      device_descriptor.requiredLimits = nullptr;
      device_descriptor.defaultQueue.label = "Default queue";
      device_descriptor.SetDeviceLostCallback(
        wgpu::CallbackMode::AllowSpontaneous,
        [](wgpu::Device const &, wgpu::DeviceLostReason reason, wgpu::StringView message, context *webgpu_ptr){
          /// Device lost callback
          auto &webgpu{*webgpu_ptr};
          auto &logger{webgpu.logger};
          logger << "ERROR: WebGPU lost device, reason " << enum_name<wgpu::DeviceLostReason>(reason) << ": " << dawn_to_string_view(message);
          webgpu.state = states::failed;
        },
        &webgpu
      );
      device_descriptor.SetUncapturedErrorCallback(
        [](wgpu::Device const &, wgpu::ErrorType type, wgpu::StringView message, context *webgpu_ptr){
          /// Uncaptured error callback
          auto &webgpu{*webgpu_ptr};
          auto &logger{webgpu.logger};
          logger << "ERROR: WebGPU uncaptured error " << enum_name<wgpu::ErrorType>(type) << ": " << dawn_to_string_view(message);
          webgpu.state = states::failed;
        },
        &webgpu
      );

      adapter.RequestDevice(
        &device_descriptor,
        wgpu::CallbackMode::AllowSpontaneous,
        [](wgpu::RequestDeviceStatus status, wgpu::Device device_in, wgpu::StringView message, context *webgpu_ptr){
          /// Request device callback
          auto &webgpu{*webgpu_ptr};
          auto &logger{webgpu.logger};
          if(message.length) logger << "WebGPU: Request device callback message: " << dawn_to_string_view(message);
          if(status != wgpu::RequestDeviceStatus::Success) {
            logger << "ERROR: WebGPU device request failure, status " << enum_name<wgpu::RequestDeviceStatus>(status);
            throw std::runtime_error{"WebGPU: Could not get adapter"};
          }
          auto &device{webgpu.device};
          device = std::move(device_in);

          // report device capabilities
          std::set<wgpu::FeatureName> device_features;
          {
            wgpu::SupportedFeatures device_supported_features;
            device.GetFeatures(&device_supported_features);
            #ifdef DEBUG_WEBGPU
              logger << "DEBUG: WebGPU device features count: " << device_supported_features.featureCount;
            #endif // DEBUG_WEBGPU
            for(size_t i{0}; i != device_supported_features.featureCount; ++i) {
              device_features.emplace(device_supported_features.features[i]);
            }
          }
          #ifdef DEBUG_WEBGPU
            for(auto const feature : device_features) {
              logger << "DEBUG: WebGPU device features: " << magic_enum::enum_name(feature);
            }
            {
              wgpu::Limits device_limits;
              bool const result{device.GetLimits(&device_limits)};
              logger << "DEBUG: WebGPU device limits result: " << std::boolalpha << result;
              logger << "DEBUG: WebGPU device limits nextInChain: " << device_limits.nextInChain;
              logger << "DEBUG: WebGPU device limits maxTextureDimension1D: " << device_limits.maxTextureDimension1D;
              logger << "DEBUG: WebGPU device limits maxTextureDimension2D: " << device_limits.maxTextureDimension2D;
              logger << "DEBUG: WebGPU device limits maxTextureDimension3D: " << device_limits.maxTextureDimension3D;
              logger << "DEBUG: WebGPU device limits maxTextureArrayLayers: " << device_limits.maxTextureArrayLayers;
              logger << "DEBUG: WebGPU device limits maxBindGroups: " << device_limits.maxBindGroups;
              logger << "DEBUG: WebGPU device limits maxBindGroupsPlusVertexBuffers: " << device_limits.maxBindGroupsPlusVertexBuffers;
              logger << "DEBUG: WebGPU device limits maxBindingsPerBindGroup: " << device_limits.maxBindingsPerBindGroup;
              logger << "DEBUG: WebGPU device limits maxDynamicUniformBuffersPerPipelineLayout: " << device_limits.maxDynamicUniformBuffersPerPipelineLayout;
              logger << "DEBUG: WebGPU device limits maxDynamicStorageBuffersPerPipelineLayout: " << device_limits.maxDynamicStorageBuffersPerPipelineLayout;
              logger << "DEBUG: WebGPU device limits maxSamplersPerShaderStage: " << device_limits.maxSamplersPerShaderStage;
              logger << "DEBUG: WebGPU device limits maxStorageBuffersPerShaderStage: " << device_limits.maxStorageBuffersPerShaderStage;
              logger << "DEBUG: WebGPU device limits maxStorageTexturesPerShaderStage: " << device_limits.maxStorageTexturesPerShaderStage;
              logger << "DEBUG: WebGPU device limits maxUniformBuffersPerShaderStage: " << device_limits.maxUniformBuffersPerShaderStage;
              logger << "DEBUG: WebGPU device limits maxUniformBufferBindingSize: " << device_limits.maxUniformBufferBindingSize;
              logger << "DEBUG: WebGPU device limits maxStorageBufferBindingSize: " << device_limits.maxStorageBufferBindingSize;
              logger << "DEBUG: WebGPU device limits minUniformBufferOffsetAlignment: " << device_limits.minUniformBufferOffsetAlignment;
              logger << "DEBUG: WebGPU device limits minStorageBufferOffsetAlignment: " << device_limits.minStorageBufferOffsetAlignment;
              logger << "DEBUG: WebGPU device limits maxVertexBuffers: " << device_limits.maxVertexBuffers;
              logger << "DEBUG: WebGPU device limits maxBufferSize: " << device_limits.maxBufferSize;
              logger << "DEBUG: WebGPU device limits maxVertexAttributes: " << device_limits.maxVertexAttributes;
              logger << "DEBUG: WebGPU device limits maxVertexBufferArrayStride: " << device_limits.maxVertexBufferArrayStride;
              logger << "DEBUG: WebGPU device limits maxInterStageShaderVariables: " << device_limits.maxInterStageShaderVariables;
              logger << "DEBUG: WebGPU device limits maxColorAttachments: " << device_limits.maxColorAttachments;
              logger << "DEBUG: WebGPU device limits maxColorAttachmentBytesPerSample: " << device_limits.maxColorAttachmentBytesPerSample;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupStorageSize: " << device_limits.maxComputeWorkgroupStorageSize;
              logger << "DEBUG: WebGPU device limits maxComputeInvocationsPerWorkgroup: " << device_limits.maxComputeInvocationsPerWorkgroup;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupSizeX: " << device_limits.maxComputeWorkgroupSizeX;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupSizeY: " << device_limits.maxComputeWorkgroupSizeY;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupSizeZ: " << device_limits.maxComputeWorkgroupSizeZ;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupsPerDimension: " << device_limits.maxComputeWorkgroupsPerDimension;
            }
          #endif // DEBUG_WEBGPU

          logger << "WebGPU acquiring queue";
          webgpu.queue = device.GetQueue();
          webgpu.state = states::ready_to_configure;
        },
        &webgpu
      );
    },
    this
  );
}

}
