#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class DataPacket {
public:
  uint8_t _header{0xAA};
  uint8_t _version{1};
  uint8_t _command_id{0};
  std::vector<std::byte> _payload{};
  uint16_t _crc16{0};

  [[nodiscard]] bool fromData(const std::vector<uint8_t> &data);

  [[nodiscard]] bool toData(std::vector<uint8_t> &out) const;

  static uint16_t generateCRC16(const DataPacket &packet);

  static bool checkCRC16(const DataPacket &packet, uint16_t crc16);

  DataPacket() = default;
  DataPacket(const DataPacket &other) = default;
  DataPacket(DataPacket &&other) = default;
  DataPacket &operator=(const DataPacket &other) = default;
  DataPacket &operator=(DataPacket &&other) = default;
  ~DataPacket() = default;

  enum MessageId {
    MSG_PWM = 0x10,
    MSG_PID = 0x20,
    MSG_STATUS = 0x30,
    RESP_PWM = 0x81,
    RESP_PID = 0x82,
    RESP_STATUS = 0x83,
    RESP_ERROR = 0xFF
  };

private:
  static constexpr uint32_t PACKET_BASE_SIZE = 6;
};
