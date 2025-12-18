#pragma once

#include "Idevice.h"
#include "data_packet.h"
#include "virtual_serial.h"
#include <functional>
#include <map>
#include <random>
class DeviceA : public IDevice {
public:
  DeviceA(std::shared_ptr<VirtualSerial> link);

protected:
  virtual void step() override;
  std::shared_ptr<VirtualSerial> _link;
  std::mt19937 _rnd;
  static const std::map<
      std::uint8_t,
      std::function<bool(const DataPacket &, std::shared_ptr<VirtualSerial>)>>
      responseDispatchTable; // TODO: fill dispatch table
};
