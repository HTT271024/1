#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include <queue>
#include <iostream>
#include <vector>
#include <algorithm> // For std::sort

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Http1Dot1PacketLossSim");

// ===================== HTTP/1.1 Server =====================
// (Server App is identical to your version, no changes needed)
class HttpServerApp : public Application {
public:
  HttpServerApp() : m_socket(0), m_port(0) {}
  virtual ~HttpServerApp() { m_socket = 0; }

  void Setup(uint16_t port, uint32_t respSize, uint32_t maxReqs) {
    m_port = port;
    m_respSize = respSize;
    m_maxReqs = maxReqs;
  }

private:
  virtual void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                MakeCallback(&HttpServerApp::HandleAccept, this));
  }
  virtual void StopApplication() override {
    if (m_socket) m_socket->Close();
  }
  void HandleAccept(Ptr<Socket> s, const Address &from) {
    s->SetRecvCallback(MakeCallback(&HttpServerApp::HandleRead, this));
  }
  void HandleRead(Ptr<Socket> s) {
    Ptr<Packet> packet;
    while ((packet = s->Recv())) {
        if (packet->GetSize() > 0) {
            // This is a simplified server that sends a response for every received packet.
            // A more realistic one would parse the request.
            m_reqsHandled++;
            std::ostringstream oss;
            oss << "HTTP/1.1 200 OK\r\nContent-Length: " << m_respSize << "\r\n\r\n";
            std::string header = oss.str();
            Ptr<Packet> resp = Create<Packet>((uint8_t*)header.c_str(), header.size());
            Ptr<Packet> body = Create<Packet>(m_respSize);
            s->Send(resp);
            s->Send(body);
        }
    }
  }
  Ptr<Socket> m_socket;
  uint16_t m_port;
  uint32_t m_respSize;
  uint32_t m_maxReqs;
  uint32_t m_reqsHandled = 0;
};

// ===================== HTTP/1.1 Client =====================
// (Client App is identical to your version, no changes needed)
class HttpClientApp : public Application {
public:
  HttpClientApp() : m_socket(0), m_port(0) {}
  virtual ~HttpClientApp() { m_socket = 0; }
  void Setup(Address servAddr, uint16_t port, uint32_t reqSize, uint32_t nReqs, double interval) {
    m_servAddr = servAddr;
    m_port = port;
    m_reqSize = reqSize;
    m_nReqs = nReqs;
    m_interval = interval;
  }
  uint32_t GetRespsRcvd() const { return m_respsRcvd; }
  const std::vector<double>& GetReqSendTimes() const { return m_reqSendTimes; }
  const std::vector<double>& GetRespRecvTimes() const { return m_respRecvTimes; }
private:
  virtual void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_servAddr), m_port));
    m_socket->SetRecvCallback(MakeCallback(&HttpClientApp::HandleRead, this));
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
  virtual void StopApplication() override {
    if (m_socket) m_socket->Close();
  }
  void SendNextRequest() {
    if (m_reqsSent < m_nReqs) {
      std::ostringstream oss;
      oss << "GET /file" << m_reqsSent << " HTTP/1.1\r\nHost: server\r\n\r\n";
      std::string req = oss.str();
      Ptr<Packet> p = Create<Packet>((uint8_t*)req.c_str(), req.size());
      m_socket->Send(p);
      m_reqSendTimes.push_back(Simulator::Now().GetSeconds());
      m_reqsSent++;
      m_waitingResp = true;
      m_bytesToRecv = 0;
      m_bytesRcvd = 0;
    }
  }
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
          if (headerEnd != std::string::npos) {
            size_t pos = m_buffer.find("Content-Length: ");
            if (pos != std::string::npos) {
              size_t end = m_buffer.find("\r\n", pos);
              std::string lenStr = m_buffer.substr(pos + 16, end - (pos + 16));
              m_bytesToRecv = std::stoi(lenStr);
              m_bodyStart = headerEnd + 4;
            } else break;
          } else break;
        }
        if (m_bytesToRecv > 0) {
          size_t bodyBytes = m_buffer.size() - m_bodyStart;
          if (bodyBytes >= m_bytesToRecv) {
            m_respsRcvd++;
            m_waitingResp = false;
            m_respRecvTimes.push_back(Simulator::Now().GetSeconds());
            if (m_respsRcvd < m_nReqs) {
              Simulator::Schedule(Seconds(m_interval), &HttpClientApp::SendNextRequest, this);
            }
            m_buffer = m_buffer.substr(m_bodyStart + m_bytesToRecv);
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
  double m_interval = 0.01;
};

// ===================== Main =====================
int main(int argc, char *argv[]) {
  // --- 参数配置 ---
  uint32_t nRequests = 100;       // 总请求数 (建议增加)
  uint32_t respSize = 100 * 1024; // 100KB per response
  uint32_t reqSize = 100;         // 请求报文大小
  uint16_t httpPort = 8080;
  double errorRate = 0.01;        // **核心参数：丢包率**
  std::string dataRate = "10Mbps";
  std::string delay = "50ms";       // **核心参数：延迟** (设为50ms, RTT=100ms)
  uint32_t nConnections = 6;      // **核心参数：并发连接数** (1为HOL, 6为并行)

  CommandLine cmd;
  cmd.AddValue("nRequests", "Number of HTTP requests", nRequests);
  cmd.AddValue("respSize", "HTTP response size (bytes)", respSize);
  cmd.AddValue("errorRate", "Packet loss rate", errorRate);
  cmd.AddValue("dataRate", "Link bandwidth", dataRate);
  cmd.AddValue("delay", "Link delay", delay);
  cmd.AddValue("nConnections", "Number of parallel HTTP/1.1 connections", nConnections);
  cmd.Parse(argc, argv);

  // --- 网络拓扑与协议栈 ---
  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));
  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);
  
  // **关键：安装丢包模型**
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
  // 将丢包模型安装在服务器的接收端网卡上
  devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  // --- 应用配置 ---
  Ptr<HttpServerApp> serverApp = CreateObject<HttpServerApp>();
  serverApp->Setup(httpPort, respSize, nRequests);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.5));
  serverApp->SetStopTime(Seconds(120.0)); // 延长运行时间以应对高丢包

  std::vector<Ptr<HttpClientApp>> clients;
  uint32_t baseReqs = nRequests / nConnections;
  uint32_t rem = nRequests % nConnections;
  for (uint32_t i = 0; i < nConnections; ++i) {
    uint32_t reqsForThisClient = baseReqs + (i < rem ? 1 : 0);
    if (reqsForThisClient == 0) continue;
    Ptr<HttpClientApp> client = CreateObject<HttpClientApp>();
    // 注意：这里的请求间隔 interval 设为0.01，模拟客户端尽快发送
    client->Setup(interfaces.GetAddress(1), httpPort, reqSize, reqsForThisClient, 0.01);
    nodes.Get(0)->AddApplication(client);
    client->SetStartTime(Seconds(1.0 + i * 0.001)); // 稍微错开启动
    client->SetStopTime(Seconds(120.0));
    clients.push_back(client);
  }

  // --- 监控与仿真 ---
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

  Simulator::Stop(Seconds(125.0)); // 保证仿真有足够时间完成
  Simulator::Run();

  // --- 结果统计 ---
  uint32_t totalResps = 0;
  std::vector<double> sendTimes, recvTimes;
  for (auto& client : clients) {
    totalResps += client->GetRespsRcvd();
    const auto& s = client->GetReqSendTimes();
    const auto& r = client->GetRespRecvTimes();
    sendTimes.insert(sendTimes.end(), s.begin(), s.end());
    recvTimes.insert(recvTimes.end(), r.begin(), r.end());
  }

  std::cout << "\n================= RESULTS (errorRate=" << errorRate 
            << ", nConnections=" << nConnections << ") =================" << std::endl;

  if (sendTimes.empty() || recvTimes.empty()) {
      std::cout << "No requests were completed. Simulation might be too short or loss rate too high." << std::endl;
  } else {
    // 排序以找到最早的发送和最晚的接收
    std::sort(sendTimes.begin(), sendTimes.end());
    std::sort(recvTimes.begin(), recvTimes.end());

    double startTime = sendTimes[0];
    double endTime = recvTimes.back();
    double pageLoadTime = endTime - startTime;
    double totalBytes = totalResps * respSize;
    double throughput = (totalBytes * 8) / (pageLoadTime * 1e6); // Mbps

    std::cout << "Total Requests Sent/Completed: " << sendTimes.size() << "/" << totalResps << std::endl;
    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;
    std::cout << "Page Load Time (onLoad): " << pageLoadTime << " s" << std::endl;
  }
  std::cout << "=======================================================\n" << std::endl;

  flowmon->SerializeToXmlFile("http1.1-loss-sim.xml", true, true);
  Simulator::Destroy();

  return 0;
}