#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include <queue>
#include <iostream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Http1Dot1Sim");

//Data packet tracking function
static void TxTrace(Ptr<const Packet> packet) {
  std::cout << "[Trace] Packet sent, size=" << packet->GetSize() << std::endl;
}
static void RxTrace(Ptr<const Packet> packet) {
  std::cout << "[Trace] Packet received, size=" << packet->GetSize() << std::endl;
}


class HttpServerApp : public Application {
public:
//Constructors and destructors
  HttpServerApp() : m_socket(0), m_port(0) {}
  virtual ~HttpServerApp() { m_socket = 0; }
//Setup function
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
  //HandleAccept function
  void HandleAccept(Ptr<Socket> s, const Address &from) {
    s->SetRecvCallback(MakeCallback(&HttpServerApp::HandleRead, this));
    m_clientSocket = s;
    m_reqsHandled = 0;
  }
  //HandleRead function
  void HandleRead(Ptr<Socket> s) {
    Ptr<Packet> packet = s->Recv();
    if (packet->GetSize() > 0 && m_reqsHandled < m_maxReqs) {
      // 解析请求（这里简单假设每个请求都有效）
      m_reqsHandled++;
      // 构造响应：HTTP/1.1 200 OK\r\nContent-Length: ...\r\n\r\n<data>
      std::ostringstream oss;
      oss << "HTTP/1.1 200 OK\r\nContent-Length: " << m_respSize << "\r\n\r\n";
      std::string header = oss.str();
      Ptr<Packet> resp = Create<Packet>((uint8_t*)header.c_str(), header.size());
      Ptr<Packet> body = Create<Packet>(m_respSize);
      s->Send(resp);
      s->Send(body);
      NS_LOG_INFO("[Server] Sent response " << m_reqsHandled << ", size=" << m_respSize);
    }
  }
  //Member variables of the server class
  Ptr<Socket> m_socket; //Server socket
  Ptr<Socket> m_clientSocket; //Client socket
  uint16_t m_port; // Port number
  uint32_t m_respSize;// Response size
  uint32_t m_maxReqs; // Maximum number of requests
  uint32_t m_reqsHandled = 0; //The number of processed requests
};


// ===================== HTTP/1.1 Client =====================
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
      NS_LOG_INFO("[Client] Sent request " << m_reqsSent);
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
            std::cout << "[Client] Received response " << m_respsRcvd << " at " << Simulator::Now().GetSeconds() << "s" << std::endl;
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
  double m_interval = 0.01;  // 默认间隔为 0.01 秒
};

// ===================== Main =====================

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

  CommandLine cmd;
  cmd.AddValue("nRequests", "Number of HTTP requests", nRequests);
  cmd.AddValue("respSize", "HTTP response size (bytes)", respSize);
  cmd.AddValue("reqSize", "HTTP request size (bytes)", reqSize);
  cmd.AddValue("httpPort", "HTTP server port", httpPort);
  cmd.AddValue("errorRate", "Packet loss rate", errorRate);
  cmd.AddValue("dataRate", "Link bandwidth", dataRate);
  cmd.AddValue("delay", "Link delay", delay);
  cmd.AddValue("interval", "Interval between HTTP requests (s)", interval);
  cmd.AddValue("nConnections", "Number of parallel HTTP/1.1 connections", nConnections);
  cmd.Parse(argc, argv);

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

  // HTTP/1.1 Application
  Ptr<HttpServerApp> serverApp = CreateObject<HttpServerApp>();
  serverApp->Setup(httpPort, respSize, nRequests);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.5));
  serverApp->SetStopTime(Seconds(30.0));

  // 多连接客户端
  std::vector<Ptr<HttpClientApp>> clients;
  std::vector<std::vector<double>> allSendTimes, allRecvTimes;
  uint32_t baseReqs = nRequests / nConnections;
  uint32_t rem = nRequests % nConnections;
  for (uint32_t i = 0; i < nConnections; ++i) {
    uint32_t reqs = baseReqs + (i < rem ? 1 : 0); // 平均分配请求
    Ptr<HttpClientApp> client = CreateObject<HttpClientApp>();
    client->Setup(interfaces.GetAddress(1), httpPort, reqSize, reqs, interval);
    nodes.Get(0)->AddApplication(client);
    client->SetStartTime(Seconds(1.0 + i * 0.01)); // 避免完全同时启动
    client->SetStopTime(Seconds(30.0));
    clients.push_back(client);
  }

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
  devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

  Config::ConnectWithoutContext(
    "/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTx",
    MakeCallback(&TxTrace));
  Config::ConnectWithoutContext(
    "/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx",
    MakeCallback(&RxTrace));

  Simulator::Stop(Seconds(35.0));
  Simulator::Run();

  // HTTP/1.1 Application 统计
  uint32_t totalResps = 0;
  std::vector<double> sendTimes, recvTimes;
  for (auto& client : clients) {
    totalResps += client->GetRespsRcvd();
    const auto& s = client->GetReqSendTimes();
    const auto& r = client->GetRespRecvTimes();
    sendTimes.insert(sendTimes.end(), s.begin(), s.end());
    recvTimes.insert(recvTimes.end(), r.begin(), r.end());
  }
  // 按发送时间排序（可选）
  std::sort(sendTimes.begin(), sendTimes.end());
  std::sort(recvTimes.begin(), recvTimes.end());

  double totalDelay = 0.0;
  size_t nDone = std::min(sendTimes.size(), recvTimes.size());
  for (size_t i = 0; i < nDone; ++i) {
    totalDelay += (recvTimes[i] - sendTimes[i]);
  }
  if (nDone > 0) {
    double avgDelay = totalDelay / nDone;
    double totalBytes = nDone * respSize;
    double totalTime = recvTimes[nDone-1] - sendTimes[0];
    double throughput = (totalBytes * 8) / (totalTime * 1e6); // Mbps
    std::cout << "The HTTP/1.1 experiment has ended. The total number of responses received by the client is: " << totalResps << "/" << nRequests << std::endl;
    std::cout << "Average delay of HTTP/1.1: " << avgDelay << " s" << std::endl;
    std::cout << "Average throughput of HTTP/1.1: " << throughput << " Mbps" << std::endl;
    // --- 新增：页面加载时间 ---
    double startTime = sendTimes[0];
    double endTime = recvTimes[nDone-1];
    double pageLoadTime = endTime - startTime;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "HTTP/1.1 Page Load Time (onLoad): " << pageLoadTime << " s" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
  }

  flowmon->CheckForLostPackets();
  flowmon->SerializeToXmlFile("flowmon.xml", true, true);

  Simulator::Destroy();

  return 0;
} 