#pragma once

#include <atomic>
#include <condition_variable>
#include <thread>
class IDevice {
public:
  virtual ~IDevice() = 0;
  void start();
  void stop();
  void join();

protected:
  virtual void run() = 0;
  std::mutex _mutex;
  std::condition_variable _cv;
  bool _running{false};

private:
  std::thread _thread;
};
