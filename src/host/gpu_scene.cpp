#include "host/gpu_scene.h"

#include <cmath>
#include <stdexcept>

#include "host/gpu_asset_pipeline.h"
#include "host/gpu_device.h"

namespace mana::gpu {

std::vector<std::uint8_t>
SceneRenderer::DrawAndReadback(std::uint32_t width, std::uint32_t height,
                               std::span<const SceneDraw> draws,
                               SDL_FColor clear) {
  return device_.RenderAndReadback(
      width, height, clear,
      [&](SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass) {
        Draw(command, pass, draws);
      },
      depth_);
}

void SceneRenderer::Draw(SDL_GPUCommandBuffer *command,
                         SDL_GPURenderPass *pass,
                         std::span<const SceneDraw> draws) {
  if (!command)
    throw std::invalid_argument("scene draw has no command buffer");
  if (!pass)
    throw std::invalid_argument("scene draw has no render pass");
  for (const auto &draw : draws) {
    if (!draw.pipeline)
      throw std::invalid_argument("scene draw has no asset pipeline");
    if (&draw.pipeline->device() != &device_)
      throw std::invalid_argument("scene draw belongs to another GPU device");
  }
  for (const auto material_pass : {MaterialPass::kOpaque,
                                   MaterialPass::kBlended}) {
    for (const auto &draw : draws) {
      draw.pipeline->DrawPass(command, pass, material_pass, draw.transform,
                              draw.textures, draw.joints);
    }
  }
}

std::array<float, 16>
MultiplyTransform(const std::array<float, 16> &left,
                  const std::array<float, 16> &right) {
  std::array<float, 16> result{};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      for (int k = 0; k < 4; ++k)
        result[column * 4 + row] +=
            left[k * 4 + row] * right[column * 4 + k];
    }
  }
  return result;
}

std::array<float, 16> TranslationTransform(float x, float y, float z) {
  return {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
          0.f, 0.f, 1.f, 0.f, x,   y,   z,   1.f};
}

std::array<float, 16> PerspectiveTransform(float vertical_fov_radians,
                                           float aspect, float near_plane,
                                           float far_plane) {
  if (!(vertical_fov_radians > 0.f) || !(aspect > 0.f) ||
      !(near_plane > 0.f) || !(far_plane > near_plane))
    throw std::invalid_argument("invalid perspective camera parameters");
  const float scale = 1.f / std::tan(vertical_fov_radians * .5f);
  std::array<float, 16> result{};
  result[0] = scale / aspect;
  result[5] = scale;
  result[10] = far_plane / (near_plane - far_plane);
  result[11] = -1.f;
  result[14] = near_plane * far_plane / (near_plane - far_plane);
  return result;
}

std::array<float, 16> LookAtTransform(const std::array<float, 3> &eye,
                                      const std::array<float, 3> &target,
                                      const std::array<float, 3> &up) {
  auto normalize = [](std::array<float, 3> value) {
    const float length = std::sqrt(value[0] * value[0] +
                                   value[1] * value[1] +
                                   value[2] * value[2]);
    if (!(length > 0.f))
      throw std::invalid_argument("camera direction has zero length");
    for (auto &component : value)
      component /= length;
    return value;
  };
  auto cross = [](const std::array<float, 3> &left,
                  const std::array<float, 3> &right) {
    return std::array<float, 3>{
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0]};
  };
  const auto forward = normalize({target[0] - eye[0], target[1] - eye[1],
                                  target[2] - eye[2]});
  const auto side = normalize(cross(forward, up));
  const auto camera_up = cross(side, forward);
  const auto dot = [](const std::array<float, 3> &left,
                      const std::array<float, 3> &right) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
  };
  return {side[0],
          camera_up[0],
          -forward[0],
          0.f,
          side[1],
          camera_up[1],
          -forward[1],
          0.f,
          side[2],
          camera_up[2],
          -forward[2],
          0.f,
          -dot(side, eye),
          -dot(camera_up, eye),
          dot(forward, eye),
          1.f};
}

} // namespace mana::gpu
