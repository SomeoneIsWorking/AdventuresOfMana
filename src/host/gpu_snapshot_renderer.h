#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "host/render_snapshot.h"

namespace mana::gpu {

class Asset;
class AssetPipeline;
class Device;
class SceneRenderer;

// SDL3 GPU consumer for the backend-independent running-frame boundary. It
// caches immutable GPU resources by shipping asset name and owns no game state.
class SnapshotRenderer {
public:
  explicit SnapshotRenderer(Device &device);
  ~SnapshotRenderer();

  SnapshotRenderer(const SnapshotRenderer &) = delete;
  SnapshotRenderer &operator=(const SnapshotRenderer &) = delete;

  std::vector<std::uint8_t> DrawAndReadback(const RenderSnapshot &snapshot,
                                            std::uint32_t width,
                                            std::uint32_t height);
  std::size_t cached_asset_count() const { return cache_.size(); }

private:
  struct CachedAsset {
    std::unique_ptr<Asset> asset;
    std::unique_ptr<AssetPipeline> pipeline;
  };

  CachedAsset &RequireAsset(const mcf::RenderAsset &source);

  Device &device_;
  std::unique_ptr<SceneRenderer> scene_;
  std::map<std::string, CachedAsset> cache_;
};

} // namespace mana::gpu
