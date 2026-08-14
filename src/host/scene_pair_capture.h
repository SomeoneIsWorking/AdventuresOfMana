#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace mana {

struct RenderSnapshot;

// Diagnostic owner for a same-frame GLES/SDL3 GPU comparison. It owns the
// secondary GPU backend and every output-side concern, but no game state.
class ScenePairCapture {
public:
  explicit ScenePairCapture(std::string output_prefix);
  ~ScenePairCapture();

  ScenePairCapture(const ScenePairCapture &) = delete;
  ScenePairCapture &operator=(const ScenePairCapture &) = delete;

  const char *driver() const;
  void Write(const RenderSnapshot &snapshot, std::uint32_t width,
             std::uint32_t height,
             std::span<const std::uint8_t> gles_bottom_up_rgba);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace mana
