#pragma once

namespace mana::gpu {

int RunAssetPipelineSelfTest(const char *archive_path,
                             bool negative_control = false,
                             const char *capture_path = nullptr);

} // namespace mana::gpu
