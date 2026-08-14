#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <vector>

namespace mana::gpu {

class Device;

class SolidPipeline {
public:
  explicit SolidPipeline(Device &device);
  ~SolidPipeline();

  SolidPipeline(const SolidPipeline &) = delete;
  SolidPipeline &operator=(const SolidPipeline &) = delete;

  std::vector<std::uint8_t> DrawAndReadback(std::uint32_t width,
                                            std::uint32_t height);

private:
  Device &device_;
  SDL_GPUGraphicsPipeline *pipeline_ = nullptr;
};

int RunPipelineSelfTest(bool negative_control = false);

} // namespace mana::gpu
