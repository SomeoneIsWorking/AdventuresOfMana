#include "host/gpu_asset_pipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <stdexcept>

#include <lucent/log.h>

#include "host/gpu_asset.h"
#include "host/gpu_device.h"
#include "host/gpu_shader.h"
#include "host/render_asset.h"
#include "mcf/mcf.h"

namespace mana::gpu {
namespace {

SDL_GPUVertexElementFormat
AttributeFormat(const mcf::VertexAttribute &attribute) {
  switch (attribute.type) {
  case mcf::VertexType::kFloat2:
    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
  case mcf::VertexType::kFloat3:
    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  case mcf::VertexType::kFloat4:
    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
  case mcf::VertexType::kUByte4Color:
    return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
  case mcf::VertexType::kUByte4Index:
    return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4;
  }
  throw std::invalid_argument("unsupported model vertex attribute type");
}

const mcf::VertexAttribute &
RequireAttribute(const Asset &asset, mcf::VertexUsage usage, const char *name) {
  const auto &layout = asset.layout();
  const auto found = std::find_if(
      layout.begin(), layout.end(),
      [usage](const auto &attribute) { return attribute.usage == usage; });
  if (found == layout.end())
    throw std::invalid_argument(std::format("model has no {} attribute", name));
  return *found;
}

} // namespace

AssetPipeline::AssetPipeline(Device &device, const Asset &asset)
    : device_(device), asset_(asset) {
  const auto &position =
      RequireAttribute(asset_, mcf::VertexUsage::kPosition, "position");
  const auto &color =
      RequireAttribute(asset_, mcf::VertexUsage::kColor, "color");
  const auto &texcoord =
      RequireAttribute(asset_, mcf::VertexUsage::kTexcoord0, "texcoord0");
  const SDL_GPUVertexBufferDescription vertex_buffer{
      .slot = 0,
      .pitch = asset_.vertex_stride(),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
  };
  const std::array<SDL_GPUVertexAttribute, 3> attributes{{
      {.location = 0,
       .buffer_slot = 0,
       .format = AttributeFormat(position),
       .offset = position.offset},
      {.location = 1,
       .buffer_slot = 0,
       .format = AttributeFormat(color),
       .offset = color.offset},
      {.location = 2,
       .buffer_slot = 0,
       .format = AttributeFormat(texcoord),
       .offset = texcoord.offset},
  }};
  SDL_GPUShader *vertex =
      CreateShader(device_, "textured", SDL_GPU_SHADERSTAGE_VERTEX,
                   ShaderResources{.uniform_buffers = 1});
  SDL_GPUShader *fragment = nullptr;
  try {
    fragment = CreateShader(device_, "textured", SDL_GPU_SHADERSTAGE_FRAGMENT,
                            ShaderResources{.samplers = 1});
    const SDL_GPUColorTargetDescription color_target{
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    };
    const SDL_GPUGraphicsPipelineCreateInfo info{
        .vertex_shader = vertex,
        .fragment_shader = fragment,
        .vertex_input_state =
            {
                .vertex_buffer_descriptions = &vertex_buffer,
                .num_vertex_buffers = 1,
                .vertex_attributes = attributes.data(),
                .num_vertex_attributes = attributes.size(),
            },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                             .cull_mode = SDL_GPU_CULLMODE_NONE,
                             .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
                             .enable_depth_clip = true},
        .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
        .target_info = {.color_target_descriptions = &color_target,
                        .num_color_targets = 1},
    };
    pipeline_ = SDL_CreateGPUGraphicsPipeline(device_.native_handle(), &info);
    if (!pipeline_)
      throw std::runtime_error(std::format(
          "SDL_CreateGPUGraphicsPipeline(textured): {}", SDL_GetError()));
  } catch (...) {
    if (fragment)
      SDL_ReleaseGPUShader(device_.native_handle(), fragment);
    SDL_ReleaseGPUShader(device_.native_handle(), vertex);
    throw;
  }
  SDL_ReleaseGPUShader(device_.native_handle(), fragment);
  SDL_ReleaseGPUShader(device_.native_handle(), vertex);
}

AssetPipeline::~AssetPipeline() {
  if (pipeline_)
    SDL_ReleaseGPUGraphicsPipeline(device_.native_handle(), pipeline_);
}

std::array<float, 16> AssetPipeline::FitTopDown() const {
  const float width = std::max(asset_.hi()[0] - asset_.lo()[0], 1e-6f);
  const float height = std::max(asset_.hi()[2] - asset_.lo()[2], 1e-6f);
  const float depth = std::max(asset_.hi()[1] - asset_.lo()[1], 1e-6f);
  const float sx = 1.8f / width;
  const float sy = 1.8f / height;
  const float sz = 0.8f / depth;
  const float tx = -(asset_.hi()[0] + asset_.lo()[0]) * sx * .5f;
  const float ty = -(asset_.hi()[2] + asset_.lo()[2]) * sy * .5f;
  const float tz = -asset_.lo()[1] * sz + .1f;
  return {sx,  0.f, 0.f, 0.f, 0.f, 0.f, sz, 0.f,
          0.f, sy,  0.f, 0.f, tx,  ty,  tz, 1.f};
}

std::vector<std::uint8_t> AssetPipeline::DrawAndReadback(std::uint32_t width,
                                                         std::uint32_t height,
                                                         bool draw,
                                                         bool textures) {
  constexpr SDL_FColor clear{0.f, 1.f, 1.f, 1.f};
  const auto transform = FitTopDown();
  return device_.RenderAndReadback(
      width, height, clear,
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        if (!draw)
          return;
        SDL_PushGPUVertexUniformData(command, 0, transform.data(),
                                     sizeof(transform));
        SDL_BindGPUGraphicsPipeline(pass, pipeline_);
        const SDL_GPUBufferBinding vertices{.buffer = asset_.vertices()};
        const SDL_GPUBufferBinding indices{.buffer = asset_.indices()};
        SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
        SDL_BindGPUIndexBuffer(pass, &indices, asset_.index_type());
        for (std::size_t i = 0; i < asset_.draws().size(); ++i) {
          const SDL_GPUTextureSamplerBinding binding{
              .texture = asset_.TextureForDraw(i, textures),
              .sampler = asset_.sampler(),
          };
          SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
          const auto &range = asset_.draws()[i];
          const std::uint32_t index_bytes =
              asset_.index_type() == SDL_GPU_INDEXELEMENTSIZE_16BIT ? 2 : 4;
          SDL_DrawGPUIndexedPrimitives(pass, range.index_count, 1,
                                       range.byte_offset / index_bytes, 0, 0);
        }
      });
}

int RunAssetPipelineSelfTest(const char *archive_path, bool negative_control) {
  mcf::Archive archive(archive_path);
  mcf::RenderAsset source;
  constexpr std::string_view name = "M0001_00_00";
  if (!mcf::LoadRenderAsset(archive, std::string(name), &source)) {
    lucent::error(
        "gpu", "ASSET SELFTEST FAIL: scanned archive for {}, matched 0", name);
    return 1;
  }
  Device device;
  Asset asset(device, source);
  AssetPipeline pipeline(device, asset);
  constexpr std::uint32_t width = 96;
  constexpr std::uint32_t height = 72;
  const auto pixels =
      pipeline.DrawAndReadback(width, height, !negative_control);
  const auto white_pixels =
      negative_control ? std::vector<std::uint8_t>{}
                       : pipeline.DrawAndReadback(width, height, true, false);
  constexpr std::array<std::uint8_t, 4> clear{0, 255, 255, 255};
  std::uint32_t changed = 0;
  std::array<bool, 256> red_values{};
  std::uint32_t distinct_red = 0;
  std::uint32_t texture_changed = 0;
  for (std::uint32_t i = 0; i < width * height; ++i) {
    const auto *pixel = pixels.data() + i * 4;
    if (!negative_control &&
        std::memcmp(pixel, white_pixels.data() + i * 4, 4) != 0)
      ++texture_changed;
    if (std::memcmp(pixel, clear.data(), 4) != 0) {
      ++changed;
      if (!red_values[pixel[0]]) {
        red_values[pixel[0]] = true;
        ++distinct_red;
      }
    }
  }
  const bool pass = negative_control ? changed == 0
                                     : changed > 0 && distinct_red >= 2 &&
                                           texture_changed > 0;
  if (!pass) {
    lucent::error(
        "gpu",
        "ASSET SELFTEST FAIL: scanned {} pixels from {} draws; {} "
        "changed from clear, {} distinct changed red values, {} differ from "
        "forced-white; expected {} changed class, at least {} red classes, "
        "and {} texture differences",
        width * height, asset.draws().size(), changed, distinct_red,
        texture_changed, negative_control ? "zero" : "nonzero",
        negative_control ? 0 : 2, negative_control ? 0 : 1);
    return 1;
  }
  lucent::info(
      "gpu",
      "ASSET SELFTEST: loaded {}; uploaded {} draws; scanned {} pixels; "
      "{} changed from clear; {} distinct changed red values; {} differ from "
      "forced-white",
      name, asset.draws().size(), width * height, changed, distinct_red,
      texture_changed);
  return 0;
}

} // namespace mana::gpu
