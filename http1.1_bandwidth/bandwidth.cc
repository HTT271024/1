#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <vector>
#include <iostream>
#include <sstream>
#include <numeric>
#include <cmath>

using namespace ns3;

class HttpServerApp : public Application {
public:
  HttpServerApp() : m_socket(0), m_port(0) {}
  void Setup(uint16_t port, uint32_t respSize, uint32_t maxReqs) {
    m_port = port; m_respSize = respSize; m_maxReqs = maxReqs;
  }
private:
  virtual void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local); m_socket->Listen();
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                MakeCallback(&HttpServerApp::HandleAccept, this));
  }
  virtual void StopApplication() override { if (m_socket) m_socket->Close(); }
  void HandleAccept(Ptr<Socket> s, const Address &) {
    s->SetRecvCallback(MakeCallback(&HttpServerApp::HandleRead, this));
    m_clientSocket = s; m_reqsHandled = 0;
  }
  void HandleRead(Ptr<Socket> s) {
    Ptr<Packet> packet = s->Recv();
    if (packet->GetSize() > 0 && m_reqsHandled < m_maxReqs) {
      m_reqsHandled++;
      std::ostringstream oss;
      oss << "HTTP/1.1 200 OK\r\nContent-Length: " << m_respSize << "\r\n\r\n";
      std::string header = oss.str();
      Ptr<Packet> resp = Create<Packet>((uint8_t*)header.c_str(), header.size());
      Ptr<Packet> body = Create<Packet>(m_respSize);
      s->Send(resp); s->Send(body);
    }
  }
  Ptr<Socket> m_socket, m_clientSocket;
  uint16_t m_port; uint32_t m_respSize, m_maxReqs, m_reqsHandled = 0;
};

class HttpClientApp : public Application {
public:
  HttpClientApp() : m_socket(0), m_port(0) {}
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
  virtual void StopApplication() override { if (m_socket) m_socket->Close(); }
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
      std::cout << "[Client] Sent request " << m_reqsSent << std::endl;
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

void RunSimulation(const std::string& dataRate, uint32_t nRequests, uint32_t respSize, uint32_t reqSize, uint16_t httpPort, double errorRate, const std::string& delay, double interval, double& throughput, double& completeRate, double& avgDelayMs, uint32_t& lost) {
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
  Ptr<HttpServerApp> serverApp = CreateObject<HttpServerApp>();
  serverApp->Setup(httpPort, respSize, nRequests);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.5));
  serverApp->SetStopTime(Seconds(60.0));
  Ptr<HttpClientApp> clientApp = CreateObject<HttpClientApp>();
  clientApp->Setup(interfaces.GetAddress(1), httpPort, reqSize, nRequests, interval);
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(60.0));
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
  devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  Simulator::Stop(Seconds(65.0));
  Simulator::Run();
  const auto& sendTimes = clientApp->GetReqSendTimes();
  const auto& recvTimes = clientApp->GetRespRecvTimes();
  double totalDelay = 0.0;
  size_t nDone = std::min(sendTimes.size(), recvTimes.size());
  for (size_t i = 0; i < nDone; ++i) {
    totalDelay += (recvTimes[i] - sendTimes[i]);
  }
  avgDelayMs = nDone > 0 ? (totalDelay / nDone) * 1000.0 : 0.0;
  double totalBytes = nDone * respSize;
  double totalTime = nDone > 0 ? (recvTimes[nDone-1] - sendTimes[0]) : 1;
  throughput = (totalBytes * 8) / (totalTime * 1e6); // Mbps
  completeRate = nDone / double(nRequests);
  lost = sendTimes.size() - recvTimes.size();
  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  std::vector<std::string> dataRates = {"2Mbps", "5Mbps", "10Mbps", "20Mbps"};
  uint32_t nRequests = 100;
  uint32_t respSize = 100*1024;
  uint32_t reqSize = 100;
  uint16_t httpPort = 8080;
  double errorRate = 0.0;
  std::string delay = "5ms";
  double interval = 0.05;
  uint32_t nRuns = 5;
  CommandLine cmd;
  cmd.AddValue("nRequests", "Number of HTTP requests", nRequests);
  cmd.AddValue("respSize", "HTTP response size (bytes)", respSize);
  cmd.AddValue("reqSize", "HTTP request size (bytes)", reqSize);
  cmd.AddValue("httpPort", "HTTP server port", httpPort);
  cmd.AddValue("errorRate", "Packet loss rate", errorRate);
  cmd.AddValue("delay", "Link delay", delay);
  cmd.AddValue("interval", "Interval between HTTP requests (s)", interval);
  cmd.AddValue("nRuns", "Number of runs for averaging", nRuns);
  cmd.Parse(argc, argv);
  std::cout << "bandwidth,throughput_mean,throughput_std,complete_rate_mean,complete_rate_std,avg_delay_ms_mean,avg_delay_ms_std,lost_mean\n";
  for (const auto& dataRate : dataRates) {
    std::vector<double> throughputs, completeRates, avgDelayMss;
    std::vector<uint32_t> losts;
    for (uint32_t run = 0; run < nRuns; ++run) {
      double t, c, d;
      uint32_t l;
      RunSimulation(dataRate, nRequests, respSize, reqSize, httpPort, errorRate, delay, interval, t, c, d, l);
      throughputs.push_back(t);
      completeRates.push_back(c);
      avgDelayMss.push_back(d);
      losts.push_back(l);
    }
    auto mean = [](const std::vector<double>& v) {
      return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    };
    auto stddev = [](const std::vector<double>& v) {
      double m = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
      double s = 0.0; for (auto x : v) s += (x-m)*(x-m);
      return sqrt(s/v.size());
    };
    double t_mean = mean(throughputs), t_std = stddev(throughputs);
    double c_mean = mean(completeRates), c_std = stddev(completeRates);
    double d_mean = mean(avgDelayMss), d_std = stddev(avgDelayMss);
    double l_mean = std::accumulate(losts.begin(), losts.end(), 0.0) / losts.size();
    std::cout << dataRate << "," << t_mean << "," << t_std << "," << c_mean << "," << c_std << "," << d_mean << "," << d_std << "," << l_mean << std::endl;
  }
  return 0;
}
