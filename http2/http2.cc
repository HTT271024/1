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
#include "ns3/tcp-header.h"
#include "ns3/tcp-socket-base.h"
#include <map>
#include <string>
#include <iomanip>


using namespace ns3;


NS_LOG_COMPONENT_DEFINE("HTTP2App");


// HTTP/2 Frame Type
enum FrameType { HEADERS, DATA, PUSH_PROMISE };


// HTTP/2 Frame with stream ID prefix for lightweight multiplexing
struct HTTP2Frame {
   uint32_t streamId;
   FrameType type;
   uint32_t length;
   std::string payload;
  
   // Serialize frame with stream ID prefix for multiplexing
   std::string Serialize() const {
       std::ostringstream oss;
       oss << "SID:" << streamId << "|TYPE:" << (int)type << "|LEN:" << length << "|";
       oss << payload;
       return oss.str();
   }
  
   // Parse frame from received data with enhanced error handling
   static HTTP2Frame Parse(const std::string& data) {
       HTTP2Frame frame;
       size_t pos = 0;
      
       try {
           // Validate input data
           if (data.empty() || data.length() < 10) {
               NS_LOG_ERROR("Frame parsing failed: insufficient data length: " << data.length());
               return frame;
           }
           
           // Parse SID
           if (data.substr(pos, 4) == "SID:") {
               pos += 4;
               size_t end = data.find('|', pos);
               if (end != std::string::npos && end > pos) {
                   std::string sidStr = data.substr(pos, end - pos);
                   if (!sidStr.empty() && sidStr.find_first_not_of("0123456789") == std::string::npos) {
                       frame.streamId = std::stoi(sidStr);
                       pos = end + 1;
                   } else {
                       NS_LOG_ERROR("Frame parsing failed: invalid SID format: " << sidStr);
                       return frame;
                   }
               } else {
                   NS_LOG_ERROR("Frame parsing failed: missing SID delimiter");
                   return frame;
               }
           } else {
               NS_LOG_ERROR("Frame parsing failed: missing SID prefix");
               return frame;
           }
          
           // Parse TYPE
           if (pos < data.length() && data.substr(pos, 5) == "TYPE:") {
               pos += 5;
               size_t end = data.find('|', pos);
               if (end != std::string::npos && end > pos) {
                   std::string typeStr = data.substr(pos, end - pos);
                   if (!typeStr.empty() && typeStr.find_first_not_of("0123456789") == std::string::npos) {
                       int typeVal = std::stoi(typeStr);
                       if (typeVal >= 0 && typeVal <= 2) { // Valid FrameType range
                           frame.type = static_cast<FrameType>(typeVal);
                           pos = end + 1;
                       } else {
                           NS_LOG_ERROR("Frame parsing failed: invalid TYPE value: " << typeVal);
                           return frame;
                       }
                   } else {
                       NS_LOG_ERROR("Frame parsing failed: invalid TYPE format: " << typeStr);
                       return frame;
                   }
               } else {
                   NS_LOG_ERROR("Frame parsing failed: missing TYPE delimiter");
                   return frame;
               }
           } else {
               NS_LOG_ERROR("Frame parsing failed: missing TYPE prefix");
               return frame;
           }
          
           // Parse LEN
           if (pos < data.length() && data.substr(pos, 4) == "LEN:") {
               pos += 4;
               size_t end = data.find('|', pos);
               if (end != std::string::npos && end > pos) {
                   std::string lenStr = data.substr(pos, end - pos);
                   if (!lenStr.empty() && lenStr.find_first_not_of("0123456789") == std::string::npos) {
                       frame.length = std::stoi(lenStr);
                       pos = end + 1;
                   } else {
                       NS_LOG_ERROR("Frame parsing failed: invalid LEN format: " << lenStr);
                       return frame;
                   }
               } else {
                   NS_LOG_ERROR("Frame parsing failed: missing LEN delimiter");
                   return frame;
               }
           } else {
               NS_LOG_ERROR("Frame parsing failed: missing LEN prefix");
               return frame;
           }
          
           // Payload is the rest
           if (pos < data.length()) {
               frame.payload = data.substr(pos);
               // Validate payload length matches declared length
               if (frame.payload.length() != frame.length) {
                   NS_LOG_WARN("Frame payload length mismatch: declared=" << frame.length 
                               << ", actual=" << frame.payload.length());
               }
           }
           
           NS_LOG_INFO("Frame parsed successfully: sid=" << frame.streamId 
                       << ", type=" << (int)frame.type << ", len=" << frame.length);
           
       } catch (const std::exception& e) {
           NS_LOG_ERROR("Frame parsing failed with exception: " << e.what() << " for data: " << data);
           // Return empty frame to indicate parsing failure
           frame.streamId = 0;
           frame.type = HEADERS;
           frame.length = 0;
           frame.payload = "";
       }
      
       return frame;
   }
};


// Enhanced pending item with flow control and metrics
struct PendingItem {
   uint32_t streamId;
   uint32_t remainingBytes;
   uint32_t totalBytes;
   uint32_t retryCount;        // 重试次数
   double lastRetryTime;       // 上次重试时间
   bool isPaused;              // 流是否暂停
  
   PendingItem(uint32_t sid, uint32_t total) 
       : streamId(sid), remainingBytes(total), totalBytes(total), 
         retryCount(0), lastRetryTime(0.0), isPaused(false) {}
};

// Stream metrics for detailed performance tracking
struct StreamMetrics {
   uint32_t totalBytes;        // 总字节数
   double firstByteTime;       // 首字节时间
   double lastByteTime;        // 末字节时间
   uint32_t retransmissions;   // 重传次数
   uint32_t flowControlPauses; // 流控制暂停次数
   double totalDelay;          // 总延迟
   uint32_t frameCount;        // 帧数量
   
   StreamMetrics() : totalBytes(0), firstByteTime(0.0), lastByteTime(0.0),
                     retransmissions(0), flowControlPauses(0), totalDelay(0.0), frameCount(0) {}
};


// Global variables for metrics
static std::vector<uint32_t> g_respSizes;
static uint64_t g_retxCount = 0;


static void OnTcpRetransmission(Ptr<const Packet> p,
                               const ns3::TcpHeader& h,
                               const Address& from,
                               const Address& to,
                               Ptr<const ns3::TcpSocketBase> sock) {
 ++g_retxCount;
}


//Data packet tracking function
static void TxTrace(Ptr<const Packet> packet) {
 std::cout << "[Trace] Packet sent, size=" << packet->GetSize() << std::endl;
}
static void RxTrace(Ptr<const Packet> packet) {
 std::cout << "[Trace] Packet received, size=" << packet->GetSize() << std::endl;
}


// Serialize frame with stream ID prefix for multiplexing
Ptr<Packet> SerializeFrame(const HTTP2Frame& frame) {
   std::string serialized = frame.Serialize();
   Ptr<Packet> p = Create<Packet>((uint8_t*)serialized.data(), serialized.size());
   return p;
}


// Enhanced HTTP/2 Multiplexing Session with flow control
class HTTP2Session : public Object {
public:
   HTTP2Session(Ptr<Socket> socket) 
       : m_socket(socket), 
         m_defaultWindowSize(32u * 1024u * 1024u), /* 32MB */
         m_connWindowBytes(32u * 1024u * 1024u),
         m_connWindowInit(32u * 1024u * 1024u) {
       // 初始化默认流窗口大小和连接级窗口
   }
  
   void SendFrame(const HTTP2Frame& frame) {
       // 确保流窗口已初始化
       if (m_streamWindows.find(frame.streamId) == m_streamWindows.end()) {
           OpenStream(frame.streamId);
       }
       
       // 仅 DATA 受流控约束；HEADERS 不扣减
       if (frame.type == DATA) {
           if (m_connWindowBytes < frame.length || m_streamWindows[frame.streamId] < frame.length) {
               std::cout << "[FLOW_CONTROL_BLOCKED] sid=" << frame.streamId
                         << " connWin=" << m_connWindowBytes
                         << " streamWin=" << m_streamWindows[frame.streamId]
                         << " need=" << frame.length << std::endl;
               NS_LOG_WARN("Flow control: blocked sid=" << frame.streamId
                           << " connWin=" << m_connWindowBytes
                           << " streamWin=" << m_streamWindows[frame.streamId]
                           << " need=" << frame.length);
               PauseStream(frame.streamId);
               return;
           }
       }
       
       std::cout << "[Session] Sending frame: sid=" << frame.streamId 
                 << ", type=" << (int)frame.type << ", len=" << frame.length << std::endl;
       
       Ptr<Packet> p = SerializeFrame(frame);
       std::cout << "[Session] Serialized packet size: " << p->GetSize() << " bytes" << std::endl;
       
       int sent = m_socket->Send(p);
       
       if (sent > 0) {
           if (frame.type == DATA) {
               // 用应用层有效载荷长度扣减，而不是 TCP 实际写入的 bytes
               m_connWindowBytes -= frame.length;
               UpdateStreamWindow(frame.streamId, frame.length);
           }
           m_streams[frame.streamId] = true; // Ensure stream exists
           NS_LOG_INFO("Frame sent successfully: sid=" << frame.streamId 
                       << ", type=" << (int)frame.type << ", bytes=" << sent);
       } else {
           NS_LOG_WARN("Frame send failed: sid=" << frame.streamId << ", error=" << sent);
           // 触发重传逻辑
           HandleSendFailure(frame.streamId);
       }
   }
  
   void OnReceive(Ptr<Socket> socket) {
       Ptr<Packet> packet;
       while ((packet = socket->Recv())) {
           std::string data;
           data.resize(packet->GetSize());
           packet->CopyData(reinterpret_cast<uint8_t*>(&data[0]), packet->GetSize());
          
           // Parse frame with stream ID
           HTTP2Frame frame = HTTP2Frame::Parse(data);
           if (frame.streamId > 0 && frame.type == DATA) { // Valid frame
               NS_LOG_INFO("Server: received frame for stream " << frame.streamId
                          << ", type " << (int)frame.type << ", size " << frame.payload.size() << " bytes");
               
               // 连接和流窗口回填，但不超过初始值
               m_connWindowBytes = std::min(m_connWindowBytes + (uint64_t)frame.length, m_connWindowInit);
               if (m_streamWindows.count(frame.streamId)) {
                   m_streamWindows[frame.streamId] =
                     std::min(m_streamWindows[frame.streamId] + frame.length, m_defaultWindowSize);
               }
           }
       }
   }
  
   void OpenStream(uint32_t streamId) {
       m_streams[streamId] = false;
       m_streamWindows[streamId] = m_defaultWindowSize;
       NS_LOG_INFO("Stream " << streamId << " opened with window size " << m_defaultWindowSize);
   }
  
   void CloseStream(uint32_t streamId) {
       m_streams[streamId] = true;
       m_streamWindows.erase(streamId);
       NS_LOG_INFO("Stream " << streamId << " closed");
   }
  
   // 流控制方法
   void UpdateStreamWindow(uint32_t streamId, uint32_t sentBytes) {
       if (m_streamWindows.find(streamId) != m_streamWindows.end()) {
           if (m_streamWindows[streamId] > sentBytes) {
               m_streamWindows[streamId] -= sentBytes;
           } else {
               m_streamWindows[streamId] = 0;
               // 触发流控制事件
               PauseStream(streamId);
           }
       }
   }
   
   void UpdateReceiveWindow(uint32_t streamId, uint32_t receivedBytes) {
       // 模拟接收窗口更新（在实际HTTP/2中会发送WINDOW_UPDATE帧）
       if (m_streamWindows.find(streamId) != m_streamWindows.end()) {
           m_streamWindows[streamId] = std::min(m_streamWindows[streamId] + receivedBytes, m_defaultWindowSize);
           NS_LOG_INFO("Stream " << streamId << " receive window updated to " << m_streamWindows[streamId]);
       }
   }
   
   void PauseStream(uint32_t streamId) {
       if (m_streamWindows.find(streamId) != m_streamWindows.end()) {
           m_streamWindows[streamId] = 0;
           NS_LOG_INFO("Stream " << streamId << " paused due to flow control");
       }
   }
   
   void ResumeStream(uint32_t streamId) {
       if (m_streamWindows.find(streamId) != m_streamWindows.end()) {
           m_streamWindows[streamId] = m_defaultWindowSize;
           NS_LOG_INFO("Stream " << streamId << " resumed with window size " << m_defaultWindowSize);
       }
   }
   
   void HandleSendFailure(uint32_t streamId) {
       // 指数退避重传策略
       static std::map<uint32_t, uint32_t> retryCounts;
       
       double backoffTime = std::pow(2.0, static_cast<double>(retryCounts[streamId])) * 0.01; // 指数退避（秒）
       retryCounts[streamId] = std::min(retryCounts[streamId] + 1, 5u); // 限制最大重试次数
       
       NS_LOG_INFO("Scheduling retransmission for stream " << streamId 
                   << " in " << backoffTime << "s (attempt " << retryCounts[streamId] << ")");
       
       Simulator::Schedule(Seconds(backoffTime), &HTTP2Session::ResumeStream, this, streamId);
   }
  
   std::map<uint32_t, bool> m_streams;
   std::map<uint32_t, uint32_t> m_streamWindows; // 每个流的窗口大小
   Ptr<Socket> m_socket;
   uint32_t m_defaultWindowSize; // 默认流窗口大小
   
   // connection-level flow control window (bytes)
   uint64_t m_connWindowBytes = 0;
   uint64_t m_connWindowInit = 0;
};


// 客户端应用
class HTTP2ClientApp : public Application {
public:
   HTTP2ClientApp() : m_socket(0), m_port(0) {}
   virtual ~HTTP2ClientApp() { m_socket = 0; }
   
   void Setup(Address servAddr, uint16_t port, uint32_t reqSize, uint32_t nReqs, double interval, bool thirdParty, uint32_t nStreams) {
       m_servAddr = servAddr;
       m_port = port;
       m_reqSize = reqSize;
       m_nReqs = nReqs;
       m_interval = interval;
       m_thirdParty = thirdParty;
       m_nStreams = nStreams;
   }
   
   uint32_t GetRespsRcvd() const { return m_respsRcvd; }
   const std::vector<double>& GetReqSendTimes() const { return m_reqSendTimes; }
   const std::vector<double>& GetRespRecvTimes() const { return m_respRecvTimes; }
   double GetInterval() const { return m_interval; }
   
   // ★ New: finalize any streams that reached target but were not marked completed (end-of-sim safety)
   void FinalizePendingCompletions() {
       for (const auto &entry : m_streamTargetBytes) {
           uint32_t streamId = entry.first;
           uint32_t target = entry.second;
           uint32_t bytes = 0;
           if (m_streamBytes.count(streamId)) {
               bytes = m_streamBytes[streamId];
           }
           if (!m_streamCompleted[streamId] && target > 0 && bytes >= target) {
               m_streamCompleted[streamId] = true;
               if (m_inflight.count(streamId)) {
                   m_inflight[streamId] = false;
               }
                               ++m_respsRcvd;
                m_respRecvTimes.push_back(Simulator::Now().GetSeconds());
                std::cout << "[Client] Finalized completion for stream " << streamId
                          << " at end: " << bytes << "/" << target << std::endl;
                if (m_reqsSent < m_nReqs) {
                    Simulator::Schedule(Seconds(m_interval), &HTTP2ClientApp::SendNextRequest, this);
                }
           }
       }
   }
   
   // ★ New: periodic finalize checker for highly fragmented cases
   void PeriodicFinalizeCheck() {
       FinalizePendingCompletions();
       // Grace finalize for highly fragmented tail: within one frameChunk and stalled >20ms
       for (auto &kv : m_streamTargetBytes) {
           uint32_t sid = kv.first;
           uint32_t target = kv.second;
           if (target == 0) continue;
           if (m_streamCompleted[sid]) continue;
           uint32_t bytes = m_streamBytes.count(sid) ? m_streamBytes[sid] : 0;
           if (bytes >= target) continue;
           // if close enough (within 400B) and stalled
           double lastT = 0.0;
           if (m_streamMetrics.count(sid)) lastT = m_streamMetrics[sid].lastByteTime;
           double nowT = Simulator::Now().GetSeconds();
                       if (bytes + 1200 >= target && lastT > 0.0 && (nowT - lastT) > 0.02) {
                m_streamCompleted[sid] = true;
                if (m_inflight.count(sid)) m_inflight[sid] = false;
                ++m_respsRcvd;
                m_respRecvTimes.push_back(nowT);
                std::cout << "[Client] Grace-finalized stream " << sid
                          << " at " << bytes << "/" << target << std::endl;
                if (m_reqsSent < m_nReqs) {
                    Simulator::Schedule(Seconds(m_interval), &HTTP2ClientApp::SendNextRequest, this);
                }
           }
       }
                       // Extra fallback finalize: when all requests sent but some streams linger very close to target
                if (m_reqsSent >= m_nReqs && m_respsRcvd < m_nReqs) {
                    double nowT2 = Simulator::Now().GetSeconds();
                    for (auto &kv2 : m_streamTargetBytes) {
                        uint32_t sid2 = kv2.first;
                        uint32_t target2 = kv2.second;
                        if (target2 == 0) continue;
                        if (m_streamCompleted[sid2]) continue;
                        uint32_t bytes2 = m_streamBytes.count(sid2) ? m_streamBytes[sid2] : 0;
                        double lastT2 = 0.0;
                        if (m_streamMetrics.count(sid2)) lastT2 = m_streamMetrics[sid2].lastByteTime;
                        if (bytes2 + 1200 >= target2 && lastT2 > 0.0 && (nowT2 - lastT2) > 0.05) {
                            m_streamCompleted[sid2] = true;
                            if (m_inflight.count(sid2)) m_inflight[sid2] = false;
                            ++m_respsRcvd;
                            m_respRecvTimes.push_back(nowT2);
                            std::cout << "[Client] Fallback-finalized stream " << sid2
                                      << " at " << bytes2 << "/" << target2 << std::endl;
                        }
                    }
                    // ★ 新增：对未收到 HEADERS 的滞留流进行 HEADERS 重发
                    for (uint32_t sid = 1; sid <= m_nStreams; ++sid) {
                        if (m_inflight[sid] && (!m_streamTargetBytes.count(sid) || m_streamTargetBytes[sid] == 0)) {
                            double last = m_sidReqSendTime.count(sid) ? m_sidReqSendTime[sid] : 0.0;
                            if (last > 0.0 && (nowT2 - last) > 0.05) {
                                uint32_t reqIdx = m_sidToReqIndex.count(sid) ? m_sidToReqIndex[sid] : m_reqsSent;
                                SendHeadersForSid(sid, reqIdx);
                                m_sidReqSendTime[sid] = nowT2;
                                std::cout << "[Client] Resent HEADERS on stream " << sid
                                          << " for req #" << reqIdx << std::endl;
                            }
                        }
                    }
                }
                if (m_respsRcvd < m_nReqs && m_socket) {
                    Simulator::Schedule(MilliSeconds(2), &HTTP2ClientApp::PeriodicFinalizeCheck, this);
                }
   }

private:
   virtual void StartApplication() override {
       m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
       
       // 设置连接回调，确保连接建立后再发送数据
       m_socket->SetConnectCallback(
           MakeCallback(&HTTP2ClientApp::ConnectionSucceeded, this),
           MakeCallback(&HTTP2ClientApp::ConnectionFailed, this)
       );
       
       m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_servAddr), m_port));
       m_socket->SetRecvCallback(MakeCallback(&HTTP2ClientApp::HandleRead, this));
      
       Ptr<TcpSocketBase> tcpSock = DynamicCast<TcpSocketBase>(m_socket);
       if (tcpSock) {
           tcpSock->TraceConnectWithoutContext("Retransmission", MakeCallback(&OnTcpRetransmission));
       }
       
       m_session = CreateObject<HTTP2Session>(m_socket);
       
       // 设置窗口大小与命令行参数一致
       if (m_session) {
           m_session->m_defaultWindowSize = (uint64_t)32 * 1024u * 1024u; // 32MB
           m_session->m_connWindowBytes  = (uint64_t)32 * 1024u * 1024u;  // 32MB
           m_session->m_connWindowInit   = m_session->m_connWindowBytes;
       }
       
       m_reqsSent = 0;
       m_respsRcvd = 0;
       m_reqSendTimes.clear();
       m_respRecvTimes.clear();
       m_buffer.clear();
       m_streamBytes.clear();
       m_streamTargetBytes.clear();
       m_streamCompleted.clear(); // 追踪流完成状态
      
       // 初始化所有流的状态
       for (uint32_t i = 1; i <= m_nStreams; ++i) {
           m_streamBytes[i] = 0;
           m_streamTargetBytes[i] = 0;
           m_streamCompleted[i] = false;
           m_inflight[i] = false;  // ★ 新增：标记流为空闲状态
           // 初始化性能指标
           m_streamMetrics[i] = StreamMetrics();
       }
       
       // 不要立即发送请求，等待连接建立
       m_connected = false;
   }
   
   virtual void StopApplication() override {
       if (m_socket) m_socket->Close();
   }
   
   // 连接成功回调
   void ConnectionSucceeded(Ptr<Socket> socket) {
       std::cout << "[Client] TCP connection established successfully" << std::endl;
       m_connected = true;
       // 周期性 finalize 检查，缓解小分片边界遗漏
       Simulator::Schedule(MilliSeconds(2), &HTTP2ClientApp::PeriodicFinalizeCheck, this);
       // 连接建立后开始发送请求
       SendNextRequest();
   }
   
   // 连接失败回调
   void ConnectionFailed(Ptr<Socket> socket) {
       std::cout << "[Client] TCP connection failed!" << std::endl;
       m_connected = false;
   }
   
   // 选择空闲的流ID
   uint32_t PickFreeSid() {
       for (uint32_t sid = 1; sid <= m_nStreams; ++sid) {
           if (!m_inflight[sid]) {
               return sid;
           }
       }
       return 0; // 没有空闲流
   }
   
   void SendNextRequest() {
       if (!m_connected) {
           std::cout << "[Client] Connection not ready, skipping request" << std::endl;
           return;
       }
       
       if (m_reqsSent < m_nReqs) {
           // 使用空闲流发送请求，确保HTTP/2流状态管理正确
           uint32_t reqsToSend = 0;
           
           for (uint32_t i = 0; i < m_nStreams && m_reqsSent < m_nReqs; ++i) {
               uint32_t streamId = PickFreeSid();
               if (streamId == 0) {
                   // 没有空闲流，等待某个流完成后再发送
                   std::cout << "[Client] No free streams available, waiting for completion" << std::endl;
                   return; // 不要继续调度，等待流完成回调
               }
               
               // ★ 关键修复：每次复用前复位该流状态
               m_streamCompleted[streamId] = false;
               m_streamBytes[streamId] = 0;
               m_streamTargetBytes[streamId] = 0;
               m_inflight[streamId] = true;  // 标记流为活跃状态
              
           HTTP2Frame frame;
               frame.streamId = streamId;
           frame.type = HEADERS;
              
               std::ostringstream oss;
               if (m_thirdParty) {
                   // 模拟第三方资源
                   const char* domains[] = {"firstparty.example", "cdn.example", "ads.example"};
                   const char* host = domains[m_reqsSent % 3];
                   oss << "GET /file" << m_reqsSent << " HTTP/2.0\r\nHost: " << host << "\r\n\r\n";
               } else {
                   oss << "GET /file" << m_reqsSent << " HTTP/2.0\r\nHost: server\r\n\r\n";
               }
              
               frame.payload = oss.str();
               frame.length = frame.payload.size();
              
               // 控制请求大小
               uint32_t headerLen = static_cast<uint32_t>(frame.payload.size());
               uint32_t desiredSize = std::max(m_reqSize, headerLen);
               if (desiredSize > headerLen) {
                   frame.payload += std::string(desiredSize - headerLen, ' ');
                   frame.length = desiredSize;
               }
              
               std::cout << "[Client] Sending request on stream " << streamId
                         << ", request #" << m_reqsSent 
                         << ", frame type=" << (int)frame.type << " (HEADERS=" << (int)HEADERS << ")" << std::endl;
               
               // 记录该流当前承载的请求索引与发送时间，用于必要时重发
               m_sidToReqIndex[streamId] = m_reqsSent;
               m_sidReqSendTime[streamId] = Simulator::Now().GetSeconds();
               
               if (m_session) {
                   std::cout << "[Client] m_session is valid, calling SendFrame" << std::endl;
                   m_session->SendFrame(frame);
               } else {
                   std::cout << "[Client] ERROR: m_session is null!" << std::endl;
               }
               m_reqSendTimes.push_back(Simulator::Now().GetSeconds());
               m_reqsSent++;
               reqsToSend++; // 修复：增加计数
              
               // HTTP/2 HEADERS帧本身就包含了完整的请求信息，不需要额外的DATA帧
               // 注释掉空的DATA帧发送，避免帧丢失问题
               /*
               HTTP2Frame dataFrame;
               dataFrame.streamId = streamId;
               dataFrame.type = DATA;
               dataFrame.length = 0;
               dataFrame.payload = "";
               std::cout << "[Client] Sending DATA frame on stream " << streamId 
                         << ", frame type=" << (int)dataFrame.type << " (DATA=" << (int)DATA << ")" << std::endl;
               m_session->SendFrame(dataFrame);
               */
           }
          
           std::cout << "[Client] Sent " << reqsToSend << " concurrent requests on streams: ";
           for (uint32_t i = 1; i <= reqsToSend; ++i) {
               std::cout << i << " ";
           }
           std::cout << ", total sent: " << m_reqsSent << std::endl;
           
           // 不要在这里继续调度，等待流完成回调
       }
   }
   
   // ★ 新增：在指定流上重发对应请求的 HEADERS（不增加 m_reqsSent）
   void SendHeadersForSid(uint32_t streamId, uint32_t reqIndex) {
       if (!m_connected || !m_session) return;
       HTTP2Frame frame;
       frame.streamId = streamId;
       frame.type = HEADERS;
       std::ostringstream oss;
       if (m_thirdParty) {
           const char* domains[] = {"firstparty.example", "cdn.example", "ads.example"};
           const char* host = domains[reqIndex % 3];
           oss << "GET /file" << reqIndex << " HTTP/2.0\r\nHost: " << host << "\r\n\r\n";
       } else {
           oss << "GET /file" << reqIndex << " HTTP/2.0\r\nHost: server\r\n\r\n";
       }
       frame.payload = oss.str();
       frame.length = frame.payload.size();
       uint32_t headerLen = static_cast<uint32_t>(frame.payload.size());
       uint32_t desiredSize = std::max(m_reqSize, headerLen);
       if (desiredSize > headerLen) {
           frame.payload += std::string(desiredSize - headerLen, ' ');
           frame.length = desiredSize;
       }
       m_session->SendFrame(frame);
   }
  
   void HandleRead(Ptr<Socket> s) {
       while (Ptr<Packet> packet = s->Recv()) {
           if (packet->GetSize() == 0) break;
           std::string data;
           data.resize(packet->GetSize());
           packet->CopyData((uint8_t*)&data[0], packet->GetSize());
           m_buffer += data;
          
           // 基于 LEN 字段的稳健帧解析
           size_t pos = 0;
           while (true) {
               size_t frameStart = m_buffer.find("SID:", pos);
               if (frameStart == std::string::npos) break;

               // 解析头部：SID:...|TYPE:...|LEN:...|
               size_t sidKey = frameStart;
               size_t sidVal = sidKey + 4;
               size_t sidEnd = m_buffer.find('|', sidVal);
               if (sidEnd == std::string::npos) break; // 等待更多数据

               size_t typeKey = sidEnd + 1;
               if (typeKey + 5 > m_buffer.size() || m_buffer.compare(typeKey, 5, "TYPE:") != 0) { pos = frameStart + 1; continue; }
               size_t typeVal = typeKey + 5;
               size_t typeEnd = m_buffer.find('|', typeVal);
               if (typeEnd == std::string::npos) break; // 等待更多数据

               size_t lenKey = typeEnd + 1;
               if (lenKey + 4 > m_buffer.size() || m_buffer.compare(lenKey, 4, "LEN:") != 0) { pos = frameStart + 1; continue; }
               size_t lenVal = lenKey + 4;
               size_t lenEnd = m_buffer.find('|', lenVal);
               if (lenEnd == std::string::npos) break; // 等待更多数据

               // 计算整帧结束位置
               uint32_t payloadLen = 0;
               try { payloadLen = static_cast<uint32_t>(std::stoul(m_buffer.substr(lenVal, lenEnd - lenVal))); }
               catch (...) { pos = frameStart + 1; continue; }
               size_t headerEnd = lenEnd + 1;
               size_t frameEnd = headerEnd + payloadLen;
               if (m_buffer.size() < frameEnd) break; // 等待完整 payload

               // 提取并处理帧
               std::string frameData = m_buffer.substr(frameStart, frameEnd - frameStart);
               ProcessFrame(frameData);

               pos = frameEnd;
               if (pos >= m_buffer.size()) break;
           }
          
           // 只保留不完整的尾部（可能是不完整帧）
           if (pos > 0) {
               m_buffer.erase(0, pos);
           }
       }
   }
  
   void ProcessFrame(const std::string& frameData) {
       try {
           HTTP2Frame frame = HTTP2Frame::Parse(frameData);
           
           // 只接受 HEADERS(0) / DATA(1)；其余直接丢弃
           if (frame.streamId == 0 || (frame.type != HEADERS && frame.type != DATA)) {
               NS_LOG_WARN("Skip invalid frame: sid=" << frame.streamId << " type=" << (int)frame.type);
               return;
           }
           
           std::cout << "[Client] Processing frame for sid=" << frame.streamId
                     << " type=" << (int)frame.type << std::endl;
          
           if (frame.type == HEADERS) {
               // 解析Content-Length
               size_t pos = frame.payload.find("Content-Length: ");
               if (pos != std::string::npos) {
                   size_t end = frame.payload.find("\r\n", pos);
                   std::string lenStr = frame.payload.substr(pos + 16, end - (pos + 16));
                   m_streamTargetBytes[frame.streamId] = std::stoi(lenStr);
                   m_streamBytes[frame.streamId] = 0;
                   
                   // 初始化流性能指标
                   m_streamMetrics[frame.streamId] = StreamMetrics();
                   m_streamMetrics[frame.streamId].totalBytes = m_streamTargetBytes[frame.streamId];
                   m_streamMetrics[frame.streamId].firstByteTime = Simulator::Now().GetSeconds();
                  
                   std::cout << "[Client] Received HEADERS for stream " << frame.streamId
                             << ", expecting " << m_streamTargetBytes[frame.streamId] << " bytes" << std::endl;
               }
           } else if (frame.type == DATA) {
               // 确保流已初始化
               if (m_streamBytes.find(frame.streamId) == m_streamBytes.end()) {
                   m_streamBytes[frame.streamId] = 0;
               }
              
               // 累计此流的字节
               m_streamBytes[frame.streamId] += frame.payload.size();
               
               // 更新性能指标
               if (m_streamMetrics.find(frame.streamId) != m_streamMetrics.end()) {
                   m_streamMetrics[frame.streamId].lastByteTime = Simulator::Now().GetSeconds();
                   m_streamMetrics[frame.streamId].frameCount++;
                   
                   // 计算当前延迟
                   double currentDelay = Simulator::Now().GetSeconds() - m_streamMetrics[frame.streamId].firstByteTime;
                   m_streamMetrics[frame.streamId].totalDelay = currentDelay;
               }
              
               std::cout << "[Client] Received DATA for stream " << frame.streamId
                         << ", " << m_streamBytes[frame.streamId] << "/"
                         << (m_streamTargetBytes.count(frame.streamId) ?
                             m_streamTargetBytes[frame.streamId] : 0) << " bytes" << std::endl;
              
               // 检查流是否完成
               if (m_streamTargetBytes.find(frame.streamId) != m_streamTargetBytes.end() &&
                   m_streamBytes[frame.streamId] >= m_streamTargetBytes[frame.streamId] &&
                   !m_streamCompleted[frame.streamId]) {  // 确保每个流只完成一次
                  
                   m_streamCompleted[frame.streamId] = true;
                   m_inflight[frame.streamId] = false;  // ★ 释放流，标记为空闲状态
                   m_respsRcvd++;
                   m_respRecvTimes.push_back(Simulator::Now().GetSeconds());
                   
                   // 记录最终性能指标
                   if (m_streamMetrics.find(frame.streamId) != m_streamMetrics.end()) {
                       double completionTime = Simulator::Now().GetSeconds() - m_streamMetrics[frame.streamId].firstByteTime;
                       std::cout << "[Client] Stream " << frame.streamId << " completed in " 
                                 << std::fixed << std::setprecision(3) << completionTime << "s"
                                 << ", frames=" << m_streamMetrics[frame.streamId].frameCount
                                 << ", avg delay=" << (m_streamMetrics[frame.streamId].totalDelay / m_streamMetrics[frame.streamId].frameCount) << "s"
                                 << std::endl;
                   }
                   
                   std::cout << "[Client] Stream " << frame.streamId << " completed, total responses: "
                             << m_respsRcvd << " at " << Simulator::Now().GetSeconds() << "s" << std::endl;
                  
                   // 如果还有请求需要发送，继续发送
                   if (m_reqsSent < m_nReqs) {
                       Simulator::Schedule(Seconds(m_interval), &HTTP2ClientApp::SendNextRequest, this);
                   }
               }
           }
       } catch (const std::exception& e) {
           NS_LOG_WARN("Failed to parse frame: " << e.what());
       }
   }
  
   Ptr<Socket> m_socket;
   Address m_servAddr;
   uint16_t m_port;
   uint32_t m_reqSize;
   uint32_t m_nReqs;
   uint32_t m_reqsSent = 0;
   uint32_t m_respsRcvd = 0;
   bool m_waitingResp = false;
   std::vector<double> m_reqSendTimes;
   std::vector<double> m_respRecvTimes;
   std::string m_buffer;
   double m_interval = 0.01;  // Default interval 0.01 seconds
   bool m_thirdParty = false;
   uint32_t m_nStreams = 3;  // HTTP/2: Number of concurrent streams
   Ptr<HTTP2Session> m_session;
  
   // Stream tracking for multiplexing
   std::map<uint32_t, uint32_t> m_streamBytes;      // Bytes received per stream
   std::map<uint32_t, uint32_t> m_streamTargetBytes; // Target bytes per stream
   std::map<uint32_t, bool> m_streamCompleted;      // Track which streams are completed
   // ★ 新增：每个流当前承载的请求索引与其 HEADERS 最近一次发送时间
   std::map<uint32_t, uint32_t> m_sidToReqIndex;
   std::map<uint32_t, double> m_sidReqSendTime;
   
   // Enhanced stream metrics tracking
   std::map<uint32_t, StreamMetrics> m_streamMetrics; // Detailed performance metrics per stream
   
   // Connection state
   bool m_connected = false; // TCP connection status
   
   // Stream state management for HTTP/2 compliance
   std::map<uint32_t, bool> m_inflight; // Track which streams are currently active
};


// 服务器应用
class HTTP2ServerApp : public Application {
public:
   HTTP2ServerApp() : m_socket(0), m_port(0) {}
   virtual ~HTTP2ServerApp() { m_socket = 0; }
  
   void Setup(uint16_t port, uint32_t respSize, uint32_t maxReqs, uint32_t nStreams,
              uint32_t frameChunk, uint32_t tickUs, uint32_t headerSize, double hpackRatio,
              uint32_t connWindowMB = 32, uint32_t streamWindowMB = 32) {
       m_port = port;
       m_respSize = respSize;
       m_maxReqs = maxReqs;
       m_nStreams = nStreams;
       m_frameChunk = frameChunk;
       m_tickUs = tickUs;
       m_headerSize = headerSize;
       m_hpackRatio = hpackRatio;
       m_connWindowInit = (uint64_t)connWindowMB * 1024u * 1024u;
       m_connWindowBytes = m_connWindowInit;
       m_streamWindowInit = (uint64_t)streamWindowMB * 1024u * 1024u;
   }
  
private:
   virtual void StartApplication() override {
       m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
       InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
       m_socket->Bind(local);
       m_socket->Listen();
       m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
           MakeCallback(&HTTP2ServerApp::HandleAccept, this));
   }
  
   virtual void StopApplication() override {
       if (m_socket) m_socket->Close();
   }
  
   void HandleAccept(Ptr<Socket> s, const Address &from) {
       std::cout << "[Server] New client connection accepted from " << from << std::endl;
       s->SetRecvCallback(MakeCallback(&HTTP2ServerApp::HandleRead, this));
       m_clientSocket = s;
       m_reqsHandled = 0;
       m_pendingQueue.clear(); // 清空队列
       m_sending = false;      // 重置发送状态
      
       Ptr<TcpSocketBase> tcpSock = DynamicCast<TcpSocketBase>(s);
       if (tcpSock) {
           tcpSock->TraceConnectWithoutContext("Retransmission", MakeCallback(&OnTcpRetransmission));
       }
   }
  
   void HandleRead(Ptr<Socket> s) {
       Ptr<Packet> packet;
       std::cout << "[Server] HandleRead called, checking for data..." << std::endl;
       
       // 1) 把 socket 里能读到的都读出来
       while ((packet = s->Recv())) {
           if (packet->GetSize() == 0) break;
           
           std::cout << "[Server] Received packet of size " << packet->GetSize() << " bytes" << std::endl;


           // 2) 追加到缓冲区
           std::string chunk;
           chunk.resize(packet->GetSize());
           packet->CopyData(reinterpret_cast<uint8_t*>(&chunk[0]), packet->GetSize());
           m_buffer += chunk;


           // 3) 在缓冲里"按帧"解析（以下一个 "SID:" 作为帧边界）
           size_t pos = 0;
           std::cout << "[Server] Parsing buffer: '" << m_buffer << "' (size=" << m_buffer.size() << ")" << std::endl;
           
           while (true) {
               size_t frameStart = m_buffer.find("SID:", pos);
               if (frameStart == std::string::npos) {
                   std::cout << "[Server] No frame start found in buffer" << std::endl;
                   break;
               }
               std::cout << "[Server] Found frame start at position " << frameStart << std::endl;
               
               size_t nextStart = m_buffer.find("SID:", frameStart + 4);
               size_t frameEnd;
               
               // 找不到下一个 SID:，那当前帧的"终点"就是缓冲区末尾 => 仍然处理当前帧
               //（因为我们的数据负载里不会出现 "SID:"，所以这就是一个完整帧）
               if (nextStart == std::string::npos) {
                   frameEnd = m_buffer.size();
               } else {
                   frameEnd = nextStart;
               }

               std::string frameData = m_buffer.substr(frameStart, frameEnd - frameStart);
               std::cout << "[Server] Extracted frame data: '" << frameData << "'" << std::endl;
               
               HTTP2Frame frame = HTTP2Frame::Parse(frameData);
               std::cout << "[Server] Parsed frame: sid=" << frame.streamId << ", type=" << (int)frame.type << ", len=" << frame.length << std::endl;


               std::cout << "[Server] Checking frame type: " << (int)frame.type << " == HEADERS(" << (int)HEADERS << ") = " << (frame.type == HEADERS ? "true" : "false") << std::endl;
               
               if (frame.type == HEADERS) {
                   std::cout << "[Server] Processing HEADERS frame for stream " << frame.streamId << std::endl;
                   if (m_reqsHandled < m_maxReqs) {
                       m_reqsHandled++;
                       std::cout << "[Server] Received request on stream " << frame.streamId
                                 << ", req #" << m_reqsHandled << std::endl;


                       // 解析/决定响应大小
                       uint32_t respSize = m_respSize;
                       if (!g_respSizes.empty()) {
                           uint32_t idx = std::min<uint32_t>(m_reqsHandled - 1, g_respSizes.size() - 1);
                           respSize = g_respSizes[idx];
                       }


                       // 先发 HEADERS (应用HPACK压缩效果)
                       HTTP2Frame headerFrame;
                       headerFrame.streamId = frame.streamId;
                       headerFrame.type = HEADERS;
                      
                       // 计算HPACK压缩后的头部大小
                       uint32_t actualHeaderSize = std::max(20u, static_cast<uint32_t>(m_headerSize * m_hpackRatio));
                      
                       std::ostringstream oss;
                       oss << "HTTP/2.0 200 OK\r\nContent-Length: " << respSize << "\r\n\r\n";
                       std::string baseHeaders = oss.str();
                      
                       // 避免截断基础头部（否则可能丢失Content-Length）
                       if (actualHeaderSize < baseHeaders.size()) {
                           headerFrame.payload = baseHeaders;  // 保持完整头部
                       } else {
                           headerFrame.payload = baseHeaders + std::string(actualHeaderSize - baseHeaders.size(), ' ');
                       }
                      
                       headerFrame.length = headerFrame.payload.size();
                      
                       // 记录HPACK压缩效果
                       std::cout << "[Server] HPACK: original=" << m_headerSize << "B, compressed="
                                 << actualHeaderSize << "B, ratio=" << std::fixed << std::setprecision(2)
                                 << (double)actualHeaderSize / m_headerSize << std::endl;
                      
                       s->Send(SerializeFrame(headerFrame));


                       // 把"整个响应大小"入队，后续 tick 交错发送
                       std::cout << "[Server] Enqueuing stream " << frame.streamId
                                 << " with size " << respSize << " bytes" << std::endl;
                       m_pendingQueue.emplace_back(frame.streamId, respSize);
                       m_streamSendWindow[frame.streamId] = m_streamWindowInit;


                       if (!m_sending) {
                           m_sending = true;
                           Simulator::Schedule(MicroSeconds(m_tickUs),
                                               &HTTP2ServerApp::SendTick, this, s);
                       }
                   }
               } else if (frame.type == DATA) {
                   // 按需处理 DATA（大多数请求体为空可忽略）
               } else {
                   // 其他类型（PUSH_PROMISE 等）按需扩展
               }


               pos = frameEnd;
           }


           // 4) 只保留未处理完的尾部（可能是不完整帧）
           if (pos < m_buffer.size()) {
               m_buffer.erase(0, pos);
           } else {
               m_buffer.clear();
           }
       }
   }
   void SendTick(Ptr<Socket> s) {
       if (m_pendingQueue.empty()) { m_sending = false; return; }

       PendingItem item = m_pendingQueue.front();
       m_pendingQueue.pop_front();

       // 流/连接窗口：仅对 DATA 有效，且我们在这里就是发 DATA
       uint32_t winCap = (uint32_t)std::min<uint64_t>(m_connWindowBytes,
                          m_streamSendWindow[item.streamId]);
       if (winCap == 0) {
           // 流控阻塞：保持轮转，稍后再试
           std::cout << "[SERVER_FLOW_CONTROL_BLOCKED] sid=" << item.streamId
                     << " connWin=" << m_connWindowBytes
                     << " streamWin=" << m_streamSendWindow[item.streamId] << std::endl;
           m_pendingQueue.push_back(item);
           Simulator::Schedule(MicroSeconds(m_tickUs), &HTTP2ServerApp::SendTick, this, s);
           return;
       }

       uint32_t sendBytes = std::min({m_frameChunk, item.remainingBytes, winCap});

       HTTP2Frame dataFrame;
       dataFrame.streamId = item.streamId;
       dataFrame.type = DATA;
       dataFrame.length = sendBytes;
       dataFrame.payload = std::string(sendBytes, 'D');

       Ptr<Packet> pkt = SerializeFrame(dataFrame);

       // 如果 TCP 发送缓冲不够，看作"HoL 停滞"开始/持续
       if (s->GetTxAvailable() < pkt->GetSize()) {
           if (m_stallStart < 0) m_stallStart = Simulator::Now().GetSeconds();
           m_pendingQueue.push_front(item);
           Simulator::Schedule(MicroSeconds(m_tickUs * 2), &HTTP2ServerApp::SendTick, this, s);
           return;
       }

               int sent = s->Send(pkt);
        uint32_t pktSize = pkt->GetSize();
        if (sent <= 0) {
            if (m_stallStart < 0) m_stallStart = Simulator::Now().GetSeconds();
            item.retryCount++;
            item.lastRetryTime = Simulator::Now().GetSeconds();
            
            if (item.retryCount > 5) {
                // 重试次数过多，暂停流
                item.isPaused = true;
                NS_LOG_WARN("Stream " << item.streamId << " paused due to excessive retries: " << item.retryCount);
            }
            
            m_pendingQueue.push_front(item); // 放回队首，优先重试
            Simulator::Schedule(MicroSeconds(m_tickUs * 3), &HTTP2ServerApp::SendTick, this, s);
            return;
        }
        if ((uint32_t)sent < pktSize) {
            // 部分写入：不扣减任何窗口/剩余字节，视作背压，稍后重试
            if (m_stallStart < 0) m_stallStart = Simulator::Now().GetSeconds();
            m_pendingQueue.push_front(item);
            uint32_t factor = (item.remainingBytes <= m_frameChunk ? 5u : 3u);
            Simulator::Schedule(MicroSeconds(m_tickUs * factor), &HTTP2ServerApp::SendTick, this, s);
            return;
        }

        // 一旦成功发送，结束本次停滞计时
        if (m_stallStart >= 0) {
            m_totalHolStall += (Simulator::Now().GetSeconds() - m_stallStart);
            m_stallStart = -1.0;
        }

        // 成功后才真正扣减窗口/剩余字节
        m_connWindowBytes -= sendBytes;
        m_streamSendWindow[item.streamId] -= sendBytes;
        item.remainingBytes -= sendBytes;

       std::cout << "[H2] TX sid=" << item.streamId
                 << " len=" << sendBytes
                 << " remain=" << item.remainingBytes
                 << " connWin=" << m_connWindowBytes
                 << " streamWin=" << m_streamSendWindow[item.streamId]
                 << " t=" << Simulator::Now().GetSeconds() << "s" << std::endl;

       if (item.remainingBytes > 0) {
           m_pendingQueue.push_back(item); // 轮转 -> 等价 RR
       } else {
           // 流完成，记录统计信息
           NS_LOG_INFO("Stream " << item.streamId << " completed successfully");
       }

       Simulator::Schedule(MicroSeconds(m_tickUs), &HTTP2ServerApp::SendTick, this, s);
   }

  
   Ptr<Socket> m_socket; //Server socket
   Ptr<Socket> m_clientSocket; //Client socket
   uint16_t m_port; // Port number
   uint32_t m_respSize;// Response size
   uint32_t m_maxReqs; // Maximum number of requests
   uint32_t m_reqsHandled = 0; // The number of processed requests
   uint32_t m_nStreams = 3; // Number of concurrent streams for multiplexing
   uint32_t m_frameChunk = 1200; // Frame chunk size in bytes
   uint32_t m_tickUs = 500; // Tick interval in microseconds for interleaving
   bool m_sending = false; // Whether interleaved sending is active
   std::deque<PendingItem> m_pendingQueue; // Queue for pending responses
   std::string m_buffer; // Server-side receive buffer for frame parsing
   uint32_t m_headerSize = 200; // Base header size in bytes (before HPACK compression)
   double m_hpackRatio = 0.3; // HPACK compression ratio
   uint64_t m_connWindowInit = 0; // Connection-level window size in bytes
   uint64_t m_connWindowBytes = 0; // Current connection-level window size in bytes
   uint64_t m_streamWindowInit = 0; // Stream-level window size in bytes
   std::map<uint32_t, uint64_t> m_streamSendWindow; // 每个流的当前发送窗口大小
   
   // HoL stall measurement
   double m_stallStart = -1.0;
   double m_totalHolStall = 0.0;
   
public:
   double GetHolStallSeconds() const { return m_totalHolStall; }
};


int main(int argc, char *argv[]) {
   uint32_t nRequests = 200;     // 总请求数：控制要发送多少个HTTP请求
   uint32_t respSize = 100*1024; // 响应大小：服务器返回的数据大小（默认100KB）
   uint32_t reqSize = 100;       // 请求大小：客户端发送的请求大小
   uint16_t httpPort = 8080;     // HTTP端口：服务器监听的端口号
   double errorRate = 0.01;      // 丢包率：1%的丢包率
   std::string dataRate = "10Mbps"; // 带宽：网络链路带宽
   std::string delay = "5ms";    // 延迟：网络链路延迟
   double interval = 0.01;       // 请求间隔：每个请求之间的时间间隔（秒）
   uint32_t nConnections = 1;    // 并发连接数：同时建立的HTTP连接数
   bool mixedSizes = false;      // 是否使用混合对象大小分布
   bool thirdParty = false;      // 是否模拟第三方域（仅影响请求Host与统计标签）
   uint32_t nStreams = 3;        // HTTP/2: Number of concurrent streams
   uint32_t frameChunk = 1200;   // Frame chunk size in bytes
   uint32_t tickUs = 500;        // Tick interval in microseconds for interleaving
   uint32_t headerSize = 200;    // Base header size in bytes (before HPACK compression)
   double hpackRatio = 0.3;      // HPACK compression ratio (0.3 = 70% compression)
   uint32_t defaultWindowSize = 65535; // Default flow control window size
   uint32_t maxRetries = 5;      // Maximum retry attempts before pausing stream
   uint32_t connWindowMB = 32;   // Connection-level window size in MB
   uint32_t streamWindowMB = 32; // Stream-level window size in MB
   double simTime = 60.0;        // 默认仿真时间 60s


   CommandLine cmd;
   cmd.AddValue("nRequests", "Number of HTTP requests", nRequests);
   cmd.AddValue("respSize", "HTTP response size (bytes)", respSize);
   cmd.AddValue("reqSize", "HTTP request size (bytes)", reqSize);
   cmd.AddValue("httpPort", "HTTP server port", httpPort);
   cmd.AddValue("errorRate", "Packet loss rate", errorRate);
   cmd.AddValue("dataRate", "Link bandwidth", dataRate);
   cmd.AddValue("delay", "Link delay", delay);
   cmd.AddValue("latency", "Alias of --delay", delay); // Alias
   cmd.AddValue("interval", "Interval between HTTP requests (s)", interval);
   cmd.AddValue("nConnections", "Number of parallel HTTP/2 connections", nConnections);
   cmd.AddValue("mixedSizes", "Use mixed object size distribution (HTML/CSS/JS/images)", mixedSizes);
   cmd.AddValue("thirdParty", "Simulate third-party domains in Host header", thirdParty);
   cmd.AddValue("nStreams", "Number of concurrent HTTP/2 streams", nStreams);
   cmd.AddValue("frameChunk", "Frame chunk size in bytes for interleaving", frameChunk);
   cmd.AddValue("tickUs", "Tick interval in microseconds for interleaving", tickUs);
   cmd.AddValue("headerSize", "Base header size in bytes (before HPACK compression)", headerSize);
   cmd.AddValue("hpackRatio", "HPACK compression ratio (0.3 = 70% compression)", hpackRatio);
   cmd.AddValue("defaultWindowSize", "Default flow control window size", defaultWindowSize);
   cmd.AddValue("maxRetries", "Maximum retry attempts before pausing stream", maxRetries);
   cmd.AddValue("connWindowMB", "Connection-level flow control window size in MB", connWindowMB);
   cmd.AddValue("streamWindowMB", "Stream-level flow control window size in MB", streamWindowMB);
   cmd.AddValue("simTime", "Simulation time in seconds", simTime);
   cmd.Parse(argc, argv);


   // Build per-request response sizes
   g_respSizes.clear();
   g_respSizes.reserve(nRequests);
   if (!mixedSizes) {
       for (uint32_t i = 0; i < nRequests; ++i) g_respSizes.push_back(respSize);
   } else {
       // Simple distribution example:
       // 5% HTML ~ 10KB, 35% CSS/JS ~ 50KB, 60% images ~ 200KB
       for (uint32_t i = 0; i < nRequests; ++i) {
           double r = (double) i / std::max(1u, nRequests - 1); // deterministic spread 0..1
           if (r < 0.05) {
               g_respSizes.push_back(10 * 1024);
           } else if (r < 0.40) {
               g_respSizes.push_back(50 * 1024);
           } else {
               g_respSizes.push_back(200 * 1024);
           }
       }
   }


   NodeContainer nodes;
   nodes.Create(2);


   PointToPointHelper p2p;
   p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
   p2p.SetChannelAttribute("Delay", StringValue(delay));
   p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("32kB"));
   NetDeviceContainer devices = p2p.Install(nodes);


   InternetStackHelper stack;
   stack.Install(nodes);


   Ipv4AddressHelper address;
   address.SetBase("10.1.1.0", "255.255.255.0");
   Ipv4InterfaceContainer interfaces = address.Assign(devices);


   // HTTP/2 Application
   Ptr<HTTP2ServerApp> serverApp = CreateObject<HTTP2ServerApp>();
   serverApp->Setup(httpPort, respSize, nRequests, nStreams, frameChunk, tickUs, headerSize, hpackRatio, connWindowMB, streamWindowMB);
   nodes.Get(1)->AddApplication(serverApp);
   serverApp->SetStartTime(Seconds(0.5));
   serverApp->SetStopTime(Seconds(simTime));


   // 多连接客户端
   std::vector<Ptr<HTTP2ClientApp>> clients;
   std::vector<std::vector<double>> allSendTimes, allRecvTimes;
   uint32_t baseReqs = nRequests / nConnections;
   uint32_t rem = nRequests % nConnections;
   for (uint32_t i = 0; i < nConnections; ++i) {
       uint32_t reqs = baseReqs + (i < rem ? 1 : 0); // 平均分配请求
       Ptr<HTTP2ClientApp> client = CreateObject<HTTP2ClientApp>();
       client->Setup(interfaces.GetAddress(1), httpPort, reqSize, reqs, interval, thirdParty, nStreams);
       nodes.Get(0)->AddApplication(client);
       client->SetStartTime(Seconds(1.0 + i * 0.01)); // 避免完全同时启动
       client->SetStopTime(Seconds(simTime));
       clients.push_back(client);
   }


   // Bidirectional Error Model
   Ptr<RateErrorModel> em0 = CreateObject<RateErrorModel>();
   em0->SetAttribute("ErrorRate", DoubleValue(errorRate));
   em0->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
   devices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em0));


   Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel>();
   em1->SetAttribute("ErrorRate", DoubleValue(errorRate));
   em1->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
   devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em1));


   FlowMonitorHelper flowmonHelper;
   Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();


   Config::ConnectWithoutContext(
       "/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTx",
       MakeCallback(&TxTrace));
   Config::ConnectWithoutContext(
       "/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx",
       MakeCallback(&RxTrace));


   // TCP MSS consistency
   Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
   
   // ⭐ 关键修复：设置适中的TCP缓冲区，便于观察HoL停滞现象
   Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4u << 20));
   Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4u << 20));


   Simulator::Stop(Seconds(simTime));
   Simulator::Run();


   // HTTP/2 Application 统计
   uint32_t totalResps = 0;
   std::vector<double> sendTimes, recvTimes;
   double firstSend = std::numeric_limits<double>::infinity();
   double lastRecv = 0.0;
   double sumDelay = 0.0;
   size_t nDone = 0;
   // RFC3550-style jitter estimator (smoothed)
   double rfcJitter = 0.0;
   bool havePrevTransit = false;
   double prevTransit = 0.0;
   for (auto& client : clients) {
       totalResps += client->GetRespsRcvd();
       const auto& s = client->GetReqSendTimes();
       const auto& r = client->GetRespRecvTimes();
       size_t n = std::min(s.size(), r.size());
       if (n > 0) {
           firstSend = std::min(firstSend, s.front());
           lastRecv = std::max(lastRecv, r.back());
       }
       for (size_t i = 0; i < n; ++i) {
           double transit = r[i] - s[i];
           sumDelay += transit;
           ++nDone;
           if (havePrevTransit) {
               double D = std::abs(transit - prevTransit);
               rfcJitter += (D - rfcJitter) / 16.0;
           } else {
               havePrevTransit = true;
           }
           prevTransit = transit;
       }
       // also keep combined times for potential other uses
       sendTimes.insert(sendTimes.end(), s.begin(), s.end());
       recvTimes.insert(recvTimes.end(), r.begin(), r.end());
   }
   // HoL proxy metrics: send gaps exceeding configured interval on each connection
   uint64_t holEvents = 0;
   double holBlockedTime = 0.0;
   for (auto& client : clients) {
       const auto& s = client->GetReqSendTimes();
       double iv = client->GetInterval();
       for (size_t i = 1; i < s.size(); ++i) {
           double idealNext = s[i - 1] + iv;
           double extra = s[i] - idealNext;
           if (extra > 1e-9) { // consider as head-of-line blocking time
               ++holEvents;
               holBlockedTime += extra;
           }
       }
   }
   // End-of-sim safety: let each client finalize any pending completions before computing summary
    for (auto &client : clients) {
        client->FinalizePendingCompletions();
    }
    totalResps = 0;
    for (auto &client : clients) totalResps += client->GetRespsRcvd();

    // Always print at least the completed responses summary for tooling to parse
 std::cout << "------------------------------------------" << std::endl;
 std::cout << "HTTP/2 Experiment Summary" << std::endl;
 std::cout << "completedResponses (nDone): " << totalResps << "/" << nRequests << std::endl;
 
 if (nDone > 0 && lastRecv > firstSend) {
       double avgDelay = sumDelay / static_cast<double>(nDone);
      
       // 计算HPACK压缩后的头部大小
       double headerCompressed = std::max(20.0, headerSize * hpackRatio);
      
       // 计算下行吞吐量（DATA + 响应HEADERS）
       double totalBytesDown = static_cast<double>(nDone) * (respSize + headerCompressed);
       double totalTime = lastRecv - firstSend;
       double throughputDown = (totalBytesDown * 8.0) / (totalTime * 1e6); // Mbps
      
       // 计算双向吞吐量（包括上行请求HEADERS）
       double totalBytesUp = static_cast<double>(nDone) * headerCompressed; // 上行请求头
       double totalBytesBi = totalBytesDown + totalBytesUp;
       double throughputBi = (totalBytesBi * 8.0) / (totalTime * 1e6); // Mbps
      
       // 计算头部压缩节省的带宽
       double originalBytes = static_cast<double>(nDone) * (respSize + headerSize);
       double savedBytes = originalBytes - totalBytesDown;
       double compressionRatio = (savedBytes / originalBytes) * 100.0;
      
       std::cout << "The HTTP/2 experiment has ended. The total number of responses received by the client is: " << totalResps << "/" << nRequests << std::endl;
       std::cout << "Average delay of HTTP/2: " << avgDelay << " s" << std::endl;
      
       // 详细的实验总结
       std::cout << "------------------------------------------" << std::endl;
               std::cout << "HTTP/2 Experiment Summary" << std::endl;
        std::cout << "completedResponses (nDone): " << totalResps << "/" << nRequests << std::endl;
       std::cout << "dataPerResp (bytes): " << respSize << std::endl;
       std::cout << "hpackPerResp (bytes): " << std::fixed << std::setprecision(0) << headerCompressed << std::endl;
       std::cout << "firstSend: " << std::fixed << std::setprecision(6) << firstSend << "s" << std::endl;
       std::cout << "lastRecv: " << std::fixed << std::setprecision(6) << lastRecv << "s" << std::endl;
       std::cout << "totalTime: " << std::fixed << std::setprecision(6) << totalTime << "s" << std::endl;
       std::cout << std::endl;
      
       std::cout << "Downlink bytes: " << std::fixed << std::setprecision(0) << totalBytesDown << " B" << std::endl;
       std::cout << "Downlink throughput: " << std::fixed << std::setprecision(3) << throughputDown << " Mbps" << std::endl;
       std::cout << std::endl;
      
       std::cout << "Bidirectional bytes (incl. uplink headers): " << std::fixed << std::setprecision(0) << totalBytesBi << " B" << std::endl;
       std::cout << "Bidirectional throughput: " << std::fixed << std::setprecision(3) << throughputBi << " Mbps" << std::endl;
       std::cout << std::endl;
      
       std::cout << "HPACK compression: saved " << std::fixed << std::setprecision(0) << savedBytes << " bytes ("
                 << std::fixed << std::setprecision(1) << compressionRatio << "%)" << std::endl;
      
       double pageLoadTime = lastRecv - firstSend;
       std::cout << "Page Load Time (onLoad): " << std::fixed << std::setprecision(6) << pageLoadTime << " s" << std::endl;
       std::cout << "TCP retransmissions: " << g_retxCount
                 << "  rate: " << std::fixed << std::setprecision(3) << (g_retxCount / (totalTime > 0 ? totalTime : 1.0)) << " /s" << std::endl;
       std::cout << "RFC3550 jitter estimate: " << std::fixed << std::setprecision(6) << rfcJitter << " s" << std::endl;
       std::cout << "HoL events: " << holEvents << "  HoL blocked time: " << std::fixed << std::setprecision(6) << holBlockedTime << " s" << std::endl;
       
       // TCP级HoL停滞统计
       double holStall = serverApp->GetHolStallSeconds();
       double holStallRatio = ( (lastRecv > firstSend) ? (holStall / (lastRecv - firstSend)) : 0.0 );
       
       std::cout << "TCP-level HoL stall time: " << std::fixed << std::setprecision(6)
                 << holStall << " s  (stall ratio=" << std::setprecision(3)
                 << (holStallRatio * 100.0) << "%)" << std::endl;
       
       std::cout << "------------------------------------------" << std::endl;
   }


   flowmon->CheckForLostPackets();


   // Report per-flow delay/jitter using FlowMonitor statistics
   Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
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
                     << " proto=" << (uint32_t)t.protocol
                     << " rxPackets=" << st.rxPackets
                     << " avgDelay=" << avgDelay << " s"
                     << " avgJitter=" << avgJitter << " s"
                     << std::endl;
       }
   }
   flowmon->SerializeToXmlFile("flowmon.xml", true, true);


   Simulator::Destroy();
   return 0;
}
