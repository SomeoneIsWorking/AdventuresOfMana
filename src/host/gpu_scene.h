#pragma once

#include <SDL3/SDL_gpu.h>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace mana::gpu {

class AssetPipeline;
class Device;

struct SceneDraw {
  AssetPipeline *pipeline = nullptr;
  std::array<float, 16> transform{};
  std::span<const float> joints;
  bool textures = true;
};

// Owns scene-wide submission order. AssetPipeline owns per-asset GPU state;
// this owner ensures every opaque draw completes before any blended draw.
class SceneRenderer {
public:
  explicit SceneRenderer(Device &device, bool depth = true)
      : device_(device), depth_(depth) {}

  std::vector<std::uint8_t>
  DrawAndReadback(std::uint32_t width, std::uint32_t height,
                  std::span<const SceneDraw> draws,
                  SDL_FColor clear = {0.f, 1.f, 1.f, 1.f});
  void Draw(SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass,
            std::span<const SceneDraw> draws);

private:
  Device &device_;
  bool depth_;
};

std::array<float, 16>
MultiplyTransform(const std::array<float, 16> &left,
                  const std::array<float, 16> &right);
std::array<float, 16> TranslationTransform(float x, float y, float z);
std::array<float, 16> PerspectiveTransform(float vertical_fov_radians,
                                           float aspect, float near_plane,
                                           float far_plane);
std::array<float, 16> LookAtTransform(const std::array<float, 3> &eye,
                                      const std::array<float, 3> &target,
                                      const std::array<float, 3> &up);

} // namespace mana::gpu
