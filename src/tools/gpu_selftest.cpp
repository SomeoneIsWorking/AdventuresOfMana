#include "host/gpu_device.h"
#include "host/gpu_overlay.h"
#include "host/gpu_pipeline.h"

#include <exception>
#include <string_view>

#include <lucent/log.h>

int main(int argc, char **argv) {
  try {
    const std::string_view option = argc == 2 ? argv[1] : "";
    if (argc > 2 || (argc == 2 && option != "--negative-control" &&
                     option != "--pipeline-negative-control" &&
                     option != "--overlay-negative-control")) {
      lucent::error("gpu",
                    "usage: {} [--negative-control | "
                    "--pipeline-negative-control | "
                    "--overlay-negative-control]",
                    argv[0]);
      return 2;
    }
    const bool device_negative = option == "--negative-control";
    const bool pipeline_negative = option == "--pipeline-negative-control";
    const bool overlay_negative = option == "--overlay-negative-control";
    const int device_bad = mana::gpu::RunDeviceSelfTest(device_negative);
    return device_negative
               ? device_bad
               : device_bad + mana::gpu::RunPipelineSelfTest(pipeline_negative) +
                     mana::gpu::RunOverlaySelfTest(overlay_negative);
  } catch (const std::exception &error) {
    lucent::error("gpu", "SELFTEST FATAL: {}", error.what());
    return 2;
  }
}
