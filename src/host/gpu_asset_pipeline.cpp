#include "host/gpu_asset_pipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <span>
#include <stdexcept>
#include <vector>

#include "host/gpu_asset.h"
#include "host/gpu_device.h"
#include "host/gpu_shader.h"

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

SDL_GPUGraphicsPipeline *
CreateGraphicsPipeline(Device &device, SDL_GPUShader *vertex,
                       SDL_GPUShader *fragment,
                       const SDL_GPUVertexBufferDescription &vertex_buffer,
                       std::span<const SDL_GPUVertexAttribute> attributes,
                       PipelineFeatures features, bool blended) {
  const SDL_GPUColorTargetDescription color_target{
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .blend_state =
          {
              .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
              .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
              .color_blend_op = SDL_GPU_BLENDOP_ADD,
              .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
              .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
              .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
              .enable_blend = blended && features.material_blending,
          },
  };
  const SDL_GPUGraphicsPipelineCreateInfo info{
      .vertex_shader = vertex,
      .fragment_shader = fragment,
      .vertex_input_state =
          {
              .vertex_buffer_descriptions = &vertex_buffer,
              .num_vertex_buffers = 1,
              .vertex_attributes = attributes.data(),
              .num_vertex_attributes =
                  static_cast<std::uint32_t>(attributes.size()),
          },
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                           .cull_mode = SDL_GPU_CULLMODE_NONE,
                           .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
                           .enable_depth_clip = true},
      .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
      .depth_stencil_state =
          {
              .compare_op = SDL_GPU_COMPAREOP_LESS,
              .enable_depth_test = features.depth_test,
              .enable_depth_write = features.depth_test &&
                                    !(blended && features.material_blending),
          },
      .target_info =
          {
              .color_target_descriptions = &color_target,
              .num_color_targets = 1,
              .depth_stencil_format = features.depth_test
                                          ? device.depth_format()
                                          : SDL_GPU_TEXTUREFORMAT_INVALID,
              .has_depth_stencil_target = features.depth_test,
          },
  };
  SDL_GPUGraphicsPipeline *pipeline =
      SDL_CreateGPUGraphicsPipeline(device.native_handle(), &info);
  if (!pipeline)
    throw std::runtime_error(
        std::format("SDL_CreateGPUGraphicsPipeline(textured {}): {}",
                    blended ? "blend" : "opaque", SDL_GetError()));
  return pipeline;
}

} // namespace

AssetPipeline::AssetPipeline(Device &device, const Asset &asset,
                             PipelineFeatures features)
    : device_(device), asset_(asset), features_(features) {
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
  std::vector<SDL_GPUVertexAttribute> attributes{{
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
  if (asset_.skinned()) {
    const auto &weight =
        RequireAttribute(asset_, mcf::VertexUsage::kWeight, "weight");
    const auto &incidence =
        RequireAttribute(asset_, mcf::VertexUsage::kIncidence, "incidence");
    attributes.push_back({.location = 3,
                          .buffer_slot = 0,
                          .format = AttributeFormat(weight),
                          .offset = weight.offset});
    attributes.push_back({.location = 4,
                          .buffer_slot = 0,
                          .format = AttributeFormat(incidence),
                          .offset = incidence.offset});
  }
  // Shadercross compacts the static shader's unused TEXCOORD3/4 semantics, so
  // its normal is location 3. The skinned shader consumes both and retains 5.
  attributes.push_back({.location = asset_.skinned() ? 5u : 3u,
                        .buffer_slot = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                        .offset = asset_.normal_offset()});
  SDL_GPUShader *vertex = CreateShader(
      device_, asset_.skinned() ? "skinned" : "textured",
      SDL_GPU_SHADERSTAGE_VERTEX, ShaderResources{.uniform_buffers = 1});
  SDL_GPUShader *fragment = nullptr;
  try {
    fragment = CreateShader(device_, "textured", SDL_GPU_SHADERSTAGE_FRAGMENT,
                            ShaderResources{.samplers = 1});
    opaque_pipeline_ = CreateGraphicsPipeline(
        device_, vertex, fragment, vertex_buffer, attributes, features_, false);
    blend_pipeline_ = CreateGraphicsPipeline(
        device_, vertex, fragment, vertex_buffer, attributes, features_, true);
  } catch (...) {
    if (opaque_pipeline_)
      SDL_ReleaseGPUGraphicsPipeline(device_.native_handle(), opaque_pipeline_);
    if (fragment)
      SDL_ReleaseGPUShader(device_.native_handle(), fragment);
    SDL_ReleaseGPUShader(device_.native_handle(), vertex);
    throw;
  }
  SDL_ReleaseGPUShader(device_.native_handle(), fragment);
  SDL_ReleaseGPUShader(device_.native_handle(), vertex);
}

AssetPipeline::~AssetPipeline() {
  if (blend_pipeline_)
    SDL_ReleaseGPUGraphicsPipeline(device_.native_handle(), blend_pipeline_);
  if (opaque_pipeline_)
    SDL_ReleaseGPUGraphicsPipeline(device_.native_handle(), opaque_pipeline_);
}

std::array<float, 16> AssetPipeline::TopDownTransform() const {
  const float width = std::max(asset_.hi()[0] - asset_.lo()[0], 1e-6f);
  const float height = std::max(asset_.hi()[2] - asset_.lo()[2], 1e-6f);
  const float depth = std::max(asset_.hi()[1] - asset_.lo()[1], 1e-6f);
  const float sx = 1.8f / width;
  const float sy = 1.8f / height;
  const float sz = -0.8f / depth;
  const float tx = -(asset_.hi()[0] + asset_.lo()[0]) * sx * .5f;
  const float ty = -(asset_.hi()[2] + asset_.lo()[2]) * sy * .5f;
  const float tz = -asset_.hi()[1] * sz + .1f;
  return {sx,  0.f, 0.f, 0.f, 0.f, 0.f, sz, 0.f,
          0.f, sy,  0.f, 0.f, tx,  ty,  tz, 1.f};
}

std::vector<std::uint8_t> AssetPipeline::DrawAndReadback(std::uint32_t width,
                                                         std::uint32_t height,
                                                         bool draw,
                                                         bool textures) {
  constexpr SDL_FColor clear{0.f, 1.f, 1.f, 1.f};
  const auto transform = TopDownTransform();
  if (!draw)
    return device_.RenderAndReadback(width, height, clear, {},
                                     features_.depth_test);
  return DrawAndReadback(width, height, transform, textures);
}

std::vector<std::uint8_t>
AssetPipeline::DrawAndReadback(std::uint32_t width, std::uint32_t height,
                               const std::array<float, 16> &transform,
                               bool textures, std::span<const float> joints,
                               const DirectionalLight &light) {
  constexpr SDL_FColor clear{0.f, 1.f, 1.f, 1.f};
  return device_.RenderAndReadback(
      width, height, clear,
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        Draw(command, pass, transform, textures, joints, light);
      },
      features_.depth_test);
}

void AssetPipeline::Draw(SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass,
                         const std::array<float, 16> &transform, bool textures,
                         std::span<const float> joints,
                         const DirectionalLight &light) {
  DrawPass(command, pass, MaterialPass::kOpaque, transform, textures, joints,
           light);
  DrawPass(command, pass, MaterialPass::kBlended, transform, textures, joints,
           light);
}

void AssetPipeline::DrawPass(SDL_GPUCommandBuffer *command,
                             SDL_GPURenderPass *pass,
                             MaterialPass material_pass,
                             const std::array<float, 16> &transform,
                             bool textures, std::span<const float> joints,
                             const DirectionalLight &light) {
  constexpr std::size_t kJointFloatCount = 80 * 3 * 4;
  const std::array<float, 8> light_uniform{light.direction_to_light[0],
                                           light.direction_to_light[1],
                                           light.direction_to_light[2],
                                           light.ambient,
                                           light.color[0],
                                           light.color[1],
                                           light.color[2],
                                           light.diffuse};
  if (asset_.skinned()) {
    if (joints.size() != kJointFloatCount)
      throw std::invalid_argument(
          std::format("skinned asset requires {} joint floats, received {}",
                      kJointFloatCount, joints.size()));
    std::array<float, 16 + kJointFloatCount + 8> uniform{};
    std::copy(transform.begin(), transform.end(), uniform.begin());
    std::copy(joints.begin(), joints.end(), uniform.begin() + 16);
    std::copy(light_uniform.begin(), light_uniform.end(),
              uniform.begin() + 16 + kJointFloatCount);
    SDL_PushGPUVertexUniformData(command, 0, uniform.data(), sizeof(uniform));
  } else {
    if (!joints.empty())
      throw std::invalid_argument("static asset received a joint palette");
    std::array<float, 24> uniform{};
    std::copy(transform.begin(), transform.end(), uniform.begin());
    std::copy(light_uniform.begin(), light_uniform.end(), uniform.begin() + 16);
    SDL_PushGPUVertexUniformData(command, 0, uniform.data(), sizeof(uniform));
  }
  const SDL_GPUBufferBinding vertices{.buffer = asset_.vertices()};
  const SDL_GPUBufferBinding indices{.buffer = asset_.indices()};
  SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
  SDL_BindGPUIndexBuffer(pass, &indices, asset_.index_type());
  const std::uint32_t index_bytes =
      asset_.index_type() == SDL_GPU_INDEXELEMENTSIZE_16BIT ? 2 : 4;
  const bool blended = material_pass == MaterialPass::kBlended;
  SDL_BindGPUGraphicsPipeline(pass,
                              blended ? blend_pipeline_ : opaque_pipeline_);
  for (std::size_t i = 0; i < asset_.draws().size(); ++i) {
    if (asset_.DrawBlended(i) != blended)
      continue;
    const SDL_GPUTextureSamplerBinding binding{
        .texture = asset_.TextureForDraw(i, textures),
        .sampler = asset_.sampler(),
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    const auto &range = asset_.draws()[i];
    SDL_DrawGPUIndexedPrimitives(pass, range.index_count, 1,
                                 range.byte_offset / index_bytes, 0, 0);
  }
}

} // namespace mana::gpu
