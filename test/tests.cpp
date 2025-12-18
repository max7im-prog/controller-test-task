#include "data_packet.h"
#include "virtual_serial.h"
#include "gtest/gtest.h"

TEST(BasicTest, SanityCheck) {
  EXPECT_EQ(true, true);
  EXPECT_NE(true, false);
}

TEST(VirtualSerialTest, EmptyReadRetunsFalse) {
  VirtualSerial link;
  std::vector<uint8_t> out;
  EXPECT_EQ(link.readA(out), false);
  EXPECT_EQ(link.readB(out), false);
}

TEST(VirtualSerialTest, SendAToB) {
  VirtualSerial link;
  std::vector<uint8_t> out;
  link.sendAtoB({1});
  link.sendAtoB({2});
  link.sendAtoB({3});

  EXPECT_EQ(link.readB(out), true);
  EXPECT_EQ(out[0], 1);
  EXPECT_EQ(link.readB(out), true);
  EXPECT_EQ(out[0], 2);
  EXPECT_EQ(link.readB(out), true);
  EXPECT_EQ(out[0], 3);
}

TEST(VirtualSerialTest, SendBToA) {
  VirtualSerial link;
  std::vector<uint8_t> out;
  link.sendBtoA({1});
  link.sendBtoA({2});
  link.sendBtoA({3});

  EXPECT_EQ(link.readA(out), true);
  EXPECT_EQ(out[0], 1);
  EXPECT_EQ(link.readA(out), true);
  EXPECT_EQ(out[0], 2);
  EXPECT_EQ(link.readA(out), true);
  EXPECT_EQ(out[0], 3);
}

TEST(VirtualSerialTest, WaitUnblocksOnSendAToB) {
  VirtualSerial link;
  std::atomic<bool> woke{false};

  std::thread t([&] {
    bool ok = link.waitAToB();
    woke = ok;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  link.sendAtoB({1});

  t.join();
  EXPECT_TRUE(woke);
}

TEST(VirtualSerialTest, WaitUnblocksOnShutdownAToB) {
  VirtualSerial link;
  std::atomic<bool> exited{false};

  std::thread t([&] {
    bool ok = link.waitAToB();
    exited = !ok;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  link.shutdown();

  t.join();
  EXPECT_TRUE(exited);
}

TEST(VirtualSerialTest, WaitUnblocksOnSendBToA) {
  VirtualSerial link;
  std::atomic<bool> woke{false};

  std::thread t([&] {
    bool ok = link.waitBToA();
    woke = ok;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  link.sendBtoA({1});

  t.join();
  EXPECT_TRUE(woke);
}

TEST(VirtualSerialTest, WaitUnblocksOnShutdownBToA) {
  VirtualSerial link;
  std::atomic<bool> exited{false};

  std::thread t([&] {
    bool ok = link.waitBToA();
    exited = !ok;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  link.shutdown();

  t.join();
  EXPECT_TRUE(exited);
}

TEST(VirtualSerialTest, SendAToBWakesOnlyOneWaiter) {
  VirtualSerial link;

  constexpr int waiterCount = 5;
  std::atomic<int> wakeups{0};

  std::vector<std::thread> threads;
  threads.reserve(waiterCount);

  for (int i = 0; i < waiterCount; ++i) {
    threads.emplace_back([&]() {
      if (link.waitAToB()) {
        ++wakeups;
      }
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  link.sendAtoB({0x42});

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_EQ(wakeups.load(), 1);

  link.shutdown();

  for (auto &t : threads) {
    t.join();
  }
}

TEST(VirtualSerialTest, SendBToAWakesOnlyOneWaiter) {
  VirtualSerial link;

  constexpr int waiterCount = 5;
  std::atomic<int> wakeups{0};

  std::vector<std::thread> threads;

  for (int i = 0; i < waiterCount; ++i) {
    threads.emplace_back([&]() {
      if (link.waitBToA()) {
        ++wakeups;
      }
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  link.sendBtoA({0x24});

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_EQ(wakeups.load(), 1);

  link.shutdown();

  for (auto &t : threads) {
    t.join();
  }
}

TEST(DataPacketTest, SerializeDeserializeRoundtrip) {
    DataPacket p;
    p._command_id = DataPacket::MSG_PWM;
    p._payload = {std::byte{0x34}, std::byte{0x12}};
    p._crc16 = DataPacket::generateCRC16(p);

    std::vector<uint8_t> raw;
    ASSERT_TRUE(p.toData(raw));

    DataPacket q;
    ASSERT_TRUE(q.fromData(raw));

    EXPECT_EQ(q._command_id, p._command_id);
    EXPECT_EQ(q._payload, p._payload);
    EXPECT_TRUE(DataPacket::checkCRC16(q, q._crc16));
}


TEST(DataPacketTest, RejectsTooShortPacket) {
    DataPacket p;
    std::vector<uint8_t> raw{0xAA, 1};

    EXPECT_FALSE(p.fromData(raw));
}

TEST(DataPacketTest, RejectsPayloadLengthMismatch) {
    DataPacket p;

    std::vector<uint8_t> raw = {
        0xAA, 1, 0x10, 5, // payload_len = 5
        0x01, 0x02,      // only 2 bytes
        0x00, 0x00
    };

    EXPECT_FALSE(p.fromData(raw));
}

TEST(DataPacketTest, CRCDetectsCorruption) {
    DataPacket p;
    p._command_id = DataPacket::MSG_STATUS;
    p._crc16 = DataPacket::generateCRC16(p);

    std::vector<uint8_t> raw;
    EXPECT_TRUE(p.toData(raw));

    raw[2] ^= 0xFF; // corrupt command_id

    DataPacket q;
    ASSERT_TRUE(q.fromData(raw));
    EXPECT_FALSE(DataPacket::checkCRC16(q, q._crc16));
}

TEST(DataPacketTest, FromDataFailsOnWrongLen){
    DataPacket p;
    p._command_id = DataPacket::MSG_STATUS;
    p._crc16 = DataPacket::generateCRC16(p);

    std::vector<uint8_t> raw;
    EXPECT_TRUE(p.toData(raw));

    raw[3] ^= 0xFF; // corrupt payload_len

    DataPacket q;
    ASSERT_FALSE(q.fromData(raw));
}


