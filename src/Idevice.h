#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>

class IDevice {
public:
  IDevice() = default;
  IDevice(std::chrono::milliseconds updateInterval);
  virtual ~IDevice() = 0;
  void start();
  void stop();
  void join();

protected:
  virtual void step() = 0;
  std::chrono::milliseconds _updateInterval{0};

private:
  void run();
  std::thread _thread;
  std::mutex _mutex;
  std::condition_variable _cv;
  std::atomic<bool> _stopCondition{false};

  IDevice(IDevice &other) = delete;
  IDevice &operator=(IDevice &other) = delete;
};
