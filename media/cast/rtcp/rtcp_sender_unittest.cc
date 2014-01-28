// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/rtcp_sender.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_task_runner.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/pacing/paced_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

namespace {
static const uint32 kSendingSsrc = 0x12345678;
static const uint32 kMediaSsrc = 0x87654321;
static const std::string kCName("test@10.1.1.1");
}  // namespace

class TestRtcpTransport : public transport::PacedPacketSender {
 public:
  TestRtcpTransport() : packet_count_(0) {
  }

  virtual bool SendRtcpPacket(const Packet& packet) OVERRIDE {
    EXPECT_EQ(expected_packet_.size(), packet.size());
    EXPECT_EQ(0, memcmp(expected_packet_.data(), packet.data(), packet.size()));
    packet_count_++;
    return true;
  }

  virtual bool SendPackets(const PacketList& packets) OVERRIDE {
    return false;
  }

  virtual bool ResendPackets(const PacketList& packets) OVERRIDE {
    return false;
  }

  void SetExpectedRtcpPacket(scoped_ptr<Packet> packet) {
    expected_packet_.swap(*packet);
  }

  int packet_count() const { return packet_count_; }

 private:
  Packet expected_packet_;
  int packet_count_;
};

class RtcpSenderTest : public ::testing::Test {
 protected:
  RtcpSenderTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_, task_runner_, task_runner_, task_runner_,
            task_runner_, task_runner_, GetDefaultCastSenderLoggingConfig())),
        rtcp_sender_(new RtcpSender(cast_environment_,
                                    &test_transport_,
                                    kSendingSsrc,
                                    kCName)) {
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  TestRtcpTransport test_transport_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<RtcpSender> rtcp_sender_;
};

TEST_F(RtcpSenderTest, RtcpReceiverReport) {
  // Empty receiver report + c_name.
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSendingSsrc, 0);
  p1.AddSdesCname(kSendingSsrc, kCName);
  test_transport_.SetExpectedRtcpPacket(p1.GetPacket());

  rtcp_sender_->SendRtcpFromRtpReceiver(RtcpSender::kRtcpRr,
      NULL, NULL, NULL, NULL);

  EXPECT_EQ(1, test_transport_.packet_count());

  // Receiver report with report block + c_name.
  TestRtcpPacketBuilder p2;
  p2.AddRr(kSendingSsrc, 1);
  p2.AddRb(kMediaSsrc);
  p2.AddSdesCname(kSendingSsrc, kCName);
  test_transport_.SetExpectedRtcpPacket(p2.GetPacket().Pass());

  transport::RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  rtcp_sender_->SendRtcpFromRtpReceiver(RtcpSender::kRtcpRr, &report_block,
                                        NULL, NULL, NULL);

  EXPECT_EQ(2, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithRrtr) {
  // Receiver report with report block + c_name.
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  transport::RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr,
      &report_block,
      &rrtr,
      NULL,
      NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithCast) {
  // Receiver report with report block + c_name.
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddCast(kSendingSsrc, kMediaSsrc);
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  transport::RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets_[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[kFrameIdWithLostPackets] =
      missing_packets;

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpCast,
      &report_block,
      NULL,
      &cast_message,
      NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithRrtraAndCastMessage) {
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  p.AddCast(kSendingSsrc, kMediaSsrc);
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  transport::RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets_[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[kFrameIdWithLostPackets] =
      missing_packets;

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr | RtcpSender::kRtcpCast,
      &report_block,
      &rrtr,
      &cast_message,
      NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithRrtrCastMessageAndLog) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;
  static const uint32 kDelayDeltaMs = 123;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  p.AddCast(kSendingSsrc, kMediaSsrc);
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  transport::RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets_[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[kFrameIdWithLostPackets] =
      missing_packets;

  // Test empty Log message.
  RtcpReceiverLogMessage receiver_log;

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr | RtcpSender::kRtcpCast |
      RtcpSender::kRtcpReceiverLog,
      &report_block,
      &rrtr,
      &cast_message,
      &receiver_log);


  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);
  p.AddReceiverFrameLog(kRtpTimestamp, 2, kTimeBaseMs);
  p.AddReceiverEventLog(kDelayDeltaMs, 5, 0);
  p.AddReceiverEventLog(kLostPacketId1, 8, kTimeDelayMs);

  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  RtcpReceiverFrameLogMessage frame_log(kRtpTimestamp);
  RtcpReceiverEventLogMessage event_log;

  event_log.type = kVideoAckSent;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
  frame_log.event_log_messages_.push_back(event_log);

  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  event_log.type = kVideoPacketReceived;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.packet_id = kLostPacketId1;
  frame_log.event_log_messages_.push_back(event_log);

  receiver_log.push_back(frame_log);

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr | RtcpSender::kRtcpCast |
      RtcpSender::kRtcpReceiverLog,
      &report_block,
      &rrtr,
      &cast_message,
      &receiver_log);

  EXPECT_TRUE(receiver_log.empty());
  EXPECT_EQ(2, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithOversizedFrameLog) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;
  static const uint32 kDelayDeltaMs = 123;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);

  transport::RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  p.AddReceiverFrameLog(kRtpTimestamp, 1, kTimeBaseMs);
  p.AddReceiverEventLog(kDelayDeltaMs, 5, 0);
  p.AddReceiverFrameLog(kRtpTimestamp + 2345,
      kRtcpMaxReceiverLogMessages, kTimeBaseMs);

  for (size_t i = 0; i < kRtcpMaxReceiverLogMessages; ++i) {
    p.AddReceiverEventLog(
        kLostPacketId1, 8, static_cast<uint16>(kTimeDelayMs * i));
  }

  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  RtcpReceiverFrameLogMessage frame_1_log(kRtpTimestamp);
  RtcpReceiverEventLogMessage event_log;

  event_log.type = kVideoAckSent;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
  frame_1_log.event_log_messages_.push_back(event_log);

  RtcpReceiverLogMessage receiver_log;
  receiver_log.push_back(frame_1_log);

  RtcpReceiverFrameLogMessage frame_2_log(kRtpTimestamp + 2345);

  for (int j = 0; j < 300; ++j) {
    event_log.type = kVideoPacketReceived;
    event_log.event_timestamp = testing_clock.NowTicks();
    event_log.packet_id = kLostPacketId1;
    frame_2_log.event_log_messages_.push_back(event_log);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }
  receiver_log.push_back(frame_2_log);

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpReceiverLog,
      &report_block,
      NULL,
      NULL,
      &receiver_log);

  EXPECT_EQ(1, test_transport_.packet_count());
  EXPECT_EQ(1u, receiver_log.size());
  EXPECT_EQ(300u - kRtcpMaxReceiverLogMessages,
            receiver_log.front().event_log_messages_.size());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithTooManyLogFrames) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;
  static const uint32 kDelayDeltaMs = 123;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);

  transport::RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  for (int i = 0; i < 119; ++i) {
    p.AddReceiverFrameLog(kRtpTimestamp, 1, kTimeBaseMs +  i * kTimeDelayMs);
    p.AddReceiverEventLog(kDelayDeltaMs, 5, 0);
  }
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  RtcpReceiverLogMessage receiver_log;

  for (int j = 0; j < 200; ++j) {
    RtcpReceiverFrameLogMessage frame_log(kRtpTimestamp);
    RtcpReceiverEventLogMessage event_log;

    event_log.type = kVideoAckSent;
    event_log.event_timestamp = testing_clock.NowTicks();
    event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
    frame_log.event_log_messages_.push_back(event_log);
    receiver_log.push_back(frame_log);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }
  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpReceiverLog,
      &report_block,
      NULL,
      NULL,
      &receiver_log);

  EXPECT_EQ(1, test_transport_.packet_count());
  EXPECT_EQ(81u, receiver_log.size());
}

}  // namespace cast
}  // namespace media
