#include "host/gpu_ui.h"

#include <cstring>
#include <format>
#include <span>
#include <stdexcept>

#include "host/gpu_device.h"
#include "host/gpu_shader.h"
#include "host/gpu_upload.h"
#include "mcf/mcf.h"

namespace mana::gpu {

UiRenderer::UiRenderer(Device &device, const mcf::Font &font)
    : device_(device) {
  if (!font.width() || !font.height() ||
      font.atlas().size() != std::size_t(font.width()) * font.height())
    throw std::invalid_argument("SDL3 UI renderer requires a complete font atlas");
  atlas_ = UploadTexture(device_, font.width(), font.height(),
                         SDL_GPU_TEXTUREFORMAT_R8_UNORM, 1, font.atlas());
  try {
    const SDL_GPUSamplerCreateInfo sampler_info{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .max_lod = 0.f,
    };
    sampler_ = SDL_CreateGPUSampler(device_.native_handle(), &sampler_info);
    if (!sampler_)
      throw std::runtime_error(
          std::format("SDL_CreateGPUSampler(ui): {}", SDL_GetError()));

    SDL_GPUShader *vertex =
        CreateShader(device_, "ui", SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader *fragment = nullptr;
    try {
      fragment = CreateShader(
          device_, "ui", SDL_GPU_SHADERSTAGE_FRAGMENT,
          ShaderResources{.samplers = 1, .uniform_buffers = 1});
      const SDL_GPUVertexBufferDescription vertex_buffer{
          .slot = 0,
          .pitch = sizeof(UiVertex),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      };
      const SDL_GPUVertexAttribute attributes[]{
          {.location = 0,
           .buffer_slot = 0,
           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
           .offset = offsetof(UiVertex, position)},
          {.location = 1,
           .buffer_slot = 0,
           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
           .offset = offsetof(UiVertex, uv)},
      };
      const SDL_GPUColorTargetDescription color_target{
          .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
          .blend_state =
              {
                  .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                  .dst_color_blendfactor =
                      SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                  .color_blend_op = SDL_GPU_BLENDOP_ADD,
                  .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                  .dst_alpha_blendfactor =
                      SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                  .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                  .enable_blend = true,
              },
      };
      const SDL_GPUGraphicsPipelineCreateInfo info{
          .vertex_shader = vertex,
          .fragment_shader = fragment,
          .vertex_input_state =
              {
                  .vertex_buffer_descriptions = &vertex_buffer,
                  .num_vertex_buffers = 1,
                  .vertex_attributes = attributes,
                  .num_vertex_attributes = 2,
              },
          .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
          .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                               .cull_mode = SDL_GPU_CULLMODE_NONE,
                               .front_face =
                                   SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
                               .enable_depth_clip = true},
          .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
          .depth_stencil_state = {.enable_depth_test = false,
                                  .enable_depth_write = false},
          .target_info =
              {
                  .color_target_descriptions = &color_target,
                  .num_color_targets = 1,
                  .depth_stencil_format = device_.depth_format(),
                  .has_depth_stencil_target = true,
              },
      };
      pipeline_ = SDL_CreateGPUGraphicsPipeline(device_.native_handle(), &info);
      if (!pipeline_)
        throw std::runtime_error(std::format(
            "SDL_CreateGPUGraphicsPipeline(ui): {}", SDL_GetError()));
    } catch (...) {
      if (fragment)
        SDL_ReleaseGPUShader(device_.native_handle(), fragment);
      SDL_ReleaseGPUShader(device_.native_handle(), vertex);
      throw;
    }
    SDL_ReleaseGPUShader(device_.native_handle(), fragment);
    SDL_ReleaseGPUShader(device_.native_handle(), vertex);
  } catch (...) {
    if (sampler_)
      SDL_ReleaseGPUSampler(device_.native_handle(), sampler_);
    SDL_ReleaseGPUTexture(device_.native_handle(), atlas_);
    sampler_ = nullptr;
    atlas_ = nullptr;
    throw;
  }
}

UiRenderer::~UiRenderer() {
  if (vertices_)
    SDL_ReleaseGPUBuffer(device_.native_handle(), vertices_);
  if (pipeline_)
    SDL_ReleaseGPUGraphicsPipeline(device_.native_handle(), pipeline_);
  if (sampler_)
    SDL_ReleaseGPUSampler(device_.native_handle(), sampler_);
  if (atlas_)
    SDL_ReleaseGPUTexture(device_.native_handle(), atlas_);
}

void UiRenderer::Prepare(const UiFrame &frame) {
  for (const auto &batch : frame.batches) {
    if (batch.vertex_count == 0 || batch.vertex_count % 3 != 0 ||
        std::uint64_t(batch.first_vertex) + batch.vertex_count >
            frame.vertices.size())
      throw std::invalid_argument("UI batch references invalid triangle vertices");
  }
  if (vertices_) {
    SDL_ReleaseGPUBuffer(device_.native_handle(), vertices_);
    vertices_ = nullptr;
  }
  batches_ = frame.batches;
  vertex_count_ = static_cast<std::uint32_t>(frame.vertices.size());
  if (frame.vertices.empty())
    return;
  const auto bytes = std::span(
      reinterpret_cast<const std::uint8_t *>(frame.vertices.data()),
      frame.vertices.size() * sizeof(UiVertex));
  vertices_ = UploadBuffer(device_, bytes, SDL_GPU_BUFFERUSAGE_VERTEX);
}

void UiRenderer::Draw(SDL_GPUCommandBuffer *command,
                      SDL_GPURenderPass *pass) {
  if (!command)
    throw std::invalid_argument("UI draw has no command buffer");
  if (!pass)
    throw std::invalid_argument("UI draw has no render pass");
  if (!vertex_count_)
    return;
  if (!vertices_)
    throw std::logic_error("UI draw has vertices but no prepared GPU buffer");
  SDL_BindGPUGraphicsPipeline(pass, pipeline_);
  const SDL_GPUBufferBinding vertices{.buffer = vertices_};
  SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
  const SDL_GPUTextureSamplerBinding atlas{.texture = atlas_,
                                           .sampler = sampler_};
  SDL_BindGPUFragmentSamplers(pass, 0, &atlas, 1);
  for (const auto &batch : batches_) {
    const std::array<float, 8> style{
        batch.color[0], batch.color[1], batch.color[2], batch.color[3],
        batch.textured ? 1.f : 0.f, 0.f, 0.f, 0.f};
    SDL_PushGPUFragmentUniformData(command, 0, style.data(), sizeof(style));
    SDL_DrawGPUPrimitives(pass, batch.vertex_count, 1, batch.first_vertex, 0);
  }
}

std::vector<std::uint8_t>
UiRenderer::DrawAndReadback(std::uint32_t width, std::uint32_t height,
                            const UiFrame &frame) {
  Prepare(frame);
  return device_.RenderAndReadback(
      width, height, SDL_FColor{.2f, .3f, .4f, 1.f},
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        Draw(command, pass);
      },
      true);
}

} // namespace mana::gpu
