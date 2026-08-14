#pragma once

#include <SDL3/SDL_gpu.h>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace mana::gpu {

class Asset;
class Device;

struct PipelineFeatures {
  bool depth_test = true;
  bool material_blending = true;
};

enum class MaterialPass { kOpaque, kBlended };

class AssetPipeline {
public:
  AssetPipeline(Device &device, const Asset &asset,
                PipelineFeatures features = {});
  ~AssetPipeline();

  AssetPipeline(const AssetPipeline &) = delete;
  AssetPipeline &operator=(const AssetPipeline &) = delete;

  std::vector<std::uint8_t> DrawAndReadback(std::uint32_t width,
                                            std::uint32_t height,
                                            bool draw = true,
                                            bool textures = true);
  std::vector<std::uint8_t>
  DrawAndReadback(std::uint32_t width, std::uint32_t height,
                  const std::array<float, 16> &transform, bool textures = true,
                  std::span<const float> joints = {});
  void Draw(SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass,
            const std::array<float, 16> &transform, bool textures = true,
            std::span<const float> joints = {});
  void DrawPass(SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass,
                MaterialPass material_pass,
                const std::array<float, 16> &transform, bool textures = true,
                std::span<const float> joints = {});
  std::array<float, 16> TopDownTransform() const;
  Device &device() const { return device_; }

private:
  Device &device_;
  const Asset &asset_;
  PipelineFeatures features_;
  SDL_GPUGraphicsPipeline *opaque_pipeline_ = nullptr;
  SDL_GPUGraphicsPipeline *blend_pipeline_ = nullptr;
};

} // namespace mana::gpu
