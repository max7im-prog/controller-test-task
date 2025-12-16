#include "protocol.h"

[[nodiscard]] bool DataPacket::fromData(const std::vector<uint8_t> &data) {
  if (data.size() < PACKET_BASE_SIZE) {
    return false;
  }

  // Local variables to avoid corrupting the packet until data is checked
  size_t offset{0};
  uint8_t header = data[offset++];
  uint8_t version = data[offset++];
  uint8_t command_id = data[offset++];
  uint8_t payload_len = data[offset++];

  if (PACKET_BASE_SIZE + payload_len != data.size()) {
    return false;
  }

  _header = header;
  _version = version;
  _command_id = command_id;
  _payload.clear();
  _payload.reserve(payload_len);
  for (size_t i = 0; i < payload_len; i++) {
    _payload.push_back(static_cast<std::byte>(data[offset++]));
  }
  _crc16 = static_cast<uint8_t>(data[offset] & 0xFF) |
           static_cast<uint8_t>((data[offset + 1] << 8) & 0xFF00);

  return true;
}

[[nodiscard]] bool DataPacket::toData(std::vector<uint8_t> &out) const {
  if (static_cast<uint32_t>(_payload.size()) >
      static_cast<uint32_t>(UINT8_MAX)) {
    return false;
  }

  uint32_t dataSize = DataPacket::PACKET_BASE_SIZE + _payload.size();
  out.clear();
  out.reserve(dataSize);

  out.push_back(_header);
  out.push_back(_version);
  out.push_back(_command_id);
  const uint8_t payload_len = static_cast<uint8_t>(_payload.size());
  out.push_back(payload_len);
  for (const auto &el : _payload) {
    out.push_back(static_cast<uint8_t>(el));
  }
  out.push_back(static_cast<uint8_t>(_crc16 & 0xFF));
  out.push_back(static_cast<uint8_t>((_crc16 >> 8) & 0xFF));

  return true;
}

uint16_t DataPacket::generateCRC16(const DataPacket &packet) {
  uint16_t crc = 0xFFFF;

  auto update_crc = [&](uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0x8408; // reversed 0x1021
      } else {
        crc >>= 1;
      }
    }
  };

  // Header fields
  update_crc(packet._header);
  update_crc(packet._version);
  update_crc(packet._command_id);

  // Payload length
  uint8_t payload_len = static_cast<uint8_t>(packet._payload.size());
  update_crc(payload_len);

  // Payload bytes
  for (std::byte b : packet._payload) {
    update_crc(static_cast<uint8_t>(b));
  }

  return ~crc;
}

bool DataPacket::checkCRC16(const DataPacket &packet, uint16_t crc16) {
  return (crc16 == DataPacket::generateCRC16(packet));
}
