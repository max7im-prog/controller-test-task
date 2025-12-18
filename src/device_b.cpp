#include "device_b.h"

#include "data_packet.h"
#include <chrono>
#include <iostream>
#include <random>

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

} // namespace

const std::map<std::uint8_t,
               std::function<bool(const DataPacket &, DeviceB &device)>>
    DeviceB::responseDispatchTable = {

        {DataPacket::MessageId::MSG_PWM,
         [](const DataPacket &dataPacket, DeviceB &device) -> bool {
           if (dataPacket._payload.size() != 2) {
             std::cerr << "[E] Malformed PWM message" << std::endl;
           } else {
             device._deviceState._pwm =
                 (static_cast<uint16_t>(dataPacket._payload[0]) & 0xFF) |
                 ((static_cast<uint16_t>(dataPacket._payload[1]) << 8) & 0xFF00);
             DataPacket respPacket;
             formPWMAckPacket(respPacket, device._deviceState._pwm);

             std::vector<uint8_t> serializedData;
             if (!respPacket.toData(serializedData)) {
               std::cerr << "failed to serialize data" << std::endl;
               return false;
             }
             device._link->sendBtoA(serializedData);
           }
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
             std::cerr << "failed to serialize data" << std::endl;
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
      std::cerr << "Shutdown was issued on link" << std::endl;
      return;
    }
  }

  std::vector<uint8_t> receivedData;
  {
    bool received = _link->readB(receivedData);
    if (!received) {
      std::cerr << "No data in link" << std::endl;
      return;
    }
  }

  DataPacket dataPacket;
  bool packetIsValid{true};
  if (packetIsValid) {
    bool parsed = dataPacket.fromData(receivedData);
    if (!parsed) {
      std::cerr << "Failed to parse data" << std::endl;
      packetIsValid = false;
      dataPacket._command_id = DataPacket::MessageId::STATE_ERROR;
    }
  }

  if (packetIsValid) {
    bool crc16Matches = DataPacket::checkCRC16(dataPacket, dataPacket._crc16);
    if (!crc16Matches) {
      std::cerr << "crc16 does not match" << std::endl;
      packetIsValid = false;
      dataPacket._command_id = DataPacket::MessageId::STATE_ERROR;
    }
  }

  {
    if (responseDispatchTable.find(dataPacket._command_id) ==
        responseDispatchTable.end()) {
      std::cerr << "Unknown command: " << dataPacket._command_id << std::endl;
      packetIsValid = false;
      dataPacket._command_id = DataPacket::MessageId::STATE_ERROR;
      if (responseDispatchTable.find(DataPacket::MessageId::STATE_ERROR) ==
          responseDispatchTable.end()) {
        std::cerr << "Unknown command: " << dataPacket._command_id << std::endl;
      }
    }
    if (!responseDispatchTable.at(dataPacket._command_id)(dataPacket, *this)) {
      std::cerr << "Failed to handle command: " << dataPacket._command_id
                << std::endl;
    }
  }
}
