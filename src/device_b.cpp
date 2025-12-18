#include "device_b.h"
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

DeviceB::DeviceB(std::shared_ptr<VirtualSerial> link)
    : IDevice(std::chrono::milliseconds(100)), _link(link),
      _rnd(std::chrono::system_clock::now().time_since_epoch().count()) {}

void DeviceB::step() {
  std::vector<uint8_t> receivedData;

  {
    bool received = _link->readB(receivedData);
    if (!received) {
      std::cerr << "Shutdown issued on link" << std::endl;
      return;
    }
  }

  DataPacket dataPacket;
  {
    bool parsed = dataPacket.fromData(receivedData);
    if (!parsed) {
      std::cerr << "Failed to parse data" << std::endl;
      return;
    }
  }

  {
    bool crc16Matches = DataPacket::checkCRC16(dataPacket, dataPacket._crc16);
    if (!crc16Matches) {
      std::cerr << "crc16 does not match" << std::endl;
      return;
    }
  }

  {
    if (responseDispatchTable.find(dataPacket._command_id) ==
        responseDispatchTable.end()) {
      std::cerr << "Unknown command: " << dataPacket._command_id << std::endl;
      return;
    }
    if (!responseDispatchTable.at(dataPacket._command_id)(dataPacket, _link)) {
      std::cerr << "Failed to handle command: " << dataPacket._command_id
                << std::endl;
    }
  }
}
