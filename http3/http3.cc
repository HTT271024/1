#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include <queue>
#include <iostream>
#include <vector>
#include <sstream>
#include <map>
#include <string>
#include <deque>
#include <iomanip>
#include <set>
#include <numeric>
#include <algorithm>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HTTP3App");

// QUIC constants
static const uint64_t kQuicMssBytes = 1200; // simulated QUIC datagram payload size

// QUIC Frame Types
enum QuicFrameType { QF_STREAM, QF_ACK, QF_PING };

// HTTP/3 Frame Types (reusing H2-like app framing)
enum FrameType { HEADERS, DATA, PUSH_PROMISE };

// -------------------- QUIC Frame --------------------
struct QuicFrame {
  QuicFrameType type{QF_STREAM};
  uint32_t      streamId{0};
  uint64_t      offset{0};
  std::string   payload;
  bool          fin{false};

  std::string Serialize() const {
    std::ostringstream oss;
    oss << "TYPE:" << int(type)
        << "|SID:" << streamId
        << "|OFF:" << offset
        << "|FIN:" << (fin ? 1 : 0)
        << "|LEN:" << payload.size() << "|"
        << payload;
    return oss.str();
  }

  static QuicFrame Parse(const std::string& data) {
    QuicFrame frame;
    try {
      size_t pos = 0;

      size_t typeStart = data.find("TYPE:", pos);
      if (typeStart == std::string::npos) return frame;
      typeStart += 5;
      size_t typeEnd = data.find("|", typeStart);
      if (typeEnd == std::string::npos) return frame;
      frame.type = (QuicFrameType) std::stoi(data.substr(typeStart, typeEnd - typeStart));
      pos = typeEnd + 1;

      size_t sidStart = data.find("SID:", pos);
      if (sidStart == std::string::npos) return frame;
      sidStart += 4;
      size_t sidEnd = data.find("|", sidStart);
      if (sidEnd == std::string::npos) return frame;
      frame.streamId = std::stoul(data.substr(sidStart, sidEnd - sidStart));
      pos = sidEnd + 1;

      size_t offStart = data.find("OFF:", pos);
      if (offStart == std::string::npos) return frame;
      offStart += 4;
      size_t offEnd = data.find("|", offStart);
      if (offEnd == std::string::npos) return frame;
      frame.offset = std::stoull(data.substr(offStart, offEnd - offStart));
      pos = offEnd + 1;

      size_t finStart = data.find("FIN:", pos);
      if (finStart == std::string::npos) return frame;
      finStart += 4;
      size_t finEnd = data.find("|", finStart);
      if (finEnd == std::string::npos) return frame;
      frame.fin = (std::stoi(data.substr(finStart, finEnd - finStart)) == 1);
      pos = finEnd + 1;

      size_t lenStart = data.find("LEN:", pos);
      if (lenStart == std::string::npos) return frame;
      lenStart += 4;
      size_t lenEnd = data.find("|", lenStart);
      if (lenEnd == std::string::npos) return frame;
      uint32_t len = std::stoul(data.substr(lenStart, lenEnd - lenStart));
      pos = lenEnd + 1;

      if (pos + len <= data.size()) {
        frame.payload = data.substr(pos, len);
      }
    } catch (const std::exception& e) {
      NS_LOG_WARN("Failed to parse QUIC frame: " << e.what());
    }
    return frame;
  }
};

// -------------------- QUIC Packet --------------------
struct QuicPacket {
  uint64_t pktNum{0};
  std::vector<QuicFrame> frames;

  std::string Serialize() const {
    std::ostringstream oss;
    oss << "PKT:" << pktNum << "|FRAMES:" << frames.size() << "|";
    for (const auto& f : frames) {
      std::string s = f.Serialize();
      oss << "FLEN:" << s.size() << "|" << s; // length prefix per frame
    }
    return oss.str();
  }

  static QuicPacket Parse(const std::string& data) {
    QuicPacket packet;
    try {
      size_t pos = 0;

      size_t pktStart = data.find("PKT:", pos);
      if (pktStart == std::string::npos) return packet;
      pktStart += 4;
      size_t pktEnd = data.find("|", pktStart);
      if (pktEnd == std::string::npos) return packet;
      packet.pktNum = std::stoull(data.substr(pktStart, pktEnd - pktStart));
      pos = pktEnd + 1;

      size_t framesStart = data.find("FRAMES:", pos);
      if (framesStart == std::string::npos) return packet;
      framesStart += 7;
      size_t framesEnd = data.find("|", framesStart);
      if (framesEnd == std::string::npos) return packet;
      uint32_t frameCount = std::stoul(data.substr(framesStart, framesEnd - framesStart));
      pos = framesEnd + 1;

      for (uint32_t i = 0; i < frameCount; ++i) {
        size_t flenPos = data.find("FLEN:", pos);
        if (flenPos == std::string::npos) return packet;
        flenPos += 5;
        size_t flenEnd = data.find("|", flenPos);
        if (flenEnd == std::string::npos) return packet;
        uint32_t flen = std::stoul(data.substr(flenPos, flenEnd - flenPos));
        size_t frameStart = flenEnd + 1;
        if (frameStart + flen > data.size()) return packet;

        std::string frameData = data.substr(frameStart, flen);
        packet.frames.push_back(QuicFrame::Parse(frameData));
        pos = frameStart + flen;
      }
    } catch (const std::exception& e) {
      NS_LOG_WARN("Failed to parse QUIC packet: " << e.what());
    }
    return packet;
  }
};

// -------------------- HTTP/3 App Frame --------------------
struct HTTP3Frame {
  uint32_t  streamId{};
  FrameType type{};
  uint32_t  length{};
  uint64_t  offset{0};  // 新增：QUIC STREAM帧的offset字段
  std::string payload;

  std::string Serialize() const {
    std::ostringstream oss;
    oss << "SID:" << streamId
        << "|TYPE:" << int(type)
        << "|LEN:" << length
        << "|OFF:" << offset << "|"
        << payload;
    return oss.str();
  }

  static HTTP3Frame Parse(const std::string& data) {
    HTTP3Frame frame;
    size_t pos = 0;
    try {
      if (data.substr(pos, 4) == "SID:") {
        pos += 4;
        size_t end = data.find('|', pos);
        if (end != std::string::npos) {
          frame.streamId = std::stoi(data.substr(pos, end - pos));
          pos = end + 1;
        }
      }
      if (pos < data.size() && data.substr(pos, 5) == "TYPE:") {
        pos += 5;
        size_t end = data.find('|', pos);
        if (end != std::string::npos) {
          frame.type = (FrameType) std::stoi(data.substr(pos, end - pos));
          pos = end + 1;
        }
      }
      if (pos < data.size() && data.substr(pos, 4) == "LEN:") {
        pos += 4;
        size_t end = data.find('|', pos);
        if (end != std::string::npos) {
          frame.length = std::stoi(data.substr(pos, end - pos));
          pos = end + 1;
        }
      }
      if (pos < data.size() && data.substr(pos, 4) == "OFF:") {
        pos += 4;
        size_t end = data.find('|', pos);
        if (end != std::string::npos) {
          frame.offset = std::stoull(data.substr(pos, end - pos));
          pos = end + 1;
        }
      }
      if (pos < data.size()) {
        if (pos + frame.length <= data.size()) {
          frame.payload = data.substr(pos, frame.length);
        } else {
          // 容错：不足LEN，取剩余，后续逻辑会告警
        frame.payload = data.substr(pos);
        }
      }
    } catch (const std::exception& e) {
      NS_LOG_WARN("Frame parsing error: " << e.what() << " for data: " << data);
    }
    return frame;
  }
};

// -------------------- Pending Item --------------------
struct PendingItem {
  uint32_t streamId;
  uint32_t remainingBytes;
  uint32_t totalBytes;
  uint32_t sentBytes;   // 新增：严格核对已发送字节数
  uint32_t tickCount;  // 跟踪该流被处理的次数
  PendingItem(uint32_t sid, uint32_t total) : streamId(sid), remainingBytes(total), totalBytes(total), sentBytes(0), tickCount(0) {}
};

// -------------------- Globals --------------------
static std::vector<uint32_t> g_respSizes;
static uint64_t g_retxCount = 0;

// -------------------- QUIC Session --------------------
class QuicSession : public Object {
public:
  QuicSession(Ptr<Socket> udp, bool quiet = false)
  : m_udp(udp), m_nextPktNum(1), m_mtu(1200), m_largestToAck(0), m_largestAcked(0), m_quiet(quiet) {
    m_udp->SetRecvCallback(MakeCallback(&QuicSession::OnUdpRecv, this));
    
    // 初始化拥塞控制参数
    // ★ 关键修复：将初始CWND从60降到10 ★
    m_cwnd = 10 * kQuicMssBytes;      // 从一个更合理的值开始慢启动
    m_ssthresh = UINT64_MAX;
    m_srtt = MilliSeconds(0);
    m_rttvar = MilliSeconds(0);
    m_rto = MilliSeconds(80);        // 降低初始RTO，加速调试 (100ms -> 80ms)
    m_bytesInFlight = 0;
    m_lastLossTs = Seconds(0);
    
    // 初始化流控窗口
    m_connWindowBytes = 256 * 1024 * 1024;  // 连接窗口 256MB
    
    // 初始化ACK相关
    m_ackDelay = MilliSeconds(1);     // 最小化ACK延迟 (2ms -> 1ms)
  }

  // 估算打包后的UDP负载大小（不含IP/UDP头）
  uint32_t EstimatePacketSize(const std::vector<QuicFrame>& frames) const {
    QuicPacket p; p.pktNum = 0; p.frames = frames;
    std::string s = p.Serialize();
    return static_cast<uint32_t>(s.size());
  }

  // 供应用层查询的拥塞控制/RTT信息
  uint64_t BytesInFlight() const { return m_bytesInFlight; }
  uint64_t CwndBytes() const { return m_cwnd; }
  Time Srtt() const { return m_srtt; }
  
  // 新增：获取发送步调延迟的函数
  Time GetPacingDelay(uint32_t packetSize) const {
    // 如果还没有SRTT估算或CWND为0，返回一个很小的默认延迟，避免除以0
    if (m_srtt == MilliSeconds(0) || m_cwnd == 0) {
      return MilliSeconds(1); 
    }

    // 步调速率 (bytes/sec) = 拥塞窗口 / RTT
    // 加一个很小的数防止除以零
    double pacingRate = static_cast<double>(m_cwnd) / (m_srtt.GetSeconds() + 1e-9);
    
    // 如果速率过低，也使用一个最小延迟
    if (pacingRate < 1.0) {
        return MilliSeconds(1);
    }
    
    // 下一个包的发送延迟 (sec) = 包大小 / 步调速率
    double delaySeconds = static_cast<double>(packetSize) / pacingRate;
    
    return Seconds(delaySeconds);
  }

  // 注册ACK唤醒回调
  void SetWakeupCallback(Callback<void> cb) { m_wakeupCb = cb; }

  void SendFrames(const std::vector<QuicFrame>& batch) {
    std::vector<QuicFrame> currentBatch;
    size_t currentSize = 0;
    
    // UDP/IP头开销估算
    const size_t udpHeaderSize = 8;  // UDP头8字节
    const size_t ipHeaderSize = 20;  // IPv4头20字节
    const size_t totalHeaderOverhead = udpHeaderSize + ipHeaderSize;
    
    // 实际可用MTU = 1200 - 头开销
    const size_t effectiveMtu = m_mtu - totalHeaderOverhead;
    
    for (const auto& frame : batch) {
      std::string s = frame.Serialize();
      // 为了更贴近真实大小，建议把开销算进去
      size_t perFrameOverhead = 6 /*"FLEN:"*/ + 1 /*"|"*/ + std::to_string(s.size()).size();
      size_t thisSize = perFrameOverhead + s.size();
      
      if (currentSize + thisSize > effectiveMtu && !currentBatch.empty()) {
        SendPacket(currentBatch);
        currentBatch.clear();
        currentSize = 0;
      }
      currentBatch.push_back(frame);
      currentSize += thisSize;
    }
    if (!currentBatch.empty()) SendPacket(currentBatch);
  }

  void OnUdpRecv(Ptr<Socket> s) {
    Address from;
    Ptr<Packet> packet;
    while ((packet = s->RecvFrom(from))) {
      if (m_peer == Address()) m_peer = from;

      std::string data;
      data.resize(packet->GetSize());
      packet->CopyData(reinterpret_cast<uint8_t*>(&data[0]), packet->GetSize());

      QuicPacket qp = QuicPacket::Parse(data);
      ProcessPacket(qp);
    }
  }

  void OpenStream(uint32_t sid) {
    m_streams[sid] = true;
    m_streamOffsets[sid] = 0;
  }

  void SendStreamData(uint32_t sid, const uint8_t* buf, uint32_t len, bool fin) {
    QuicFrame f;
    f.type = QF_STREAM;
    f.streamId = sid;
    f.offset = m_streamOffsets[sid];
    f.payload = std::string(reinterpret_cast<const char*>(buf), len);
    f.fin = fin;

    // 记录发送FIN的情况
    if (f.fin) std::cout << "[QUIC] SEND FIN sid=" << f.streamId << " pkt=" << m_nextPktNum << std::endl;

    SendFrames({f});
    m_streamOffsets[sid] += len;
  }

  void SetStreamDataCallback(Callback<void, uint32_t, const uint8_t*, uint32_t, bool> cb) {
    m_onStreamData = cb;
  }

private:
  void SendPacket(const std::vector<QuicFrame>& frames, bool isRetransmission = false) {
    // 判断是否 ACK-only 包
    bool ackOnly = true;
    for (const auto &f : frames) { if (f.type != QF_ACK) { ackOnly = false; break; } }

    QuicPacket p;
    p.pktNum = m_nextPktNum++;
    p.frames = frames;

    std::string s = p.Serialize();
    uint32_t sz = s.size();
    
    // ★ 关键修复 ★
    // 仅对非重传、非ACK-only的包进行拥塞控制检查
    if (!ackOnly && !isRetransmission) {
      if (!CanSend(sz)) {
        // 限频打印
        static Time lastLog = Seconds(0);
        Time now = Simulator::Now();
        if ((now - lastLog) >= MilliSeconds(1)) {
          std::cout << "[QUIC] Congestion control blocked: cwnd=" << m_cwnd 
                    << " bytesInFlight=" << m_bytesInFlight << " need=" << sz << std::endl;
          lastLog = now;
        }
        // 将帧重新排队，RTT驱动退避
        Time backoff = std::max(MilliSeconds(1), m_srtt > MilliSeconds(0) ? m_srtt/4 : MilliSeconds(2));
        Simulator::Schedule(backoff, &QuicSession::SendFrames, this, frames);
        return;
      }
    }
    
    // 流控检查（对STREAM帧）；ACK-only 不需检查
    if (!ackOnly) {
      for (const auto& frame : frames) {
        if (frame.type == QF_STREAM) {
          if (!CanSendStreamData(frame.streamId, sz)) {
            // 流控阻塞，重新排队
            Time backoff = std::max(MilliSeconds(1), m_srtt > MilliSeconds(0) ? m_srtt/4 : MilliSeconds(2));
            Simulator::Schedule(backoff, &QuicSession::SendFrames, this, frames);
            return;
          }
          break;  // 只需要检查第一个STREAM帧
        }
      }
    }
    
    // 只有非 ACK-only 才入未确认表并计入 in-flight
    if (!ackOnly) {
      m_unacked[p.pktNum] = {p, Simulator::Now(), sz};
      m_bytesInFlight += sz;
      // 启动RTO定时器
      ArmRto();
      ArmPto(); // ★ 新增：启动PTO定时器 ★
    }
    
    Ptr<Packet> udpPkt = Create<Packet>(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    if (!(m_peer == Address())) m_udp->SendTo(udpPkt, 0, m_peer);
    else                        m_udp->Send(udpPkt);
    
    // 添加调试信息（仅对含数据的包）
    if (!ackOnly && !m_quiet) {
      for (const auto& f : frames) {
        if (f.type == QF_STREAM) {
          std::cout << "[QUIC] Sent packet " << p.pktNum << " with STREAM frame for stream " 
                    << f.streamId << " size=" << f.payload.size() << " fin=" << f.fin 
                    << " (bytesInFlight=" << m_bytesInFlight << ")" << std::endl;
        }
      }
    }
  }

  void ProcessPacket(const QuicPacket& packet) {
    bool ackEliciting = false;
    
    // 记录收到的包号
    m_recvPkts.insert(packet.pktNum);
    
    for (const auto& f : packet.frames) {
      if (f.type != QF_ACK) ackEliciting = true;

      if (f.type == QF_STREAM) {
        if (!m_quiet) {
          std::cout << "[QUIC] Received packet " << packet.pktNum << " with STREAM frame for stream " 
                  << f.streamId << " size=" << f.payload.size() << " fin=" << f.fin << std::endl;
        }
        // 记录收到FIN的情况
        if (f.fin) {
          std::cout << "[QUIC] Received FIN for stream " << f.streamId << " in packet " << packet.pktNum << std::endl;
        }
        if (!m_onStreamData.IsNull()) {
          m_onStreamData(f.streamId,
                         reinterpret_cast<const uint8_t*>(f.payload.data()),
                         f.payload.size(),
                         f.fin);
        }
      } else if (f.type == QF_ACK) {
        OnAckReceived(f.offset, f.payload);
      }
    }

    if (ackEliciting) {
      m_largestToAck = std::max(m_largestToAck, packet.pktNum);
      // 前16个包立即ACK，其余按m_ackDelay
      if (m_recvPkts.size() <= 16) {
        FlushAck();
      } else {
        // 根据RTT动态调整ACK延迟：clamp to [2ms, 10ms]
        Time t = MilliSeconds(3);
        if (m_srtt > MilliSeconds(0)) {
          t = std::min(std::max(m_srtt / 6, MilliSeconds(2)), MilliSeconds(6));
        }
        if (!m_ackTimer.IsPending()) {
          m_ackTimer = Simulator::Schedule(t, &QuicSession::FlushAck, this);
        }
      }
    }
  }

  void FlushAck() {
    if (m_recvPkts.empty()) return;
    // Build ACK ranges bitmap for last up to 64 packets
    struct AckInfo { uint64_t largest; uint64_t mask; };
    auto build = [&]() -> AckInfo {
      AckInfo a{0,0};
      if (m_recvPkts.empty()) return a;
      a.largest = *m_recvPkts.rbegin();
      uint64_t L = a.largest;
      for (int i = 1; i <= 64; ++i) {
        if (L < static_cast<uint64_t>(i)) break;
        uint64_t pn = L - i;
        if (m_recvPkts.count(pn)) a.mask |= (1ULL << (i - 1));
      }
      return a;
    };
    AckInfo info = build();
    QuicFrame ack; ack.type = QF_ACK; ack.offset = info.largest; ack.payload = ""; // 累计ACK
    SendFrames({ack});
  }
 
  void OnAckReceived(uint64_t largest, const std::string& payloadMaskStr) {
    // Parse mask (支持累计ACK：空payload表示累计ACK)
    uint64_t mask = 0;
    bool cumulativeAck = payloadMaskStr.empty();
    if (!cumulativeAck) {
    try { mask = std::stoull(payloadMaskStr); } catch (...) { mask = 0; }
    }

    // RTT from largest if present
    auto itLargest = m_unacked.find(largest);
    if (itLargest != m_unacked.end()) {
      Time rtt = Simulator::Now() - itLargest->second.sent;
      if (m_srtt == MilliSeconds(0)) { m_srtt = rtt; m_rttvar = rtt / 2; }
      else { Time diff = (rtt > m_srtt) ? (rtt - m_srtt) : (m_srtt - rtt); m_rttvar = (3 * m_rttvar + diff) / 4; m_srtt = (7 * m_srtt + rtt) / 8; }
      m_rto = std::max(m_srtt + 4 * m_rttvar, MilliSeconds(100));
      if (!m_quiet) {
        std::cout << "[QUIC] RTT update: " << rtt.GetMilliSeconds() << "ms, SRTT: "
                  << m_srtt.GetMilliSeconds() << "ms, RTO: " << m_rto.GetMilliSeconds() << "ms" << std::endl;
      }
    }

    // Track largest acked for loss heuristics
    if (largest > m_largestAcked) m_largestAcked = largest;

    // --- replace ACK handling with this block ---
    std::vector<uint64_t> acked;
    if (cumulativeAck) {
      for (auto &kv : m_unacked) if (kv.first <= largest) acked.push_back(kv.first);
    } else {
      acked.push_back(largest);
      for (int i = 1; i <= 64; ++i) {
        if (mask & (1ULL << (i - 1))) {
          if (largest >= static_cast<uint64_t>(i)) acked.push_back(largest - i);
        }
      }
    }

    // 先统计被确认的字节数（在删除前）
    uint64_t bytesAcked = 0;
    for (uint64_t pn : acked) {
      auto it = m_unacked.find(pn);
      if (it != m_unacked.end()) bytesAcked += it->second.size;
    }

    // 然后再删除这些已确认包并从 bytesInFlight 扣除
    for (uint64_t pn : acked) {
      auto it = m_unacked.find(pn);
      if (it != m_unacked.end()) {
        m_bytesInFlight = (m_bytesInFlight >= it->second.size ? m_bytesInFlight - it->second.size : 0);
        m_unacked.erase(it);
      }
    }

    // Congestion control
    // ★ 使用新逻辑更新 CWND ★
    if (bytesAcked > 0) { // 只有在确认了新数据时才增加窗口
        if (m_cwnd < m_ssthresh) {
            // 慢启动阶段：CWND 增加量等于确认的数据量
            m_cwnd += bytesAcked;
        } else {
            // 拥塞避免阶段：每个RTT大约增加一个MSS
            // (kQuicMssBytes * kQuicMssBytes) / m_cwnd 是一个标准的近似算法
            // ★ 关键修复：保证增量最少为1，防止整数除法结果为0 ★
            uint64_t increment = (kQuicMssBytes * kQuicMssBytes) / (m_cwnd ? m_cwnd : 1);
            m_cwnd += std::max((uint64_t)1, increment);
        }
    }
    // 放宽/移除上限保护，避免过早封顶（如需可设更高上限）
    // const uint64_t kCwndCap = 256 * kQuicMssBytes;
    // if (m_cwnd > kCwndCap) m_cwnd = kCwndCap;
    // 更详细的ACK日志，包括bytesAcked和重传计数
    std::cout << "[QUIC] ACK largest=" << largest << " bytesAcked=" << bytesAcked
              << " cwnd=" << m_cwnd << " inflight=" << m_bytesInFlight << " retx=" << g_retxCount << std::endl;

    // Loss detection by packet threshold (3) with time threshold
    {
      std::vector<uint64_t> toRetx;
      toRetx.reserve(m_unacked.size());
      Time now = Simulator::Now();
      // 回到更接近 RFC9002 的丢包阈值
      const int kPacketThresh = 3; // 收紧丢包探测                  // 16 -> 3
      Time timeThresh = (m_srtt > MilliSeconds(0))
                        ? std::max(m_srtt*2, MilliSeconds(30)) : MilliSeconds(120);
      for (const auto &kv : m_unacked) {
        uint64_t pn = kv.first;
        const OutPkt &op = kv.second;
        bool pktThresh = (pn + kPacketThresh <= m_largestAcked);
        bool timeOld   = (now - op.sent) >= timeThresh;
        if (pktThresh && timeOld) {
          toRetx.push_back(pn);
        }
      }
      for (uint64_t pn : toRetx) {
        auto itCheck = m_unacked.find(pn);
        if (itCheck != m_unacked.end()) {
          std::cout << "[QUIC] Loss pn=" << pn << " -> retransmit as new" << std::endl;
          Retransmit(pn);
        }
      }
      // 拥塞响应：每个RTT最多降低一次，避免连环收缩
      if (!toRetx.empty() && (m_srtt == MilliSeconds(0) || (Simulator::Now() - m_lastLossTs) >= m_srtt)) {
        // ★ 关键修复 1：设置一个合理的最小窗口下限 (4个MSS)，而不是32个 ★
        uint64_t floor = 4 * kQuicMssBytes;
        
        // ★ 关键修复 2：将窗口减半（标准的乘法减小），这比 *7/8 更稳健 ★
        uint64_t newSsthresh = std::max<uint64_t>(m_cwnd / 2, floor);
        
        m_ssthresh = newSsthresh;
        m_cwnd = newSsthresh; // 进入拥塞避免阶段，CWND通常被重置为ssthresh
        m_lastLossTs = Simulator::Now();
    }
    }

    if (!m_unacked.empty()) {
      ArmRto();
      ArmPto(); // ★ 新增：重置PTO定时器 ★
    } else {
      if (m_retxTimer.IsPending()) m_retxTimer.Cancel();
      if (m_ptoTimer.IsPending()) m_ptoTimer.Cancel(); // ★ 新增：取消PTO定时器 ★
    }
    // ACK驱动：收到ACK后尝试唤醒发送方
    if (!m_wakeupCb.IsNull()) m_wakeupCb();
  }

  Ptr<Socket> m_udp;
  Address m_peer;
  uint64_t m_nextPktNum;
  uint32_t m_mtu;
  std::map<uint32_t, bool> m_streams;
  std::map<uint32_t, uint64_t> m_streamOffsets;
  std::map<uint64_t, std::pair<QuicPacket, Time>> m_unackedPackets;
  Callback<void, uint32_t, const uint8_t*, uint32_t, bool> m_onStreamData;

  uint64_t m_largestToAck;
  uint64_t m_largestAcked;
  EventId  m_ackTimer;

  // ACK相关成员
  std::set<uint64_t> m_recvPkts;  // 记录收到的包号
  Time m_ackDelay;                 // ACK延迟时间
  
  // 拥塞控制相关成员
  uint64_t m_cwnd;           // 拥塞窗口
  uint64_t m_ssthresh;       // 慢启动阈值
  Time m_srtt;               // 平滑RTT
  Time m_rttvar;             // RTT变化
  Time m_rto;                // 重传超时
  uint64_t m_bytesInFlight;  // 在途字节数
  
  // 流控相关成员
  uint64_t m_connWindowBytes;      // 连接级流控窗口
  std::map<uint32_t, uint64_t> m_streamWindows;  // 流级流控窗口
  
  // 未确认包表
  struct OutPkt { 
    QuicPacket p; 
    Time sent; 
    uint32_t size; 
  };
  std::map<uint64_t, OutPkt> m_unacked;
  EventId m_retxTimer;
  EventId m_ptoTimer; // 新增：探测超时定时器

  // 发送唤醒回调
  Callback<void> m_wakeupCb;
  
  // 拥塞控制检查
  bool CanSend(uint32_t sz) { 
    return m_bytesInFlight + sz <= m_cwnd; 
  }
  
  // 流控检查
  bool CanSendStreamData(uint32_t streamId, uint32_t sz) {
    // 检查连接级流控
    if (m_bytesInFlight + sz > m_connWindowBytes) {
      if (!m_quiet) {
        std::cout << "[QUIC] Connection flow control blocked: connWin=" << m_connWindowBytes 
                  << " bytesInFlight=" << m_bytesInFlight << " need=" << sz << std::endl;
      }
      return false;
    }
    
    // 检查流级流控
    auto it = m_streamWindows.find(streamId);
    if (it != m_streamWindows.end() && it->second < sz) {
      if (!m_quiet) {
        std::cout << "[QUIC] Stream flow control blocked: sid=" << streamId 
                  << " streamWin=" << it->second << " need=" << sz << std::endl;
      }
      return false;
    }
    
    return true;
  }
  
  // 设置流窗口
  void SetStreamWindow(uint32_t streamId, uint64_t windowBytes) {
    m_streamWindows[streamId] = windowBytes;
  }
  
  // 更新连接窗口
  void UpdateConnWindow(uint64_t windowBytes) {
    m_connWindowBytes = windowBytes;
  }
  
  // 重传处理
  void Retransmit(uint64_t pktNum) {
    // 检测重复重传
    static std::set<uint64_t> retransmitted;
    if (retransmitted.count(pktNum)) {
      std::cout << "[WARN] Duplicate retransmission of packet " << pktNum << std::endl;
      return;
    }
    retransmitted.insert(pktNum);
    auto it = m_unacked.find(pktNum);
    if (it == m_unacked.end()) return;

    // Take frames of lost packet
    auto frames = it->second.p.frames;
    uint32_t sz = it->second.size;

    // Remove old packet from inflight and table
    m_bytesInFlight = (m_bytesInFlight >= sz ? m_bytesInFlight - sz : 0);
    m_unacked.erase(it);

    // Send as a new packet (will assign new pktNum and re-add to unacked)
    g_retxCount++; // 确保重传计数增加
    if (!m_quiet) {
      std::cout << "[QUIC] Retransmitting packet " << pktNum << " (total retx: " << g_retxCount << ")" << std::endl;
    }
    // MODIFIED: 调用 SendPacket 时，传入 true 表示这是重传包
    SendPacket(frames, true);
  }
  
  void ArmRto() {
    if (m_unacked.empty()) { if (m_retxTimer.IsPending()) m_retxTimer.Cancel(); return; }
    if (!m_retxTimer.IsPending())
      m_retxTimer = Simulator::Schedule(m_rto, &QuicSession::OnRto, this);
  }
  
  // 新增：启动PTO定时器
  void ArmPto() {
      if (m_ptoTimer.IsPending()) m_ptoTimer.Cancel();
      // 如果有在途数据，则设置PTO
      if (m_bytesInFlight > 0) {
          // PTO 时间可以设置为 RTO 的一个倍数或一个固定值
          // 减小PTO延时，加速调试
          Time pto_delay = m_rto + MilliSeconds(20);
          m_ptoTimer = Simulator::Schedule(pto_delay, &QuicSession::OnPto, this);
      }
  }

  // 新增：PTO超时处理函数
  void OnPto() {
      if (m_bytesInFlight == 0) {
          return; // 没有在途数据，不需要探测
      }
      
      if (!m_quiet) {
        std::cout << "[QUIC] PTO Fired! Sending a PING frame to elicit ACK." << std::endl;
      }

      // 发送一个PING帧来探测网络
      QuicFrame ping;
      ping.type = QF_PING;
      ping.streamId = 0;
      ping.offset = 0;
      ping.payload = "";

      // 注意：PING帧也需要封装在QuicPacket里
      QuicPacket p;
      p.pktNum = 0; // PING包可以不消耗包序号
      p.frames.push_back(ping);

      std::string s = p.Serialize();
      Ptr<Packet> udpPkt = Create<Packet>(reinterpret_cast<const uint8_t*>(s.data()), s.size());
      if (!(m_peer == Address())) m_udp->SendTo(udpPkt, 0, m_peer);
      else m_udp->Send(udpPkt);

      // 强制快速重传最早未确认包，避免长时间等待
      if (!m_unacked.empty()) {
        uint64_t oldest = m_unacked.begin()->first;
        std::cout << "[QUIC] PTO -> force retransmit pkt " << oldest << std::endl;
        Retransmit(oldest);
      }

      // PTO超时后，进行指数退避并重新启动定时器
      ArmPto(); 
  }
  
  
  void OnRto() {
    if (m_unacked.empty()) return;
    
    // 超时重传最早的包
    auto it = m_unacked.begin();
    Retransmit(it->first);
    
    // 拥塞控制：只在"距离上次收缩 >= SRTT"时收缩一次，且别太狠
    if (m_srtt == MilliSeconds(0) || (Simulator::Now() - m_lastLossTs) >= m_srtt) {
      // ★ 关键修复 1：设置一个合理的最小窗口下限 (4个MSS) ★
      uint64_t floor = 4 * kQuicMssBytes;
      
      // ★ 关键修复 2：将窗口减半（标准的乘法减小）★
      uint64_t newSsthresh = std::max<uint64_t>(m_cwnd / 2, floor);
      
      m_ssthresh = newSsthresh;
      m_cwnd = newSsthresh;
      m_lastLossTs = Simulator::Now();
    }
    m_rto = std::min(Seconds(3), m_rto*2);
    
    // 重新启动RTO定时器
    ArmRto();
  }

  Time m_lastLossTs;  // 上次执行拥塞收缩的时间
  bool m_quiet{false};  // 添加安静模式标志
};

// -------------------- HTTP/3 Client --------------------
class Http3ClientApp : public Application {
public:
  Http3ClientApp() : m_socket(0), m_port(0) {}
  void Setup(Address servAddr, uint16_t port, uint32_t reqSize, uint32_t nReqs, double interval, bool thirdParty, uint32_t nStreams, bool quiet = false) {
    m_servAddr = servAddr; m_port = port; m_reqSize = reqSize; m_nReqs = nReqs;
    m_interval = interval; m_thirdParty = thirdParty; m_nStreams = nStreams;
    m_quiet = quiet;
  }

  uint32_t GetRespsRcvd() const { return m_respsRcvd; }
  const std::vector<double>& GetReqSendTimes() const { return m_reqSendTimes; }
  const std::vector<double>& GetRespRecvTimes() const { return m_respRecvTimes; }
  double GetInterval() const { return m_interval; }

  // push stats
  uint32_t GetPushStreams() const { return m_pushStreams; }
  uint32_t GetPushCompleted() const { return m_pushCompleted; }
  uint64_t GetTotalPushBytes() const { uint64_t tot=0; for (auto& kv: m_pushBytes) tot+=kv.second; return tot; }

  // 在Http3ClientApp类中添加VerifyCompletedStreams方法
  void VerifyCompletedStreams() const {
    for (auto const& [sid, is_completed] : m_streamCompleted) {
      if (is_completed) {
        auto targetIt = m_streamTargetBytes.find(sid);
        auto receivedBytes = BytesReceived(sid);
        if (targetIt != m_streamTargetBytes.end()) {
          uint64_t target = targetIt->second;
          if (target != receivedBytes) {
            std::cout << "[ERROR] Data Integrity Fail on Stream " << sid
                      << ": Expected " << target << ", Got " << receivedBytes << std::endl;
          }
        }
      }
    }
  }

private:
  void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_servAddr), m_port));
    m_session = CreateObject<QuicSession>(m_socket, m_quiet);  // 传递quiet参数
    m_session->SetStreamDataCallback(MakeCallback(&Http3ClientApp::OnStreamData, this));

    m_reqsSent = m_respsRcvd = 0;
    m_reqSendTimes.clear(); m_respRecvTimes.clear();
    m_rxBuf.clear(); m_streamBytes.clear(); m_streamTargetBytes.clear(); m_streamCompleted.clear();
    m_streamDataFrames.clear();  // 新增
    m_pushBytes.clear(); m_pushTargetBytes.clear(); m_pushCompleted=0; m_pushStreams=0;
    m_nextStreamId = 1;  // 从 1 开始递增（模拟即可，真实 QUIC 会用奇数）

    // 初始化流重组相关
    m_streamBytesReceived.clear();
    m_finalSize.clear();

    // 握手时延建模（模拟TLS1.3 1-RTT）
    // 从P2P延迟估算RTT，添加握手开销
    double estimatedRtt = 0.010;  // 默认10ms，实际应该从网络配置获取
    double handshakeDelay = estimatedRtt;  // 1-RTT握手
    
    if (!m_quiet) {
      std::cout << "[QUIC] Estimated RTT: " << (estimatedRtt * 1000) << "ms, handshake delay: " 
                << (handshakeDelay * 1000) << "ms" << std::endl;
    }
    
    // 延迟发送第一个请求，模拟握手过程
    Simulator::Schedule(Seconds(handshakeDelay), &Http3ClientApp::SendNextRequest, this);
  }

  void StopApplication() override { if (m_socket) m_socket->Close(); }

  // 原来的 SendNextRequest 改名为 StartRequests，在开始时调用
  void SendNextRequest() {
    uint32_t reqsToSend = std::min(m_nReqs - m_reqsSent, m_nStreams);
    for (uint32_t i = 0; i < reqsToSend; ++i) {
      SendSingleRequest();
    }
  }

  void OnStreamData(uint32_t streamId, const uint8_t* data, uint32_t len, bool fin) {
    // ① 先把数据追加到该流的专属缓冲
    std::string& buf = m_rxBuf[streamId];
    buf.append(reinterpret_cast<const char*>(data), len);
    if (!m_quiet) { // 已有
      std::cout << "[DEBUG] Stream " << streamId << " buffer size: " << buf.size() << " after adding " << len << " bytes" << std::endl;
    }

    // ② 在该流的缓冲里，用 LEN: 精确切帧
    size_t pos = 0;
    while (pos < buf.size()) {
      size_t frameStart = buf.find("SID:", pos);
      if (frameStart == std::string::npos) break;

      size_t lenStart = buf.find("LEN:", frameStart);
      if (lenStart == std::string::npos) break;

      size_t lenValueStart = lenStart + 4;
      size_t lenValueEnd = buf.find("|", lenValueStart);
      if (lenValueEnd == std::string::npos) break;

      uint32_t frameLen;
      try {
        frameLen = std::stoul(buf.substr(lenValueStart, lenValueEnd - lenValueStart));
      } catch (...) { break; }

      size_t payloadStart = lenValueEnd + 1;
      // 若存在 OFF: 字段，跳过 OFF:...|
      if (payloadStart + 4 <= buf.size() && buf.compare(payloadStart, 4, "OFF:") == 0) {
        size_t offEnd = buf.find('|', payloadStart + 4);
        if (offEnd == std::string::npos) break; // 等待更多数据
        payloadStart = offEnd + 1;
      }
      if (payloadStart + frameLen > buf.size()) break;  // 等待更多字节

      // 提取这一完整帧并处理
      std::string frameData = buf.substr(frameStart, payloadStart - frameStart + frameLen);
      
      // MODIFIED: Wrap the log
      if (!m_quiet) {
        std::cout << "[DEBUG] Parsing frame: start=" << frameStart 
                  << " payloadStart=" << payloadStart 
                  << " frameLen=" << frameLen 
                  << " actualSize=" << frameData.size() 
                  << " for stream " << streamId << std::endl;
      }
      ProcessFrame(streamId, frameData);

      pos = frameStart + frameData.size();
    }

    // ③ 丢掉已消费的前缀，留下不完整的尾巴
    if (pos > 0) {
      // MODIFIED: Wrap the log
      if (!m_quiet) {
        std::cout << "[DEBUG] Stream " << streamId << " removing " << pos << " bytes, remaining: " << (buf.size() - pos) << std::endl;
      }
      buf.erase(0, pos);
    }

    // ④ 收到该流的 QUIC FIN，表示流结束，需要检查完成状态
    if (fin) {
      if (m_streamTargetBytes.count(streamId)) {
        uint64_t have = BytesReceived(streamId);
        uint64_t need = m_streamTargetBytes[streamId];
        if (have < need) {
          std::cout << "[WARN] FIN before target on stream " << streamId
                    << " got=" << have
                    << " need=" << need << "\n";
        }
      }
      // MODIFIED: Wrap the log
      if (!m_quiet) {
        std::cout << "[DEBUG] Received FIN for stream " << streamId << std::endl;
      }
      CheckStreamCompletion(streamId);
    }
  }

  void ProcessFrame(uint32_t quicSid, const std::string& frameData) {
    try {
      HTTP3Frame f = HTTP3Frame::Parse(frameData);

      // 统一以 QUIC 层的流号为准（忽略帧内 SID）
      uint32_t sid = quicSid;

      // 仅用于调试：如果帧内 SID 与外层不一致，打印一下，便于排查
      if (f.streamId != 0 && f.streamId != quicSid) {
        std::cout << "[WARN] HTTP3 SID(" << f.streamId
                  << ") != QUIC SID(" << quicSid << "), using QUIC SID" << std::endl;
      }

      bool isPush = (sid >= 1000) || (f.payload.find("x-push: 1") != std::string::npos);

      if (f.type == HEADERS) {
        // MODIFIED: Wrap the log
        if (!m_quiet) {
          std::cout << "[DEBUG] Received HEADERS for stream " << sid << std::endl;
        }
        size_t p = f.payload.find("Content-Length: ");
        if (p != std::string::npos) {
          size_t e = f.payload.find("\r\n", p);
          uint32_t len = std::stoi(f.payload.substr(p + 16, e - (p + 16)));
          if (isPush) {
            m_pushTargetBytes[sid] = len; m_pushBytes[sid] = 0; ++m_pushStreams;
          } else {
            m_streamTargetBytes[sid] = len; m_streamBytes[sid] = 0;
            // MODIFIED: Wrap the log
            if (!m_quiet) {
              std::cout << "[DEBUG] Set target for stream " << sid << ": " << len << " bytes" << std::endl;
            }
          }
        } else {
          // 健壮性检查：若没解析到Content-Length，立刻报警
          std::cout << "[ERROR] No Content-Length in HEADERS (sid=" << sid << "), payload: " << f.payload << std::endl;
        }
        return;
      }

      if (f.type == DATA) {
        // 使用offset进行流重组
        uint32_t dataLen = f.length;
        uint64_t dataOffset = f.offset;

        // 额外稳固：检查LEN字段与payload大小的一致性
        if (f.length != f.payload.size()) {
          std::cout << "[WARN] Stream " << sid << " LEN(" << f.length 
                    << ") != payload.size(" << f.payload.size() << "), using LEN" << std::endl;
        }

        if (isPush || m_pushTargetBytes.count(sid)) {
          m_pushBytes[sid] += dataLen;
          if (m_pushTargetBytes[sid] > 0 &&
              m_pushBytes[sid] >= m_pushTargetBytes[sid]) {
            ++m_pushCompleted;
          }
          return;
        }

        // 健壮性检查：若没收到HEADERS就收到DATA，给出警告
        if (!m_streamTargetBytes.count(sid)) {
          std::cout << "[WARN] DATA before Content-Length (sid=" << sid << "), dataLen=" << dataLen << std::endl;
        }

        // 使用offset进行流重组
        MarkReceived(sid, dataOffset, dataLen);
        
        // 按缺口"催一下"重传
        if (!HasFullPrefix(sid, m_streamTargetBytes[sid])) {
          QuicFrame ack; ack.type = QF_ACK; ack.offset = 0; ack.payload = ""; // 累计ACK
          m_session->SendFrames({ack});
        }
        
        // 检查是否完成
        uint64_t have = BytesReceived(sid);
        uint64_t need = m_streamTargetBytes[sid];
        
        if (need > 0 && HasFullPrefix(sid, need)) {
          Complete(sid);
        } else {
          // MODIFIED: Wrap the log
          if (!m_quiet) {
            // 调试信息：显示流进度
            std::cout << "[DEBUG] Stream " << sid << " received DATA: offset=" << dataOffset 
                      << " len=" << dataLen << " total: " << have << "/" << need 
                      << " bytes" << std::endl;
          }
        }
      }
    } catch (const std::exception& e) {
      NS_LOG_WARN("Failed to parse frame: " << e.what());
    }
  }

  void CheckStreamCompletion(uint32_t streamId) {
    if (!m_streamTargetBytes.count(streamId)) return;
    uint64_t need = m_streamTargetBytes[streamId];
    uint64_t have = BytesReceived(streamId);
    if (need > 0 && HasFullPrefix(streamId, need) && !m_streamCompleted[streamId]) {
      m_streamCompleted[streamId] = true;
      ++m_respsRcvd;
      m_respRecvTimes.push_back(Simulator::Now().GetSeconds());
      if (!m_quiet) {
        std::cout << "[DEBUG] Stream " << streamId << " completed! Total: " << m_respsRcvd << "/" << m_nReqs << std::endl;
      }
      
      // ★ 关键修复 ★
      // 如果还有请求需要发送，立即发送下一个，而不是等待
      if (m_respsRcvd < m_nReqs && m_reqsSent < m_nReqs) {
        SendSingleRequest();
      }
    } else if (!m_quiet) {
      std::cout << "[DEBUG] Stream " << streamId << " progress: " 
              << have << "/" << need 
                << " bytes" << std::endl;
    }
  }
  
  // ★ 新增一个辅助函数 ★
  void SendSingleRequest() {
    if (m_reqsSent >= m_nReqs) return;

    uint32_t streamId = m_nextStreamId++;
    m_session->OpenStream(streamId);

    HTTP3Frame h;
    h.streamId = streamId;
    h.type = HEADERS;
    std::ostringstream oss;
    if (m_thirdParty) {
      const char* domains[] = {"firstparty.example","cdn.example","ads.example"};
      const char* host = domains[m_reqsSent % 3];
      oss << "GET /file" << m_reqsSent << " HTTP/3.0\r\nHost: " << host << "\r\n\r\n";
    } else {
      oss << "GET /file" << m_reqsSent << " HTTP/3.0\r\nHost: server\r\n\r\n";
    }
    h.payload = oss.str();
    h.length  = h.payload.size();
    uint32_t desired = std::max(m_reqSize, (uint32_t)h.payload.size());
    if (desired > h.payload.size()) { h.payload.append(desired - h.payload.size(), ' '); h.length = desired; }

    std::string hs = h.Serialize();
    m_session->SendStreamData(streamId, reinterpret_cast<const uint8_t*>(hs.data()), hs.size(), false);

    HTTP3Frame end;
    end.streamId = streamId; end.type = DATA; end.length = 0; end.payload = "";
    std::string es = end.Serialize();
    m_session->SendStreamData(streamId, reinterpret_cast<const uint8_t*>(es.data()), es.size(), true);

    m_reqSendTimes.push_back(Simulator::Now().GetSeconds());
    ++m_reqsSent;
  }

  Ptr<Socket> m_socket;
  Address m_servAddr;
  uint16_t m_port;
  uint32_t m_reqSize, m_nReqs;
  uint32_t m_reqsSent{0}, m_respsRcvd{0};
  std::vector<double> m_reqSendTimes, m_respRecvTimes;
  std::map<uint32_t, std::string> m_rxBuf;   // 每条流独立的接收缓冲
  double m_interval{0.01};
  bool m_thirdParty{false};
  uint32_t m_nStreams{3};
  Ptr<QuicSession> m_session;

  std::map<uint32_t,uint32_t> m_streamBytes, m_streamTargetBytes;
  std::map<uint32_t,bool>     m_streamCompleted;
  std::map<uint32_t,uint32_t> m_streamDataFrames;  // 新增：跟踪每个流接收到的DATA帧数量
  uint32_t m_nextStreamId{1};   // 每个请求使用唯一的流 ID（单调递增）

  // 流重组相关成员 - 简化版本
  std::map<uint32_t, uint64_t> m_streamBytesReceived;  // 每个流已接收的字节数
  std::map<uint32_t, uint64_t> m_finalSize;  // 来自FIN或HEADERS的Content-Length

  std::map<uint32_t,uint32_t> m_pushBytes, m_pushTargetBytes;
  uint32_t m_pushCompleted{0}, m_pushStreams{0};
  
  // 区间重组结构
  struct Range { uint64_t lo; uint64_t hi; };
  std::map<uint32_t, std::vector<Range>> m_ranges;   // sid -> ranges

  void AddRange(uint32_t sid, uint64_t off, uint32_t len) {
    if (!len) return;
    uint64_t lo = off, hi = off + len;
    auto &v = m_ranges[sid];
    std::vector<Range> out; out.reserve(v.size() + 1);
    bool inserted = false;
    for (auto &r : v) {
      if (hi < r.lo) {
        if (!inserted) { out.push_back({lo, hi}); inserted = true; }
        out.push_back(r);
      } else if (r.hi < lo) {
        out.push_back(r);
      } else {
        lo = std::min(lo, r.lo);
        hi = std::max(hi, r.hi);
      }
    }
    if (!inserted) out.push_back({lo, hi});
    v.swap(out);
  }

  bool HasFullPrefix(uint32_t sid, uint64_t need) const {
    auto it = m_ranges.find(sid);
    if (it == m_ranges.end()) return need == 0;
    const auto& v = it->second;
    if (need == 0) return true;
    if (v.empty() || v.front().lo != 0) return false;
    uint64_t reach = 0;
    for (auto &r : v) {
      if (r.lo > reach) return false;
      reach = std::max(reach, r.hi);
      if (reach >= need) return true;
    }
    return false;
  }

  // 流重组核心方法：基于offset的区间合并
  void MarkReceived(uint32_t streamId, uint64_t offset, uint32_t length) {
    AddRange(streamId, offset, length);
    if (!m_quiet) {
      std::cout << "[DEBUG] MarkReceived: stream=" << streamId
                << " offset=" << offset << " len=" << length
                << " total=" << BytesReceived(streamId) << " bytes" << std::endl;
    }
  }
  
  uint64_t BytesReceived(uint32_t streamId) const {
    uint64_t sum = 0;
    auto it = m_ranges.find(streamId);
    if (it != m_ranges.end()) {
      for (auto &r : it->second) sum += (r.hi - r.lo);
    }
    return sum;
  }
  
  void Complete(uint32_t streamId) {
    if (m_streamCompleted[streamId]) return;
    
    m_streamCompleted[streamId] = true;
    ++m_respsRcvd;
    m_respRecvTimes.push_back(Simulator::Now().GetSeconds());
    
    uint64_t totalSize = m_streamTargetBytes[streamId];
    std::cout << "STREAM_COMPLETED_LOG," << Simulator::Now().GetSeconds()
              << "," << streamId << "," << totalSize << std::endl;
    
    if (!m_quiet) {
      std::cout << "[DEBUG] Stream " << streamId << " completed via offset reassembly! Total: " 
                << m_respsRcvd << "/" << m_nReqs << std::endl;
    }
    
    if (m_respsRcvd < m_nReqs && m_reqsSent < m_nReqs) {
      Simulator::Schedule(Seconds(m_interval), &Http3ClientApp::SendNextRequest, this);
    }
  }
  bool m_quiet{false};
};

// -------------------- HTTP/3 Server --------------------
class Http3ServerApp : public Application {
public:
  Http3ServerApp() : m_socket(0), m_port(0) {}
  void Setup(uint16_t port, uint32_t respSize, uint32_t maxReqs, uint32_t nStreams,
             uint32_t frameChunk, uint32_t tickUs, uint32_t headerSize, double hpackRatio,
             bool enablePush, uint32_t pushSize, bool quiet = false) {
    m_port = port; m_respSize = respSize; m_maxReqs = maxReqs; m_nStreams = nStreams;
    m_frameChunk = frameChunk; m_tickUs = tickUs; m_headerSize = headerSize;
    m_hpackRatio = hpackRatio; m_enablePush = enablePush; m_pushSize = pushSize;
    m_quiet = quiet;
  }

  uint64_t GetHolEvents() const { return m_srvHolEvents; }
  double GetHolBlockedTime() const { return m_srvHolBlockedTime; }

  // 在Http3ServerApp类中添加LogCongestionState方法
  void LogCongestionState() {
    if (m_session) {
      std::cout << "CWND_LOG," << Simulator::Now().GetSeconds() << ","
                << m_session->CwndBytes() << "," << m_session->BytesInFlight() << std::endl;
    }
    Simulator::Schedule(MilliSeconds(10), &Http3ServerApp::LogCongestionState, this);
  }

private:
  void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_port));
    m_session = CreateObject<QuicSession>(m_socket, m_quiet);  // 传递quiet参数
    m_session->SetStreamDataCallback(MakeCallback(&Http3ServerApp::OnStreamData, this));
    // 绑定ACK唤醒回调：收到ACK后立即尝试继续发送
    m_session->SetWakeupCallback(MakeCallback(&Http3ServerApp::OnCanSend, this));
    m_reqsHandled = 0; m_pendingQueue.clear(); m_sending = false; m_nextPushSid = 1001; m_reqBuf.clear();
    m_streamOffsets.clear();  // 初始化流偏移
    // 服务器侧HoL统计
    m_srvHolBlockedTime = 0.0; m_srvHolEvents = 0; m_blocking = false; m_blockStart = Seconds(0);
    
    // 添加拥塞控制日志
    Simulator::Schedule(MilliSeconds(10), &Http3ServerApp::LogCongestionState, this);
  }

  void OnCanSend() {
    if (!m_sending) {
      m_sending = true;
      Simulator::ScheduleNow(&Http3ServerApp::SendTick, this);
    }
  }

  void StopApplication() override { if (m_socket) m_socket->Close(); }

  void OnStreamData(uint32_t streamId, const uint8_t* data, uint32_t len, bool fin) {
    std::string& buf = m_reqBuf[streamId];
    buf.append(reinterpret_cast<const char*>(data), len);

    size_t pos = 0;
    while (pos < buf.size()) {
      size_t frameStart = buf.find("SID:", pos);
      if (frameStart == std::string::npos) break;

      size_t lenStart = buf.find("LEN:", frameStart);
      if (lenStart == std::string::npos) break;

      size_t lenValueStart = lenStart + 4;
      size_t lenValueEnd = buf.find("|", lenValueStart);
      if (lenValueEnd == std::string::npos) break;

      uint32_t frameLen;
      try {
        frameLen = std::stoul(buf.substr(lenValueStart, lenValueEnd - lenValueStart));
      } catch (...) { break; }

      size_t payloadStart = lenValueEnd + 1;
      // 若存在 OFF: 字段，跳过 OFF:...|
      if (payloadStart + 4 <= buf.size() && buf.compare(payloadStart, 4, "OFF:") == 0) {
        size_t offEnd = buf.find('|', payloadStart + 4);
        if (offEnd == std::string::npos) break;
        payloadStart = offEnd + 1;
      }
      if (payloadStart + frameLen > buf.size()) break;

      std::string frameData = buf.substr(frameStart, payloadStart - frameStart + frameLen);
      ProcessFrame(frameData);

      pos = frameStart + frameData.size();
    }

    if (pos > 0) buf.erase(0, pos);
    // fin 的语义同上：不强制做任何事，由应用层 HEADERS/DATA 驱动响应
  }

  void ProcessFrame(const std::string& frameData) {
    try {
      HTTP3Frame f = HTTP3Frame::Parse(frameData);
      if (f.type == HEADERS) {
        if (m_reqsHandled >= m_maxReqs) return;
        ++m_reqsHandled;

        uint32_t rsz = m_respSize;
        if (!g_respSizes.empty()) {
          uint32_t idx = std::min<uint32_t>(m_reqsHandled - 1, g_respSizes.size() - 1);
          rsz = g_respSizes[idx];
        }

        // QPACK (模拟) 压缩后头部大小 - 修复：绝不截断头部
        std::ostringstream oss;
        oss << "HTTP/3.0 200 OK\r\nContent-Length: " << rsz << "\r\n\r\n";
        std::string baseHeaders = oss.str();
        
        uint32_t want = std::max<uint32_t>(baseHeaders.size(), (uint32_t)(m_headerSize * m_hpackRatio));
        // 仅在不足时用空格填充，绝不截断
        std::string hdr = baseHeaders;
        if (want > baseHeaders.size()) {
          hdr.append(want - baseHeaders.size(), ' ');
        }

        // 发响应 HEADERS（序列化为 HTTP3Frame）
        HTTP3Frame hf;
        hf.streamId = f.streamId; hf.type = HEADERS; hf.payload = hdr; hf.length = hdr.size();
        std::string hs = hf.Serialize();
        m_session->SendStreamData(f.streamId, reinterpret_cast<const uint8_t*>(hs.data()), hs.size(), false);

        // enqueue DATA
        m_pendingQueue.emplace_back(f.streamId, rsz);

        // shadow push
        if (m_enablePush) {
          uint32_t psid = m_nextPushSid++;
          
          // 显式打开推送流
          m_session->OpenStream(psid);

          HTTP3Frame promise; // attach to parent stream
          promise.streamId = f.streamId; promise.type = PUSH_PROMISE;
          std::ostringstream pss; pss << "PUSH /p" << psid << " promised-stream: " << psid << "\r\n";
          promise.payload = pss.str(); promise.length = promise.payload.size();
          std::string pm = promise.Serialize();
          m_session->SendStreamData(f.streamId, reinterpret_cast<const uint8_t*>(pm.data()), pm.size(), false);

          HTTP3Frame ph;
          ph.streamId = psid; ph.type = HEADERS;
          std::ostringstream hss;
          hss << "HTTP/3.0 200 OK\r\nContent-Length: " << m_pushSize << "\r\nx-push: 1\r\n\r\n";
          ph.payload = hss.str(); ph.length = ph.payload.size();
          std::string ss = ph.Serialize();
          m_session->SendStreamData(psid, reinterpret_cast<const uint8_t*>(ss.data()), ss.size(), false);

          m_pendingQueue.emplace_back(psid, m_pushSize);
        }

        // ★ 关键修改 ★
        // 如果当前没有在发送，则立即启动发送循环
        if (!m_sending) { 
          m_sending = true; 
          Simulator::ScheduleNow(&Http3ServerApp::SendTick, this); 
        }
      }
    } catch (const std::exception& e) {
      NS_LOG_WARN("Failed to parse frame: " << e.what());
    }
  }

  void SendTick() {
    // 如果队列已空，停止发送循环
    if (m_pendingQueue.empty()) {
        m_sending = false;
        return;
    }

    // ★ 关键修复 1: 在每次Tick的开始就检查拥塞窗口 ★
    // 如果窗口已满，则停止发送，等待网络事件(ACK)通过 OnCanSend() 唤醒
    if (m_session->BytesInFlight() >= m_session->CwndBytes()) {
        m_sending = false; // 等待被唤醒
        return;
    }

    // ★ 关键修复 2: 实现真正的轮询调度（一次只处理一个任务）★

    // 1. 从队列头部取出一个任务
    PendingItem item = m_pendingQueue.front();
    m_pendingQueue.pop_front();

    // 2. 为这个任务发送一小块数据（一个数据包的量）
    const uint32_t effMtu = 1200 - 28; // 估算MTU
    const uint32_t safety = 64;       // 预留头部开销
    uint32_t sendBytes = std::min({m_frameChunk, item.remainingBytes, effMtu - safety});
    
    if (sendBytes > 0) {
        HTTP3Frame df;
        df.streamId = item.streamId;
        df.type = DATA;
        df.payload.assign(sendBytes, 'D');
        df.length = sendBytes;
        
        // 确保流偏移被正确初始化和使用
        if (m_streamOffsets.find(item.streamId) == m_streamOffsets.end()) {
            m_streamOffsets[item.streamId] = 0;
        }
        df.offset = m_streamOffsets[item.streamId];

        bool isLast = (item.remainingBytes <= sendBytes);
        std::string s = df.Serialize();
        m_session->SendStreamData(item.streamId, reinterpret_cast<const uint8_t*>(s.data()), s.size(), isLast);

        m_streamOffsets[item.streamId] += sendBytes;
        item.remainingBytes -= sendBytes;
        item.sentBytes += sendBytes;
        
        // 若之前处于阻塞，首包发出时结束一次HoL计时
        if (m_blocking) { 
            m_srvHolBlockedTime += (Simulator::Now() - m_blockStart).GetSeconds(); 
            m_blocking = false; 
        }
    }

    // 3. 如果这个任务还没完成，就把它放回队列的尾部
    if (item.remainingBytes > 0) {
        m_pendingQueue.push_back(item);
    }

    // ★ 关键修复 3: 只要队列中还有任务，就立即调度下一次Tick ★
    // 这会创建一个连续、平滑的发送流，而不是之前的"爆发-等待"模式。
    if (!m_pendingQueue.empty()) {
        m_sending = true;
        Simulator::ScheduleNow(&Http3ServerApp::SendTick, this);
    } else {
        m_sending = false; // 所有任务都完成了
    }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
  uint32_t m_respSize, m_maxReqs, m_reqsHandled{0};
  uint32_t m_nStreams{3};
  uint32_t m_frameChunk{1200};
  uint32_t m_tickUs{500};
  bool m_sending{false};
  std::deque<PendingItem> m_pendingQueue;
  std::map<uint32_t, std::string> m_reqBuf;  // 每条流独立的接收缓冲（请求方向）
  uint32_t m_headerSize{200};
  double m_hpackRatio{0.3};
  bool m_enablePush{false};
  uint32_t m_pushSize{12*1024};
  uint32_t m_nextPushSid{1001};
  Ptr<QuicSession> m_session;
  
  // 流偏移跟踪
  std::map<uint32_t, uint64_t> m_streamOffsets;  // 每个流的当前偏移
  // 服务器侧HoL统计
  double m_srvHolBlockedTime{0.0};
  uint64_t m_srvHolEvents{0};
  bool m_blocking{false};
  Time m_blockStart;
  bool m_quiet{false};
};

// -------------------- main --------------------
int main(int argc, char* argv[]) {
  uint32_t nRequests = 16;            // 减少请求数，便于调试
  uint32_t respSize  = 150*1024;      // 增大响应大小，重现大对象问题
  uint32_t reqSize   = 100;
  uint16_t httpPort  = 8080;
  double   errorRate = 0.01;          // 设置丢包率，重现丢包问题
  std::string dataRate = "10Mbps";
  std::string delay    = "5ms";
  double interval = 0.01;
  uint32_t nConnections = 1;
  bool mixedSizes = false;
  bool thirdParty = false;
  uint32_t nStreams = 3;
  uint32_t frameChunk = std::min(1200u - 28u - 32u, 1200u); // 限制不跨UDP包
  uint32_t tickUs     = 500;
  uint32_t headerSize = 200;
  double   hpackRatio = 0.3;   // simulate QPACK ratio
  bool enablePush = false;
  uint32_t pushSize = 12*1024;
  double pushHitRate = 1.0;
  double simTime = 120.0;  // 默认更长仿真时间
  bool quiet = false;  // 添加安静模式标志

  CommandLine cmd;
  cmd.AddValue("nRequests", "Number of HTTP requests", nRequests);
  cmd.AddValue("respSize", "HTTP response size (bytes)", respSize);
  cmd.AddValue("reqSize", "HTTP request size (bytes)", reqSize);
  cmd.AddValue("httpPort", "HTTP server port", httpPort);
  cmd.AddValue("errorRate", "Packet loss rate", errorRate);
  cmd.AddValue("dataRate", "Link bandwidth", dataRate);
  cmd.AddValue("delay", "Link delay", delay);
  cmd.AddValue("latency", "Alias of --delay", delay);
  cmd.AddValue("interval", "Interval between HTTP requests (s)", interval);
  cmd.AddValue("nConnections", "Number of parallel HTTP/3 connections", nConnections);
  cmd.AddValue("mixedSizes", "Use mixed object size distribution", mixedSizes);
  cmd.AddValue("thirdParty", "Simulate third-party Hosts", thirdParty);
  cmd.AddValue("nStreams", "Number of concurrent HTTP/3 streams", nStreams);
  cmd.AddValue("frameChunk", "Frame chunk size", frameChunk);
  cmd.AddValue("tickUs", "Tick interval (us)", tickUs);
  cmd.AddValue("headerSize", "Base header size", headerSize);
  cmd.AddValue("hpackRatio", "QPACK ratio", hpackRatio);
  cmd.AddValue("enablePush", "Enable shadow server push", enablePush);
  cmd.AddValue("pushSize", "Push object size (bytes)", pushSize);
  cmd.AddValue("pushHitRate", "Push hit probability", pushHitRate);
  cmd.AddValue("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue("quiet", "Disable verbose per-packet/frame logs for performance", quiet);  // 添加quiet参数
  cmd.Parse(argc, argv);

  g_respSizes.clear(); g_respSizes.reserve(nRequests);
  if (!mixedSizes) {
    for (uint32_t i=0;i<nRequests;++i) g_respSizes.push_back(respSize);
  } else {
    for (uint32_t i=0;i<nRequests;++i) {
      double r = double(i)/std::max(1u, nRequests-1);
      if (r < 0.05) g_respSizes.push_back(10*1024);
      else if (r < 0.40) g_respSizes.push_back(50*1024);
      else g_respSizes.push_back(200*1024);
    }
  }

  NodeContainer nodes; nodes.Create(2);
  PointToPointHelper p2p; p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));
  p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("32kB"));
  NetDeviceContainer devs = p2p.Install(nodes);

  InternetStackHelper stack; stack.Install(nodes);

  Ipv4AddressHelper addr; addr.SetBase("10.1.1.0","255.255.255.0");
  Ipv4InterfaceContainer ifs = addr.Assign(devs);

  Ptr<Http3ServerApp> server = CreateObject<Http3ServerApp>();
  server->Setup(httpPort, respSize, nRequests, nStreams, frameChunk, tickUs,
                headerSize, hpackRatio, enablePush, pushSize, quiet);
  nodes.Get(1)->AddApplication(server);
  server->SetStartTime(Seconds(0.5));
  server->SetStopTime(Seconds(simTime));

  std::vector< Ptr<Http3ClientApp> > clients;
  uint32_t baseReqs = nRequests / nConnections;
  uint32_t rem = nRequests % nConnections;
  for (uint32_t i=0;i<nConnections;++i) {
    uint32_t reqs = baseReqs + (i < rem ? 1 : 0);
    Ptr<Http3ClientApp> c = CreateObject<Http3ClientApp>();
    c->Setup(ifs.GetAddress(1), httpPort, reqSize, reqs, interval, thirdParty, nStreams, quiet);  // 传递quiet参数
    nodes.Get(0)->AddApplication(c);
    c->SetStartTime(Seconds(1.0 + i*0.01));
    c->SetStopTime(Seconds(simTime));
    clients.push_back(c);
  }

  Ptr<RateErrorModel> em0 = CreateObject<RateErrorModel>();
  em0->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em0->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
  devs.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em0));

  Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel>();
  em1->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em1->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
  devs.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em1));

  FlowMonitorHelper fmHelper;
  Ptr<FlowMonitor> flowmon = fmHelper.InstallAll();

  Simulator::Stop(Seconds(simTime + 1.0));  // 留1s缓冲
  Simulator::Run();

  uint32_t totalResps = 0;
  std::vector<double> sendTimes, recvTimes;
  double firstSend = std::numeric_limits<double>::infinity();
  double lastRecv = 0.0, sumDelay = 0.0;
  size_t nDone = 0;
  // 修复Jitter计算：按照RFC3550计算interarrival variation
  double rfcJitter = 0.0;

  // 收集所有客户端的数据
  for (auto& c : clients) {
    totalResps += c->GetRespsRcvd();
    const auto& s = c->GetReqSendTimes();
    const auto& r = c->GetRespRecvTimes();
    size_t n = std::min(s.size(), r.size());
    if (n > 0) { 
      firstSend = std::min(firstSend, s.front()); 
      lastRecv = std::max(lastRecv, r.back()); 
    }
    for (size_t i=0;i<n;++i) {
      double transit = r[i]-s[i];
      sumDelay += transit; ++nDone;
    }
    sendTimes.insert(sendTimes.end(), s.begin(), s.end());
    recvTimes.insert(recvTimes.end(), r.begin(), r.end());
  }

  // 计算正确的jitter
  if (nDone > 1) {
    // 先排序，避免多客户端插入次序导致0
    std::vector<double> sortedRecvTimes = recvTimes;
    std::sort(sortedRecvTimes.begin(), sortedRecvTimes.end());
    
    std::vector<double> interarrivalTimes;
          for (size_t i = 1; i < sortedRecvTimes.size() && i < nDone; ++i) {
        double interarrival = sortedRecvTimes[i] - sortedRecvTimes[i-1];
        if (interarrival > 0 && interarrival < 20.0) { // 过滤异常值，阈值放宽到20s
          interarrivalTimes.push_back(interarrival);
        }
      }
    
    if (interarrivalTimes.size() > 1) {
      // 计算interarrival variation的标准差作为jitter
      double mean = std::accumulate(interarrivalTimes.begin(), interarrivalTimes.end(), 0.0) / interarrivalTimes.size();
      double variance = 0.0;
      for (double time : interarrivalTimes) {
        variance += (time - mean) * (time - mean);
      }
      rfcJitter = std::sqrt(variance / interarrivalTimes.size());
    }
  }

  // 修复HoL Events计算：只统计真正的队列阻塞事件
  uint64_t holEvents = server->GetHolEvents();
  double holBlockedTime = server->GetHolBlockedTime();

  // 始终打印最小概要，便于外部工具抓取（无论是否计算出完整统计）
  double completionRate = (nDone > 0) ? (double(nDone) / double(nRequests)) * 100.0 : 0.0;
  std::cout << "The HTTP/3 experiment has ended. The total number of responses received by the client is: "
            << totalResps << "/" << nRequests << " (completion rate: " << std::fixed << std::setprecision(1) << completionRate << "%)" << std::endl;
  std::cout << "completedResponses (nDone): " << totalResps << "/" << nRequests << std::endl;

  if (nDone > 0 && lastRecv > firstSend) {
  // 修复Response Time计算：只计算单个请求的响应时间
  double avgDelay = 0.0;
  if (nDone > 0) {
    // 只计算单个请求的响应时间，不跨请求
    std::vector<double> individualDelays;
    for (size_t i = 0; i < nDone; ++i) {
      if (i < sendTimes.size() && i < recvTimes.size()) {
        double individualDelay = recvTimes[i] - sendTimes[i];
        if (individualDelay > 0 && individualDelay < 10.0) { // 过滤异常值
          individualDelays.push_back(individualDelay);
        }
      }
    }
    
    if (!individualDelays.empty()) {
      avgDelay = std::accumulate(individualDelays.begin(), individualDelays.end(), 0.0) / individualDelays.size();
    }
  }
    double headerCompressed = std::max(20.0, headerSize * hpackRatio);

    double totalBytesDown = double(nDone) * (respSize + headerCompressed);
    // 修复吞吐量计算：只用已完成请求的实际传输时间
    double bytesPer = (respSize + headerCompressed);
    double bytesDown = 0.0;
    double timeSum = 0.0;
    size_t n = std::min(sendTimes.size(), recvTimes.size());
    for (size_t i = 0; i < n; ++i) {
      if (recvTimes[i] > sendTimes[i]) {
        double dt = recvTimes[i] - sendTimes[i];
        if (dt > 0 && dt < simTime) {   // 过滤异常
          bytesDown += bytesPer;
          timeSum += dt;
        }
      }
    }
    
    // SANITY调试输出
    std::cout << "[SANITY] nDone=" << nDone
              << " bytesPer=" << (respSize + headerCompressed)
              << " bytesDown=" << bytesDown
              << " timeSum=" << timeSum << "s" << std::endl;
    
    // 修复吞吐量计算：使用实际传输时间窗口，而不是所有请求时间总和
    double actualTransmissionTime = lastRecv - firstSend;
    double throughputDown = (actualTransmissionTime > 0) ? (bytesDown * 8.0) / (actualTransmissionTime * 1e6) : 0.0;

    double totalBytesUp = double(nDone) * headerCompressed;
    double totalBytesBi = totalBytesDown + totalBytesUp;
    double totalTime = lastRecv - firstSend;
    // 修复双向吞吐量计算
    double bytesBiPer = (respSize + headerCompressed + headerCompressed); // 下行+上行
    double bytesBi = 0.0;
    double timeSumBi = 0.0;
    for (size_t i = 0; i < n; ++i) {
      if (recvTimes[i] > sendTimes[i]) {
        double dt = recvTimes[i] - sendTimes[i];
        if (dt > 0 && dt < simTime) {   // 过滤异常
          bytesBi += bytesBiPer;
          timeSumBi += dt;
        }
      }
    }
    double throughputBi = (actualTransmissionTime > 0) ? (totalBytesBi * 8.0) / (actualTransmissionTime * 1e6) : 0.0; // 与下行同窗计算

    double originalBytes = double(nDone) * (respSize + headerSize);
    double savedBytes = originalBytes - totalBytesDown;
    double compressionRatio = (originalBytes > 0) ? (savedBytes / originalBytes) * 100.0 : 0.0;

    std::cout << "Average delay of HTTP/3: " << avgDelay << " s" << std::endl;

    std::cout << "------------------------------------------\n";
    std::cout << "HTTP/3 Experiment Summary\n";
    std::cout << "completedResponses (nDone): " << nDone << "/" << nRequests << std::endl;
    std::cout << "dataPerResp (bytes): " << respSize << std::endl;
    std::cout << "qpackPerResp (bytes): " << std::fixed << std::setprecision(0) << headerCompressed << std::endl;
    std::cout << "firstSend: " << std::fixed << std::setprecision(6) << firstSend << "s\n";
    std::cout << "lastRecv: "  << std::fixed << std::setprecision(6) << lastRecv  << "s\n";
    std::cout << "totalTime: " << std::fixed << std::setprecision(6) << totalTime << "s\n\n";

    std::cout << "Downlink bytes: " << std::fixed << std::setprecision(0) << totalBytesDown << " B\n";
    std::cout << "Downlink throughput: " << std::fixed << std::setprecision(3) << throughputDown << " Mbps\n\n";

    std::cout << "Bidirectional bytes (incl. uplink headers): " << std::fixed << std::setprecision(0) << totalBytesBi << " B\n";
    std::cout << "Bidirectional throughput: " << std::fixed << std::setprecision(3) << throughputBi << " Mbps\n\n";

    std::cout << "QPACK compression: saved " << std::fixed << std::setprecision(0) << savedBytes
              << " bytes (" << std::fixed << std::setprecision(1) << compressionRatio << "%)\n";
      // 修复Page Load Time：使用逐请求的均值
    double pageLoadTime = 0.0;
    if (nDone > 0) {
      std::vector<double> individualPLTs;
      for (size_t i = 0; i < n; ++i) {
        if (recvTimes[i] > sendTimes[i]) {
          double dt = recvTimes[i] - sendTimes[i];
          if (dt > 0 && dt < simTime) {   // 过滤异常
            individualPLTs.push_back(dt);
          }
        }
      }
      if (!individualPLTs.empty()) {
        pageLoadTime = std::accumulate(individualPLTs.begin(), individualPLTs.end(), 0.0) / individualPLTs.size();
      }
    }
    std::cout << "Page Load Time (onLoad): " << std::fixed << std::setprecision(6) << pageLoadTime << " s\n";
    std::cout << "QUIC retransmissions: " << g_retxCount
              << "  rate: " << std::fixed << std::setprecision(3) << (g_retxCount / (totalTime > 0 ? totalTime : 1.0)) << " /s\n";
    std::cout << "RFC3550 jitter estimate: " << std::fixed << std::setprecision(6) << rfcJitter << " s\n";
    std::cout << "HoL events: " << holEvents << "  HoL blocked time: " << std::fixed << std::setprecision(6) << holBlockedTime << " s\n";
    std::cout << "------------------------------------------\n";

    // ---- Structured one-line summary for CSV harvesting ----
    auto parseMs = [](const std::string &s)->int{
      try {
        size_t pos = s.find("ms");
        if (pos != std::string::npos) return std::stoi(s.substr(0,pos));
        return std::stoi(s);
      } catch (...) { return 0; }
    };
    auto parseMbps = [](const std::string &s)->double{
      try {
        size_t pos = s.find("Mbps");
        if (pos != std::string::npos) return std::stod(s.substr(0,pos));
        return std::stod(s);
      } catch (...) { return 0.0; }
    };
    int delayMsOut = parseMs(delay);
    double bwOut = parseMbps(dataRate);
    double lossOut = errorRate;
    double p50s = pageLoadTime;

    std::cout << "CSV_SUMMARY "
              << "latency_ms=" << delayMsOut
              << " bandwidth_mbps=" << std::fixed << std::setprecision(3) << bwOut
              << " loss_rate=" << std::setprecision(6) << lossOut
              << " throughput_mbps=" << std::setprecision(3) << throughputDown
              << " plt_s=" << std::setprecision(6) << p50s
              << " retx_count=" << g_retxCount
              << " jitter_s=" << std::setprecision(6) << rfcJitter
              << " hol_events=" << holEvents
              << " hol_time_s=" << std::setprecision(6) << holBlockedTime
              << " qpack_saved_bytes=" << (long long)std::llround(savedBytes)
              << " qpack_compression_percent=" << std::setprecision(1) << compressionRatio
              << std::endl;
  }

  flowmon->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmHelper.GetClassifier());
  if (classifier) {
    const auto& stats = flowmon->GetFlowStats();
    for (const auto& kv : stats) {
      uint32_t flowId = kv.first;
      const FlowMonitor::FlowStats& st = kv.second;
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
      double avgDelay = (st.rxPackets > 0) ? st.delaySum.GetSeconds() / st.rxPackets : 0.0;
      double avgJitter = (st.rxPackets > 1) ? st.jitterSum.GetSeconds() / (st.rxPackets - 1) : 0.0;
      std::cout << "Flow " << flowId
                << " src=" << t.sourceAddress << ":" << t.sourcePort
                << " -> dst=" << t.destinationAddress << ":" << t.destinationPort
                << " proto=" << uint32_t(t.protocol)
                << " rxPackets=" << st.rxPackets
                << " avgDelay=" << avgDelay << " s"
                << " avgJitter=" << avgJitter << " s"
                << std::endl;
    }
  }
  flowmon->SerializeToXmlFile("flowmon.xml", true, true);

  // 验证数据完整性
  std::cout << "\n------ Data Integrity Verification ------\n";
  for (auto& client : clients) {
    client->VerifyCompletedStreams();
  }
  std::cout << "------------------------------------------\n";

  Simulator::Destroy();
  return 0;
}
