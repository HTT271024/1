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
#include "ns3/tcp-header.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Http1Dot1Sim");

//TCP count
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

// listen to TCP -- server 
class HttpServerApp : public Application {
public:
//Constructors and destructors
  HttpServerApp() : m_socket(0), m_port(0) {}
  virtual ~HttpServerApp() { m_socket = 0; }
//Setup function External configuration interface
  void Setup(uint16_t port, uint32_t respSize, uint32_t maxReqs, uint32_t respHdrBytes) {
    m_port = port;
    m_respSize = respSize;
    m_maxReqs = maxReqs;
    m_respHdrBytes = respHdrBytes;
  }

//Create a TCP listening socket
private:
  virtual void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                MakeCallback(&HttpServerApp::HandleAccept, this));
  }

//stop listen to socket and TCP return call 
  virtual void StopApplication() override {
    if (m_socket) m_socket->Close();
  }
  //record client socket 
  void HandleAccept(Ptr<Socket> s, const Address &from) {
    s->SetRecvCallback(MakeCallback(&HttpServerApp::HandleRead, this));
    m_clientSocket = s;
    m_reqsHandledMap[s] = 0; // per-socket counter
    Ptr<TcpSocketBase> tcpSock = DynamicCast<TcpSocketBase>(s);
    if (tcpSock) {
      // 确保 trace 签名匹配
      tcpSock->TraceConnectWithoutContext("Retransmission", MakeCallback(&OnTcpRetransmission));
    }
  }
  //HandleRead function
  void HandleRead(Ptr<Socket> s) {
    Ptr<Packet> packet = s->Recv();
    if (!packet || packet->GetSize() == 0) return;

    // 每个 socket 的已处理请求计数
    uint32_t &m_reqsHandled = m_reqsHandledMap[s];

    if (m_reqsHandled < m_maxReqs) {
      m_reqsHandled++;
      uint32_t rIdx = g_respSizes.empty() ? 0 : std::min<uint32_t>(m_reqsHandled - 1, g_respSizes.size() - 1);
      uint32_t thisRespSize = g_respSizes.empty() ? m_respSize : g_respSizes[rIdx];
      
      std::ostringstream oss;
      oss << "HTTP/1.1 200 OK\r\n"
          << "Server: ns3-http1/0.1\r\n"
          << "Content-Type: application/octet-stream\r\n"
          << "Content-Length: " << thisRespSize << "\r\n"
          << "Connection: keep-alive\r\n";
      std::string base = oss.str();
      size_t need = (m_respHdrBytes > base.size() + 4) ? (m_respHdrBytes - (base.size() + 4)) : 0;
      if (need > 0) oss << "X-Fill: " << std::string(need, 'y') << "\r\n";
      oss << "\r\n";
      std::string header = oss.str();

      // const-correct packet creation
      Ptr<Packet> resp = Create<Packet>(reinterpret_cast<const uint8_t*>(header.data()), header.size());
      Ptr<Packet> body = Create<Packet>(thisRespSize);
      s->Send(resp);
      s->Send(body);
      NS_LOG_INFO("[Server] Sent response " << m_reqsHandled << ", size=" << thisRespSize << ", header size=" << header.size());
    }
  }
  //set up 
  Ptr<Socket> m_socket; //Server socket
  Ptr<Socket> m_clientSocket; //Client socket
  uint16_t m_port; // Port number
  uint32_t m_respSize;// Response size
  uint32_t m_maxReqs; // Maximum number of requests
  std::map<Ptr<Socket>, uint32_t> m_reqsHandledMap; // 替换原来的 uint32_t m_reqsHandled = 0;
  uint32_t m_respHdrBytes; // Fixed response header size
};


// ===================== Client =====================
class HttpClientApp : public Application {
public:
  HttpClientApp() : m_socket(0), m_port(0) {}
  virtual ~HttpClientApp() { m_socket = 0; }

//HttpClientApp setup function
  void Setup(Address servAddr, uint16_t port, uint32_t reqSize, uint32_t nReqs, double interval, bool thirdParty, uint32_t reqHdrBytes) {
    m_servAddr = servAddr;
    m_port = port;
    m_reqSize = reqSize;
    m_nReqs = nReqs;
    m_interval = interval;
    m_thirdParty = thirdParty;
    m_reqHdrBytes = reqHdrBytes;
  }

  uint32_t GetRespsRcvd() const { return m_respsRcvd; }
  const std::vector<double>& GetReqSendTimes() const { return m_reqSendTimes; }
  const std::vector<double>& GetRespRecvTimes() const { return m_respRecvTimes; }
  double GetInterval() const { return m_interval; }
  const std::vector<uint32_t>& GetDoneSizes() const { return m_doneSizes; }

//create TCP socket
private:
  virtual void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_servAddr), m_port));
    m_socket->SetRecvCallback(MakeCallback(&HttpClientApp::HandleRead, this));
    
    // 禁用Nagle算法，减少应用层引入的HOL干扰
    Ptr<TcpSocketBase> tcpSock = DynamicCast<TcpSocketBase>(m_socket);
    if (tcpSock) {
      tcpSock->SetAttribute("TcpNoDelay", BooleanValue(true));
      tcpSock->TraceConnectWithoutContext("Retransmission", MakeCallback(&OnTcpRetransmission));
    }

//Send the first request immediately
    m_reqsSent = 0;
    m_respsRcvd = 0;
    m_reqSendTimes.clear();
    m_respRecvTimes.clear();
    m_buffer.clear();
    m_waitingResp = false;
    m_bytesToRecv = 0;
    m_bodyStart = 0;
    SendNextRequest();
  }

//stop connection
  virtual void StopApplication() override {
    if (m_socket) m_socket->Close();
  }
  //Construct the HTTP/1.1 request line and the Host header
  void SendNextRequest() {
    if (m_reqsSent < m_nReqs) {
      // 构造固定大小的请求头
      std::ostringstream oss;
      if (m_thirdParty) {
        // Alternate among domains to mimic third-party resources
        const char* domains[] = {"firstparty.example", "cdn.example", "ads.example"};
        const char* host = domains[m_reqsSent % 3];
        oss << "GET /file" << m_reqsSent << " HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Connection: keep-alive\r\n";
      } else {
        oss << "GET /file" << m_reqsSent << " HTTP/1.1\r\n"
            << "Host: server\r\n"
            << "Connection: keep-alive\r\n";
      }
      
      // 添加常见头部，便于凑到稳定尺寸
      oss << "User-Agent: ns3-http1/0.1\r\n"
          << "Accept: */*\r\n";
      
      // 填充X-Fill头部到目标大小
      std::string base = oss.str();
      size_t need = (m_reqHdrBytes > base.size() + 4) ? (m_reqHdrBytes - (base.size() + 4)) : 0;
      if (need > 0) {
        oss << "X-Fill: " << std::string(need, 'x') << "\r\n";
      }
      oss << "\r\n";

      std::string header = oss.str();
      uint32_t headerLen = static_cast<uint32_t>(header.size());
      
      // 请求最小长度控制（保留原有逻辑）
      uint32_t desiredSize = std::max(m_reqSize, headerLen);

      // 创建 packet 时使用 const 指针
      Ptr<Packet> p = Create<Packet>(reinterpret_cast<const uint8_t*>(header.data()), headerLen);
      if (desiredSize > headerLen) {
        Ptr<Packet> padding = Create<Packet>(desiredSize - headerLen);
        p->AddAtEnd(padding);
      }
      // record the time of sending request 
      m_socket->Send(p);
      m_reqSendTimes.push_back(Simulator::Now().GetSeconds());
      m_reqsSent++;
      m_waitingResp = true;
      m_bytesToRecv = 0;
      m_bytesRcvd = 0;
      NS_LOG_INFO("[Client] Sent request " << m_reqsSent << ", header size=" << headerLen);
    }
  }
  //read all the readable data in socket 
  void HandleRead(Ptr<Socket> s) {
    while (Ptr<Packet> packet = s->Recv()) {
      if (packet->GetSize() == 0) break;
      std::string data;
      data.resize(packet->GetSize());
      packet->CopyData((uint8_t*)&data[0], packet->GetSize());
      m_buffer += data;

      while (m_waitingResp) {
        if (m_bytesToRecv == 0) {
          size_t headerEnd = m_buffer.find("\r\n\r\n");
          if (headerEnd == std::string::npos) break;
            size_t pos = m_buffer.find("Content-Length: ");
          if (pos == std::string::npos) break;
              size_t end = m_buffer.find("\r\n", pos);
          if (end == std::string::npos || end <= pos + 16) break;
              std::string lenStr = m_buffer.substr(pos + 16, end - (pos + 16));
          try {
            m_bytesToRecv = static_cast<uint32_t>(std::stoul(lenStr));
          } catch (...) {
            // 解析失败，放弃此次循环，等待更多数据
            break;
          }
              m_bodyStart = headerEnd + 4;
        }
        if (m_bytesToRecv > 0) {
          size_t bodyBytes = (m_buffer.size() > m_bodyStart) ? (m_buffer.size() - m_bodyStart) : 0;
          if (bodyBytes >= m_bytesToRecv) {
            m_respsRcvd++;
            m_waitingResp = false;
            m_respRecvTimes.push_back(Simulator::Now().GetSeconds());
            // 记录实际接收的响应大小
            m_doneSizes.push_back(m_bytesToRecv);
            std::cout << "[Client] Received response " << m_respsRcvd << " at " << Simulator::Now().GetSeconds() << "s, size=" << m_bytesToRecv << " bytes" << std::endl;
            if (m_respsRcvd < m_nReqs) {
              Simulator::Schedule(Seconds(m_interval), &HttpClientApp::SendNextRequest, this);
            }
            // 安全地截断 buffer
            size_t cutPos = m_bodyStart + m_bytesToRecv;
            if (cutPos <= m_buffer.size()) {
              m_buffer = m_buffer.substr(cutPos);
            } else {
              m_buffer.clear();
            }
            m_bytesToRecv = 0;
            m_bodyStart = 0;
          } else break;
        }
      }
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
  uint32_t m_bytesToRecv = 0;
  uint32_t m_bytesRcvd = 0;
  std::vector<double> m_reqSendTimes;
  std::vector<double> m_respRecvTimes;
  std::string m_buffer;
  uint32_t m_bodyStart = 0;
  double m_interval = 0.01;  // 默认间隔为 0.01 秒
  bool m_thirdParty = false;
  uint32_t m_reqHdrBytes; // Fixed request header size
  std::vector<uint32_t> m_doneSizes; // 记录每个响应的实际接收大小
};

// ===================== Main =====================

int main(int argc, char *argv[]) {
  // 清零变量
  g_retxCount = 0;
  g_respSizes.clear();
  
  // 调整仿真时间：基于页面完成时间，而不是固定30s
  double simTime = 35.0;  // 默认35s，足够完成页面请求
  
  // HTTP/1.1 参数
  uint32_t nRequests = 20;
  uint32_t respSize = 102400;  // 100KB
  uint32_t reqSize = 1024;     // 1KB
  uint16_t httpPort = 8080;
  double errorRate = 0.01;     // 1% packet loss
  std::string dataRate = "10Mbps";
  std::string delay = "10ms";
  double interval = 0.00;      // 50ms between requests
  uint32_t nConnections = 1;   // HTTP/1.1: single connection
  bool mixedSizes = false;     // fixed size responses
  bool thirdParty = false;     // single domain
  uint32_t reqHdrBytes = 256;  // fixed request header
  uint32_t respHdrBytes = 256; // fixed response header

  CommandLine cmd;
  cmd.AddValue("nRequests", "Number of HTTP requests", nRequests);
  cmd.AddValue("respSize", "HTTP response size (bytes)", respSize);
  cmd.AddValue("reqSize", "HTTP request size (bytes)", reqSize);
  cmd.AddValue("httpPort", "HTTP server port", httpPort);
  cmd.AddValue("errorRate", "Packet loss rate", errorRate);
  cmd.AddValue("dataRate", "Link bandwidth", dataRate);
  cmd.AddValue("delay", "Link delay", delay);
  cmd.AddValue("latency", "Alias of --delay", delay); //别名
  cmd.AddValue("interval", "Interval between HTTP requests (s)", interval);
  cmd.AddValue("nConnections", "Number of parallel HTTP/1.1 connections", nConnections);
  cmd.AddValue("mixedSizes", "Use mixed object size distribution (HTML/CSS/JS/images)", mixedSizes);
  cmd.AddValue("thirdParty", "Simulate third-party domains in Host header", thirdParty);
  cmd.AddValue("reqHdrBytes", "Fixed request header size (bytes)", reqHdrBytes);
  cmd.AddValue("respHdrBytes", "Fixed response header size (bytes)", respHdrBytes);
  cmd.Parse(argc, argv);

  //构造每个请求的响应体大小数组
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

//Create two nodes: one for the client and one for the server
  NodeContainer nodes;
  nodes.Create(2);

//point to point link -- droptail queue is first in first out
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));
  p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("32kB"));
  NetDeviceContainer devices = p2p.Install(nodes);

//install
  InternetStackHelper stack;
  stack.Install(nodes);

//IPv4 address
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // HTTP/1.1 Application
  Ptr<HttpServerApp> serverApp = CreateObject<HttpServerApp>();
  serverApp->Setup(httpPort, respSize, nRequests, respHdrBytes);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.5));
  serverApp->SetStopTime(Seconds(simTime));  // 使用动态仿真时间

  // 多连接客户端
  std::vector<Ptr<HttpClientApp>> clients;
  std::vector<std::vector<double>> allSendTimes, allRecvTimes;
  uint32_t baseReqs = nRequests / nConnections;
  uint32_t rem = nRequests % nConnections;
  for (uint32_t i = 0; i < nConnections; ++i) {
    uint32_t reqs = baseReqs + (i < rem ? 1 : 0); // 平均分配请求
    Ptr<HttpClientApp> client = CreateObject<HttpClientApp>();
    client->Setup(interfaces.GetAddress(1), httpPort, reqSize, reqs, interval, thirdParty, reqHdrBytes);
    nodes.Get(0)->AddApplication(client);
    client->SetStartTime(Seconds(1.0 + i * 0.01)); // 避免完全同时启动
    client->SetStopTime(Seconds(simTime));  // 使用动态仿真时间
    clients.push_back(client);
  }

// packet loss model for both client and server
  Ptr<RateErrorModel> em0 = CreateObject<RateErrorModel>();
  em0->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em0->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
  devices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em0));

  Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel>();
  em1->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em1->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
  devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em1));

//install flow monitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();


//链路层发/收事件挂到你的 TxTrace/RxTrace
  Config::ConnectWithoutContext(
    "/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTx",
    MakeCallback(&TxTrace));
  Config::ConnectWithoutContext(
    "/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx",
    MakeCallback(&RxTrace));


  // TCP MSS consistency
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));

  // 设置足够大的TCP缓冲区大小，确保能容纳最大的响应
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(256 * 1024));  // 256KB发送缓冲
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(256 * 1024));  // 256KB接收缓冲
  
  // 使用更合适的拥塞控制算法
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // HTTP/1.1 Application 统计
  uint32_t totalResps = 0;
  std::vector<double> sendTimes, recvTimes;
  double firstSend = std::numeric_limits<double>::infinity();
  double lastRecv = 0.0;
  double sumDelay = 0.0;
  size_t nDone = 0;
  uint64_t totalActualBytes = 0; // 实际接收的字节数（用于混合大小分布）
 
  double rfcJitter = 0.0;
  bool havePrevTransit = false;
  double prevTransit = 0.0;

  // 直接使用全局的最早和最晚时间来计算页面加载时间
  double pageFirstSend = std::numeric_limits<double>::infinity();
  double pageLastRecv = 0.0;
  
  //统计每个客户端的响应数、发送时间、接收时间
  for (auto& client : clients) {
    totalResps += client->GetRespsRcvd();
    const auto& s = client->GetReqSendTimes();
    const auto& r = client->GetRespRecvTimes();
    
    // 直接在这里更新全局的最早和最晚时间
    if (!s.empty()) {
      firstSend = std::min(firstSend, s.front());
      pageFirstSend = std::min(pageFirstSend, s.front());
    }
    if (!r.empty()) {
      lastRecv = std::max(lastRecv, r.back());
      pageLastRecv = std::max(pageLastRecv, r.back());
    }
    
    size_t n = std::min(s.size(), r.size());
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

  // 计算实际接收的字节数（从客户端记录的实际大小）
  for (auto& client : clients) {
    const auto& doneSizes = client->GetDoneSizes();
    for (auto size : doneSizes) {
      totalActualBytes += size;
    }
  }

  // 修复HoL统计：基于响应完成间隔，而不是发送间隔
  uint64_t holEvents = 0;
  double holBlockedTime = 0.0;
  for (auto& client : clients) {
    const auto& r = client->GetRespRecvTimes();
    if (r.size() > 1) {
      // 计算响应完成间隔，如果间隔过大说明有阻塞
      for (size_t i = 1; i < r.size(); ++i) {
        double respInterval = r[i] - r[i-1];
        // 如果响应间隔超过理想间隔的2倍，认为是HoL阻塞
        double idealInterval = client->GetInterval() * 1.5; // 允许一些容差
        if (respInterval > idealInterval) {
        ++holEvents;
          holBlockedTime += (respInterval - idealInterval);
        }
      }
    }
  }

  // 在统计输出前，添加 pageTime sanity check
  // 添加调试信息，显示pageFirstSend和pageLastRecv的值
  std::cout << "DEBUG: For file size [" << respSize << "], first send time is: " << pageFirstSend << std::endl;
  std::cout << "DEBUG: For file size [" << respSize << "], last receive time is: " << pageLastRecv << std::endl;
  
  // 确保pageTime是有效的值
  double pageTime = 0.0;
  if (pageFirstSend != std::numeric_limits<double>::infinity() && pageLastRecv > pageFirstSend) {
    pageTime = pageLastRecv - pageFirstSend;
  } else {
    std::cout << "DEBUG: Invalid page times detected, using fallback values:" << std::endl;
    if (firstSend != std::numeric_limits<double>::infinity() && lastRecv > firstSend) {
      pageTime = lastRecv - firstSend;
      std::cout << "DEBUG: Using global times: " << firstSend << " to " << lastRecv << std::endl;
    } else {
      // 如果仍然无效，使用理论值
      double theoretical_transfer_time = (respSize * 8) / (1000 * 1e6); // 假设1000Mbps
      pageTime = theoretical_transfer_time + 0.0015; // 加上TCP握手和HTTP开销
      std::cout << "DEBUG: Using theoretical time: " << pageTime << std::endl;
    }
  }
  std::cout << "DEBUG: Calculated pageTime is: " << pageTime << std::endl;

  if (nDone > 0 && lastRecv > firstSend) {
    double avgDelay = sumDelay / static_cast<double>(nDone);
    double totalTime = lastRecv - firstSend;
    
    // 使用页面时间计算吞吐量，更准确
    double throughput = (totalActualBytes * 8.0) / (pageTime * 1e6); // Mbps
    
    std::cout << "The HTTP/1.1 experiment has ended. The total number of responses received by the client is: " << totalResps << "/" << nRequests << std::endl;
    std::cout << "Average delay of HTTP/1.1: " << avgDelay << " s" << std::endl;
    std::cout << "Average throughput of HTTP/1.1: " << throughput << " Mbps" << std::endl;
    std::cout << "Total bytes received: " << totalActualBytes << " bytes" << std::endl;
    
    // 使用页面时间作为PLT，而不是整个仿真时间
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "HTTP/1.1 Page Load Time (onLoad): " << pageTime << " s" << std::endl;
    std::cout << "Page completed: " << totalResps << "/" << nRequests << " requests" << std::endl;
    std::cout << "TCP retransmissions: " << g_retxCount
              << "  rate: " << (g_retxCount / (pageTime > 0 ? pageTime : 1.0)) << " /s" << std::endl;
    std::cout << "RFC3550 jitter estimate: " << rfcJitter << " s" << std::endl;
    std::cout << "HoL events: " << holEvents << "  HoL blocked time: " << holBlockedTime << " s" << std::endl;
    std::cout << "Fixed header sizes - Request: " << reqHdrBytes << "B, Response: " << respHdrBytes << "B" << std::endl;
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

