#include "device_a.h"
#include "device_b.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"
#include "virtual_serial.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace {
std::atomic<bool> g_running{true};
}

int main(int argc, char **argv) {
  std::signal(SIGINT, [](int) -> void { g_running = false; });
  std::signal(SIGTERM, [](int) -> void { g_running = false; });

  auto logger = spdlog::rotating_logger_mt("logger", "log.txt",
                                           1048576 * 3, // 5MB files
                                           3            // 3 files
  );

  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);

  auto link = std::make_shared<VirtualSerial>();

  DeviceA a(link);
  DeviceB b(link);

  a.start();
  b.start();
  spdlog::info("[MAIN] program started");

  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  link->shutdown();
  a.stop();
  b.stop();

  a.join();
  b.join();

  std::cout << "Shut down" << std::endl;

  spdlog::info("[MAIN] program finished");

  return 0;
}
