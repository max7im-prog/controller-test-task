#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

class VirtualSerial {
public:
  void sendAtoB(const std::vector<uint8_t> &data);
  void sendBtoA(const std::vector<uint8_t> &data);

  bool readA(std::vector<uint8_t> &out);
  bool readB(std::vector<uint8_t> &out);

  [[nodiscard]] bool waitAToB();
  [[nodiscard]] bool waitBToA();

  void shutdown();


private:
  std::queue<std::vector<uint8_t>> _queueAToB;
  std::queue<std::vector<uint8_t>> _queueBToA;

  std::mutex _mutexAToB;
  std::mutex _mutexBToA;

  std::condition_variable _cvAToB;
  std::condition_variable _cvBToA;

  bool _shutdown{false};

};
