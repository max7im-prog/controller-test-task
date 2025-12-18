#pragma once

#include <atomic>
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

  template <typename Rep, typename Period>
  bool waitAToB(std::chrono::duration<Rep, Period> timeout) {
    std::unique_lock<std::mutex> lock(_mutexAToB);
    bool ready = _cvAToB.wait_for(
        lock, timeout, [&]() { return _shutdown || !_queueAToB.empty(); });

    return ready && !_shutdown;
  }

  template <typename Rep, typename Period>
  bool waitBToA(std::chrono::duration<Rep, Period> timeout) {
    std::unique_lock<std::mutex> lock(_mutexBToA);
    bool ready = _cvBToA.wait_for(
        lock, timeout, [&]() { return _shutdown || !_queueBToA.empty(); });

    return ready && !_shutdown;
  }
  void shutdown();

private:
  std::queue<std::vector<uint8_t>> _queueAToB;
  std::queue<std::vector<uint8_t>> _queueBToA;

  std::mutex _mutexAToB;
  std::mutex _mutexBToA;

  std::condition_variable _cvAToB;
  std::condition_variable _cvBToA;

  std::atomic<bool> _shutdown{false};
};
