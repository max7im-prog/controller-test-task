#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class DataPacket {
public:
  uint8_t _header;
  uint8_t _version;
  uint8_t _command_id;
  uint8_t _payload_len;
  std::vector<std::byte> _payload;
  uint16_t _crc16;

  DataPacket();

  [[nodiscard]] bool fromData(const std::vector<uint8_t> &data);
  [[nodiscard]] bool toData(std::vector<uint8_t> &out) const;

  DataPacket(const DataPacket &other) = default;
  DataPacket(DataPacket &&other) = default;
  DataPacket &operator=(const DataPacket &other) = default;
  DataPacket &operator=(DataPacket &&other) = default;
  ~DataPacket() = default;

private:
  static uint16_t generateCRC16(const DataPacket &packet);
  static bool checkCRC16(const DataPacket &packet, uint16_t crc16);
  static constexpr uint32_t PACKET_BASE_SIZE = 6;
};
