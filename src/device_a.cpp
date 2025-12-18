#include "device_a.h"
#include "data_packet.h"
#include <bit>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>

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

void formPIDDataPacket(DataPacket &dataPacket, float kp, float ki, float kd) {
  dataPacket._command_id = DataPacket::MessageId::MSG_PID;
  dataPacket._payload.reserve(12);
  dataPacket._payload.clear();

  auto pushFloat = [&](float val) {
    uint32_t bits = std::bit_cast<uint32_t>(val);
    dataPacket._payload.push_back(static_cast<std::byte>(bits & 0xFF));
    dataPacket._payload.push_back(static_cast<std::byte>((bits >> 8) & 0xFF));
    dataPacket._payload.push_back(static_cast<std::byte>((bits >> 16) & 0xFF));
    dataPacket._payload.push_back(static_cast<std::byte>((bits >> 24) & 0xFF));
  };

  pushFloat(kp);
  pushFloat(ki);
  pushFloat(kd);

  dataPacket._crc16 = DataPacket::generateCRC16(dataPacket);
}

} // namespace

const std::map<std::uint8_t,
               std::function<bool(const DataPacket &, DeviceA &device)>>
    DeviceA::responseDispatchTable = {

        {DataPacket::RESP_STATUS,
         [](const DataPacket &dataPacket, DeviceA &device) -> bool {
           if (dataPacket._payload.size() != 2) {
             std::cerr << "[E] Malformed status response" << std::endl;
           } else {
             uint8_t battery = static_cast<uint8_t>(dataPacket._payload[0]);
             uint8_t temperature = static_cast<uint8_t>(dataPacket._payload[1]);

             std::ostringstream oss;
             oss << "[A] Received status: battery=" << static_cast<int>(battery)
                 << " temperature=" << static_cast<int>(temperature)
                 << std::endl;
             std::cout << oss.str();
           }
           return true;
         }},

        {DataPacket::RESP_ERROR,
         [](const DataPacket &dataPacket, DeviceA &device) -> bool {
           std::ostringstream oss;
           oss << "[A] Status: ERROR" << std::endl;
           std::cout << oss.str();
           return true;
         }},

        {DataPacket::RESP_PWM,
         [](const DataPacket &dataPacket, DeviceA &device) -> bool {
           if (dataPacket._payload.size() != 2) {
             std::cerr << "[E] Malformed PWM response" << std::endl;
           } else {
             uint16_t pwm =
                 (static_cast<uint16_t>(dataPacket._payload[0]) & 0xFF) |
                 ((static_cast<uint16_t>(dataPacket._payload[1]) << 8) &
                  0xFF00);

             std::ostringstream oss;
             oss << "[A] Received status: pwm=" << static_cast<int>(pwm)
                 << std::endl;
             std::cout << oss.str();
           }
           return true;
         }},

        {DataPacket::RESP_PID,
         [](const DataPacket &dataPacket, DeviceA &device) -> bool {
           if (dataPacket._payload.size() != 12) {
             std::cerr << "[E] Malformed PID response" << std::endl;
           } else {

             size_t iter{0};

             auto readFloat = [&]() -> float {
               uint32_t bits{0};
               bits |= std::to_integer<uint32_t>(dataPacket._payload[iter++]);
               bits |= (std::to_integer<uint32_t>(dataPacket._payload[iter++])
                        << 8);
               bits |= (std::to_integer<uint32_t>(dataPacket._payload[iter++])
                        << 16);
               bits |= (std::to_integer<uint32_t>(dataPacket._payload[iter++])
                        << 24);
               return std::bit_cast<float>(bits);
             };

             float kp = readFloat();
             float ki = readFloat();
             float kd = readFloat();

             std::ostringstream oss;
             oss << "[A] Received status: kp=" << kp << ", ki=" << ki
                 << ", kd=" << kd << std::endl;
             std::cout << oss.str();
           }
           return true;
         }}

};

DeviceA::DeviceA(std::shared_ptr<VirtualSerial> link)
    : IDevice(std::chrono::milliseconds(100)), _link(link),
      _rnd(std::chrono::system_clock::now().time_since_epoch().count()) {}

void DeviceA::step() {
  DataPacket sendDataPacket;

  {
    auto rndRes = _rnd() % 2;
    if (rndRes == 0) {
      // MSG_PWM
      uint16_t pwm = static_cast<uint16_t>(_rnd() % 1000);
      formPWMDataPacket(sendDataPacket, pwm);

    } else if (rndRes == 1) {
      // MSG_STATUS
      formStatusDataPacket(sendDataPacket);
    }   }

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
            receivedDataPacket, *this)) {
      std::cerr << "Failed to handle command: "
                << receivedDataPacket._command_id << std::endl;
    }
  }
}
