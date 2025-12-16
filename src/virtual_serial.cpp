#include "virtual_serial.h"
#include <mutex>

void VirtualSerial::sendAtoB(const std::vector<uint8_t> &data) {
  {
    std::lock_guard<std::mutex> lock(_mutexAToB);
    _queueAToB.push(data);
  }
}

void VirtualSerial::sendBtoA(const std::vector<uint8_t> &data) {
  {
    std::lock_guard<std::mutex> lock(_mutexBToA);
    _queueBToA.push(data);
  }
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
