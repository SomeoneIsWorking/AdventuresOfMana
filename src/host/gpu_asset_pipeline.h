#pragma once

#include <SDL3/SDL_gpu.h>

#include <array>
#include <cstdint>
#include <vector>

namespace mana::gpu {

class Asset;
class Device;

class AssetPipeline {
public:
  AssetPipeline(Device &device, const Asset &asset);
  ~AssetPipeline();

  AssetPipeline(const AssetPipeline &) = delete;
  AssetPipeline &operator=(const AssetPipeline &) = delete;

  std::vector<std::uint8_t> DrawAndReadback(std::uint32_t width,
                                            std::uint32_t height,
                                            bool draw = true,
                                            bool textures = true);

private:
  std::array<float, 16> FitTopDown() const;

  Device &device_;
  const Asset &asset_;
  SDL_GPUGraphicsPipeline *pipeline_ = nullptr;
};

int RunAssetPipelineSelfTest(const char *archive_path,
                             bool negative_control = false);

} // namespace mana::gpu
