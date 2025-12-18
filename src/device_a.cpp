#include "device_a.h"
#include "data_packet.h"
#include <chrono>
#include <iostream>
#include <random>

namespace {
void formPWMDataPacket(DataPacket &dataPacket, uint16_t pwm) {
  dataPacket._command_id = DataPacket::MessageId::MSG_PWM;
  dataPacket._payload.reserve(2);
  dataPacket._payload.clear();
  dataPacket._payload.push_back(std::byte(pwm & 0xFF));
  dataPacket._payload.push_back(std::byte((pwm >> 8) & 0xFF));
  dataPacket._crc16 = DataPacket::generateCRC16(dataPacket);
}

void formStatusDataPacket(DataPacket &dataPacket) {
  dataPacket._command_id = DataPacket::MessageId::MSG_STATUS;
  dataPacket._payload.clear();
  dataPacket._crc16 = DataPacket::generateCRC16(dataPacket);
}

} // namespace

DeviceA::DeviceA(std::shared_ptr<VirtualSerial> link)
    : IDevice(std::chrono::milliseconds(100)), _link(link),
      _rnd(std::chrono::system_clock::now().time_since_epoch().count()) {}

void DeviceA::step() {
  DataPacket dataPacket;

  if (_rnd() % 2 == 0) {
    // MSG_PWM
    uint16_t pwm = static_cast<uint16_t>(_rnd() % 1000);
    formPWMDataPacket(dataPacket, pwm);

  } else {
    // MSG_STATUS
    formStatusDataPacket(dataPacket);
  }

  {
    std::vector<uint8_t> serializedData;
    bool serialized = dataPacket.toData(serializedData);
    if (serialized) {
      _link->sendAtoB(serializedData);
    } else {
      std::cerr << "Failed to serialize data" << std::endl;
      return;
    }
  }

  {
    bool responded = _link->waitBToA();
    if (responded) {
      // TODO: handle response
    } else {
      std::cerr << "Shutdown was issued on link" << std::endl;
      return;
    }
  }
}
