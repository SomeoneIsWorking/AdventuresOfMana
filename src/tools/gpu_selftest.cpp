#include "host/gpu_device.h"

#include <exception>
#include <string_view>

#include <lucent/log.h>

int main(int argc, char **argv) {
  try {
    if (argc > 2 ||
        (argc == 2 && std::string_view(argv[1]) != "--negative-control")) {
      lucent::error("gpu", "usage: {} [--negative-control]", argv[0]);
      return 2;
    }
    return mana::gpu::RunDeviceSelfTest(argc == 2);
  } catch (const std::exception &error) {
    lucent::error("gpu", "SELFTEST FATAL: {}", error.what());
    return 2;
  }
}
