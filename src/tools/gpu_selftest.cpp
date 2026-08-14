#include "host/gpu_device.h"

#include <exception>

#include <lucent/log.h>

int main() {
  try {
    return mana::gpu::RunDeviceSelfTest();
  } catch (const std::exception &error) {
    lucent::error("gpu", "SELFTEST FATAL: {}", error.what());
    return 2;
  }
}
