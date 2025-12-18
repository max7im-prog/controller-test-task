#include "device_a.h"
#include "data_packet.h"
#include <chrono>
#include <iostream>
#include <random>

const std::map<
    std::uint8_t,
    std::function<bool(const DataPacket &, std::shared_ptr<VirtualSerial>)>>
    DeviceA::responseDispatchTable = {

        {DataPacket::RESP_STATUS,
         [](const DataPacket &dataPacket,
            std::shared_ptr<VirtualSerial> link) -> bool {
           if (dataPacket._payload.size() != 2) {
             std::cerr << "[E] Malformed status response" << std::endl;
           } else {
             uint8_t battery = static_cast<uint8_t>(dataPacket._payload[0]);
             uint8_t temperature = static_cast<uint8_t>(dataPacket._payload[1]);
             std::cout << "[A] Received status: battery=" << battery
                       << " temperature=" << temperature << std::endl;
           }
           return true;
         }},

        {DataPacket::RESP_PWM,
         [](const DataPacket &dataPacket,
            std::shared_ptr<VirtualSerial> link) -> bool {
           if (dataPacket._payload.size() != 2) {
             std::cerr << "[E] Malformed PWM response" << std::endl;
           } else {
             uint16_t pwm =
                 (static_cast<uint8_t>(dataPacket._payload[0]) & 0xFF) |
                 (static_cast<uint8_t>(dataPacket._payload[1] << 8) & 0xFF00);
             std::cout << "[A] Received status: pwm=" << pwm << std::endl;
           }
           return true;
         }}

};

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
  DataPacket sendDataPacket;

  if (_rnd() % 2 == 0) {
    // MSG_PWM
    uint16_t pwm = static_cast<uint16_t>(_rnd() % 1000);
    formPWMDataPacket(sendDataPacket, pwm);

  } else {
    // MSG_STATUS
    formStatusDataPacket(sendDataPacket);
  }

  {
    std::vector<uint8_t> serializedData;
    bool serialized = sendDataPacket.toData(serializedData);
    if (serialized) {
      _link->sendAtoB(serializedData);
    } else {
      std::cerr << "Failed to serialize data" << std::endl;
      return;
    }
  }

  {
    bool hasData = _link->waitBToA();
    if (!hasData) {
      std::cerr << "Shutdown was issued on link" << std::endl;
      return;
    }
  }

  std::vector<uint8_t> receivedData;

  {
    bool hasData = _link->readA(receivedData);
    if (!hasData) {
      std::cerr << "No data on link" << std::endl;
      return;
    }
  }

  DataPacket receivedDataPacket;
  {
    bool parsed = receivedDataPacket.fromData(receivedData);
    if (!parsed) {

      std::cerr << "Failed to parse data packet" << std::endl;
      return;
    }
  }

  {
    bool crc16Matches =
        DataPacket::checkCRC16(receivedDataPacket, receivedDataPacket._crc16);
    if (!crc16Matches) {
      std::cerr << "CRC16 does not match on receive" << std::endl;
      return;
    }
  }

  {
    if (responseDispatchTable.find(receivedDataPacket._command_id) ==
        responseDispatchTable.end()) {
      std::cerr << "Unknown command: " << receivedDataPacket._command_id
                << std::endl;
      return;
    }
    if (!responseDispatchTable.at(receivedDataPacket._command_id)(
            receivedDataPacket, _link)) {
      std::cerr << "Failed to handle command: "
                << receivedDataPacket._command_id << std::endl;
    }
  }
}
