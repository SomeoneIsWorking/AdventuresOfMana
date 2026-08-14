#include "host/gpu_sprite.h"

#include <format>
#include <span>
#include <stdexcept>

#include "host/gpu_device.h"
#include "host/gpu_shader.h"
#include "host/gpu_upload.h"
#include "host/render_sprite.h"

namespace mana::gpu {

SpriteRenderer::SpriteRenderer(Device &device, const SpriteImage &image,
                               std::uint32_t viewport_width,
                               std::uint32_t viewport_height)
    : device_(device) {
  const auto vertices =
      BuildAspectFitSprite(image, viewport_width, viewport_height);
  texture_ = UploadTexture(device_, image.width, image.height,
                           SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 4, image.rgba);
  try {
    const SDL_GPUSamplerCreateInfo sampler_info{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    sampler_ = SDL_CreateGPUSampler(device_.native_handle(), &sampler_info);
    if (!sampler_)
      throw std::runtime_error(
          std::format("SDL_CreateGPUSampler(sprite): {}", SDL_GetError()));
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t *>(vertices.data()),
        vertices.size() * sizeof(UiVertex));
    vertices_ = UploadBuffer(device_, bytes, SDL_GPU_BUFFERUSAGE_VERTEX);

    SDL_GPUShader *vertex =
        CreateShader(device_, "sprite", SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader *fragment = nullptr;
    try {
      fragment = CreateShader(device_, "sprite", SDL_GPU_SHADERSTAGE_FRAGMENT,
                              {.samplers = 1});
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
      const SDL_GPUColorTargetDescription target{
          .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
          .blend_state = {.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                          .dst_color_blendfactor =
                              SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                          .color_blend_op = SDL_GPU_BLENDOP_ADD,
                          .src_alpha_blendfactor =
                              SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                          .dst_alpha_blendfactor =
                              SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                          .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                          .enable_blend = true},
      };
      const SDL_GPUGraphicsPipelineCreateInfo info{
          .vertex_shader = vertex,
          .fragment_shader = fragment,
          .vertex_input_state = {.vertex_buffer_descriptions = &vertex_buffer,
                                 .num_vertex_buffers = 1,
                                 .vertex_attributes = attributes,
                                 .num_vertex_attributes = 2},
          .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
          .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                               .cull_mode = SDL_GPU_CULLMODE_NONE,
                               .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
                               .enable_depth_clip = true},
          .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
          .depth_stencil_state = {.enable_depth_test = false,
                                  .enable_depth_write = false},
          .target_info = {.color_target_descriptions = &target,
                          .num_color_targets = 1,
                          .depth_stencil_format = device_.depth_format(),
                          .has_depth_stencil_target = true},
      };
      pipeline_ = SDL_CreateGPUGraphicsPipeline(device_.native_handle(), &info);
      if (!pipeline_)
        throw std::runtime_error(std::format(
            "SDL_CreateGPUGraphicsPipeline(sprite): {}", SDL_GetError()));
    } catch (...) {
      if (fragment)
        SDL_ReleaseGPUShader(device_.native_handle(), fragment);
      SDL_ReleaseGPUShader(device_.native_handle(), vertex);
      throw;
    }
    SDL_ReleaseGPUShader(device_.native_handle(), fragment);
    SDL_ReleaseGPUShader(device_.native_handle(), vertex);
  } catch (...) {
    if (vertices_)
      SDL_ReleaseGPUBuffer(device_.native_handle(), vertices_);
    if (sampler_)
      SDL_ReleaseGPUSampler(device_.native_handle(), sampler_);
    SDL_ReleaseGPUTexture(device_.native_handle(), texture_);
    throw;
  }
}

SpriteRenderer::~SpriteRenderer() {
  if (pipeline_)
    SDL_ReleaseGPUGraphicsPipeline(device_.native_handle(), pipeline_);
  if (vertices_)
    SDL_ReleaseGPUBuffer(device_.native_handle(), vertices_);
  if (sampler_)
    SDL_ReleaseGPUSampler(device_.native_handle(), sampler_);
  if (texture_)
    SDL_ReleaseGPUTexture(device_.native_handle(), texture_);
}

void SpriteRenderer::Draw(SDL_GPUCommandBuffer *command,
                          SDL_GPURenderPass *pass) {
  if (!command || !pass)
    throw std::invalid_argument("sprite draw requires command buffer and pass");
  SDL_BindGPUGraphicsPipeline(pass, pipeline_);
  const SDL_GPUBufferBinding vertices{.buffer = vertices_};
  SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
  const SDL_GPUTextureSamplerBinding texture{.texture = texture_,
                                             .sampler = sampler_};
  SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
  SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
}

} // namespace mana::gpu
