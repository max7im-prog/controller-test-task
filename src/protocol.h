#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class DataPacket {
public:
  uint8_t header = 0xAA;
  uint8_t version = 0x01;
  uint8_t command_id;
  uint8_t payload_len;
  std::vector<std::byte> payload;
  uint8_t crc16;

  DataPacket();

  void fromData(const std::vector<uint8_t> &data);
  void toData(std::vector<uint8_t>& out) const;

  DataPacket(const DataPacket &other) = default;
  DataPacket(DataPacket &&other) = default;
  DataPacket &operator=(const DataPacket &other) = default;
  DataPacket &operator=(DataPacket &&other) = default;
  ~DataPacket() = default;

private:
  static uint8_t generateCRC16(const DataPacket& packet);
  static bool checkCRC16(const DataPacket& packet, uint8_t crc16);


};
