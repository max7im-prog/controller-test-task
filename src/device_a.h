#pragma once

#include "Idevice.h"
#include "virtual_serial.h"
#include <random>
class DeviceA : public IDevice {
public:
  DeviceA(std::shared_ptr<VirtualSerial> link);

protected:
  virtual void step() override;
  std::shared_ptr<VirtualSerial> _link;
  std::mt19937 _rnd;
};
