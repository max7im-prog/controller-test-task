#include "virtual_serial.h"
#include <mutex>

void VirtualSerial::sendAtoB(const std::vector<uint8_t> &data) {
  {
    std::lock_guard<std::mutex> lock(_mutexAToB);
    _queueAToB.push(data);
  }
  _cvAToB.notify_one();
}

void VirtualSerial::sendBtoA(const std::vector<uint8_t> &data) {
  {
    std::lock_guard<std::mutex> lock(_mutexBToA);
    _queueBToA.push(data);
  }
  _cvBToA.notify_one();
}

bool VirtualSerial::readA(std::vector<uint8_t> &out) {
  {
    std::lock_guard<std::mutex> lock(_mutexBToA);
    if (_queueBToA.empty()) {
      return false;
    }
    out = std::move(_queueBToA.front());
    _queueBToA.pop();
  }
  return true;
}

bool VirtualSerial::readB(std::vector<uint8_t> &out) {
  {
    std::lock_guard<std::mutex> lock(_mutexAToB);
    if (_queueAToB.empty()) {
      return false;
    }
    out = std::move(_queueAToB.front());
    _queueAToB.pop();
  }
  return true;
}

bool VirtualSerial::waitAToB() {
  std::unique_lock<std::mutex> lock(_mutexAToB);
  _cvAToB.wait(lock, [&]() { return _shutdown || !_queueAToB.empty(); });

  return !_shutdown;
}

bool VirtualSerial::waitBToA() {
  std::unique_lock<std::mutex> lock(_mutexBToA);
  _cvBToA.wait(lock, [&]() { return _shutdown || !_queueBToA.empty(); });

  return !_shutdown;
}


void VirtualSerial::shutdown() {
  {
    std::lock_guard<std::mutex> lockAToB{_mutexAToB};
    std::lock_guard<std::mutex> lockBToA{_mutexBToA};
    _shutdown = true;
  }
  _cvAToB.notify_all();
  _cvBToA.notify_all();
}
