

#include "device_a.h"
#include "device_b.h"
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

  auto link = std::make_shared<VirtualSerial>();

  DeviceA a(link);
  DeviceB b(link);

  a.start();
  b.start();

  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  a.stop();
  b.stop();
  link->shutdown();

  a.join();
  b.join();

  std::cout << "Shut down" << std::endl;

  return 0;
}
