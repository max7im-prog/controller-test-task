#include "device_b.h"

#include "data_packet.h"
#include <chrono>
#include <iostream>
#include <random>
#include <spdlog/spdlog.h>

namespace {
void formPWMAckPacket(DataPacket &dataPacket, uint16_t pwm) {
  dataPacket._command_id = DataPacket::MessageId::RESP_PWM;
  dataPacket._payload.reserve(2);
  dataPacket._payload.clear();
  dataPacket._payload.push_back(std::byte(pwm & 0xFF));
  dataPacket._payload.push_back(std::byte((pwm >> 8) & 0xFF));
  dataPacket._crc16 = DataPacket::generateCRC16(dataPacket);
}

void formStatusAckPacket(DataPacket &dataPacket, uint8_t battery,
                         uint8_t temperature) {
  dataPacket._command_id = DataPacket::MessageId::RESP_STATUS;
  dataPacket._payload.clear();
  dataPacket._payload.push_back(static_cast<std::byte>(battery));
  dataPacket._payload.push_back(static_cast<std::byte>(temperature));
  dataPacket._crc16 = DataPacket::generateCRC16(dataPacket);
}

void formErrorResponsePacket(DataPacket &dataPacket) {
  dataPacket._command_id = DataPacket::MessageId::RESP_ERROR;
  dataPacket._payload.clear();
  dataPacket._crc16 = DataPacket::generateCRC16(dataPacket);
}

void formPIDAckPacket(DataPacket &dataPacket, float kp, float ki, float kd) {
  dataPacket._command_id = DataPacket::MessageId::RESP_PID;
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
               std::function<bool(const DataPacket &, DeviceB &device)>>
    DeviceB::responseDispatchTable = {

        {DataPacket::MessageId::MSG_PWM,
         [](const DataPacket &dataPacket, DeviceB &device) -> bool {
           DataPacket respPacket;
           if (dataPacket._payload.size() != 2) {
             spdlog::error("[B] Malformed PWM message");
             formErrorResponsePacket(respPacket);
           } else {
             device._deviceState._pwm =
                 (static_cast<uint16_t>(dataPacket._payload[0]) & 0xFF) |
                 ((static_cast<uint16_t>(dataPacket._payload[1]) << 8) &
                  0xFF00);
             formPWMAckPacket(respPacket, device._deviceState._pwm);
           }

           std::vector<uint8_t> serializedData;
           if (!respPacket.toData(serializedData)) {
             spdlog::error("[B] failed to serialize data");
             return false;
           }
           device._link->sendBtoA(serializedData);
           return true;
         }},

        {DataPacket::MSG_PID,
         [](const DataPacket &dataPacket, DeviceB &device) -> bool {
           DataPacket respPacket;

           if (dataPacket._payload.size() != 12) {
             spdlog::error("[B] Malformed PID message");
             formErrorResponsePacket(respPacket);
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

             device._deviceState.kp = readFloat();
             device._deviceState.ki = readFloat();
             device._deviceState.kd = readFloat();
             formPIDAckPacket(respPacket, device._deviceState.kp,
                              device._deviceState.ki, device._deviceState.kd);
           }

           std::vector<uint8_t> serializedData;
           if (!respPacket.toData(serializedData)) {
             spdlog::error("[B] failed to serialize data");
             return false;
           }
           device._link->sendBtoA(serializedData);
           return true;
         }},

        {DataPacket::MessageId::STATE_ERROR,
         [](const DataPacket &dataPacket, DeviceB &device) -> bool {
           DataPacket respPacket;
           formErrorResponsePacket(respPacket);

           std::vector<uint8_t> serializedData;
           if (!respPacket.toData(serializedData)) {
             std::cerr << "failed to serialize data" << std::endl;
             return false;
           }
           device._link->sendBtoA(serializedData);
           return true;
         }},

        {DataPacket::MessageId::MSG_STATUS,
         [](const DataPacket &dataPacket, DeviceB &device) -> bool {
           DataPacket respPacket;
           formStatusAckPacket(respPacket, device._rnd() % 100,
                               device._rnd() % 20 + 20);

           std::vector<uint8_t> serializedData;
           if (!respPacket.toData(serializedData)) {
             spdlog::error("[B] failed to serialize data");
             return false;
           }

           device._link->sendBtoA(serializedData);
           return true;
         }}};

DeviceB::DeviceB(std::shared_ptr<VirtualSerial> link)
    : IDevice(std::chrono::milliseconds(0)), _link(link),
      _rnd(std::chrono::system_clock::now().time_since_epoch().count()) {}

void DeviceB::step() {

  {
    bool hasData = _link->waitAToB();
    if (!hasData) {
      spdlog::info("[B] Shutdown was issued on link");
      return;
    }
  }

  std::vector<uint8_t> receivedData;
  {
    bool received = _link->readB(receivedData);
    if (!received) {
      spdlog::info("[B] No data in link");
      return;
    }
  }

  DataPacket dataPacket;
  bool packetIsValid{true};
  if (packetIsValid) {
    bool parsed = dataPacket.fromData(receivedData);
    if (!parsed) {
      spdlog::error("[B] Failed to parse data");
      packetIsValid = false;
      dataPacket._command_id = DataPacket::MessageId::STATE_ERROR;
    }
  }

  if (packetIsValid) {
    bool crc16Matches = DataPacket::checkCRC16(dataPacket, dataPacket._crc16);
    if (!crc16Matches) {
      spdlog::error("[B] crc16 does not match");
      packetIsValid = false;
      dataPacket._command_id = DataPacket::MessageId::STATE_ERROR;
    }
  }

  {
    if (responseDispatchTable.find(dataPacket._command_id) ==
        responseDispatchTable.end()) {
      spdlog::error("[B] Unknown command: " +
                    std::to_string(static_cast<int>(dataPacket._command_id)));
      packetIsValid = false;
      dataPacket._command_id = DataPacket::MessageId::STATE_ERROR;
      if (responseDispatchTable.find(DataPacket::MessageId::STATE_ERROR) ==
          responseDispatchTable.end()) {
        spdlog::error("[B] Unknown command: " +
                      std::to_string(static_cast<int>(dataPacket._command_id)));
      }
    }
    if (!responseDispatchTable.at(dataPacket._command_id)(dataPacket, *this)) {
        spdlog::error("[B] Failed to handle command: " +
                      std::to_string(static_cast<int>(dataPacket._command_id)));
    }
  }
}
