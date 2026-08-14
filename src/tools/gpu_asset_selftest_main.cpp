#include "tools/gpu_asset_selftest.h"

#include <exception>
#include <string_view>

#include <lucent/log.h>

int main(int argc, char **argv) {
  try {
    const bool negative =
        argc == 3 && std::string_view(argv[1]) == "--negative-control";
    const bool capture = argc == 4 && std::string_view(argv[1]) == "--capture";
    const bool capture_pair =
        argc == 5 && std::string_view(argv[1]) == "--capture-pair";
    const char *archive = negative       ? argv[2]
                          : capture      ? argv[3]
                          : capture_pair ? argv[4]
                          : argc == 2    ? argv[1]
                                         : nullptr;
    if (!archive) {
      lucent::error(
          "gpu",
          "usage: {} [--negative-control] ARCHIVE | --capture PNG ARCHIVE | "
          "--capture-pair ENHANCED_PNG VANILLA_PNG ARCHIVE",
          argv[0]);
      return 2;
    }
    return mana::gpu::RunAssetPipelineSelfTest(archive, negative,
                                               capture        ? argv[2]
                                               : capture_pair ? argv[2]
                                                              : nullptr,
                                               capture_pair ? argv[3]
                                                            : nullptr);
  } catch (const std::exception &error) {
    lucent::error("gpu", "ASSET SELFTEST FATAL: {}", error.what());
    return 2;
  }
}
