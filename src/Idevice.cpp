#include "Idevice.h"

IDevice::~IDevice() = default;

void IDevice::run() {
  std::unique_lock<std::mutex> lock{_mutex};
  while (!_stopCondition) {
    _cv.wait_for(lock, _updateInterval,
                 [&]() -> bool { return static_cast<bool>(_stopCondition); });
    if (_stopCondition) {
      break;
    }
    lock.unlock();
    step();
    lock.lock();
  }
}

void IDevice::start() { _thread = std::thread(&IDevice::run, this); }

void IDevice::stop() {
  _stopCondition = true;
  _cv.notify_all();
}

void IDevice::join() {
  if (_thread.joinable()) {
    _thread.join();
  }
}

IDevice::IDevice(std::chrono::milliseconds updateInterval)
    : _updateInterval(updateInterval) {}
