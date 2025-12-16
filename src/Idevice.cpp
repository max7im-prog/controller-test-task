#include "Idevice.h"
void IDevice::start() {
  _running = true;
  _thread = std::thread(&IDevice::run, this);
}

void IDevice::stop() {
  {
    std::lock_guard<std::mutex> lock{_mutex};
    _running = false;
  }
  _cv.notify_all();
}

void IDevice::join() {
  if (_thread.joinable()) {
    _thread.join();
  }
}
