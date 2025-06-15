#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HttpBaselineTest");

class SimpleHttpServer : public Application {
public:
  SimpleHttpServer(uint64_t* total) : m_started(false), m_totalBytes(total) {}

  bool m_started;
  Time m_startTime;
  Time m_lastReceiveTime;

  void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
    m_socket->Bind(local);
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                MakeCallback(&SimpleHttpServer::HandleAccept, this));
  }

  void HandleAccept(Ptr<Socket> s, const Address &from) {
    s->SetRecvCallback(MakeCallback(&SimpleHttpServer::HandleRead, this));
  }

  void HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    while ((packet = socket->Recv())) {
      *m_totalBytes += packet->GetSize();
      
      if (!m_started) {
        m_startTime = Simulator::Now();
        m_started = true;
      }
      m_lastReceiveTime = Simulator::Now();
    }
  }

private:
  Ptr<Socket> m_socket;
  uint64_t* m_totalBytes;
};

class SimpleHttpClient : public Application {
public:
  void Setup(Address address, uint32_t payloadSize, uint32_t nStreams, bool isHttp2) {
    m_peer = address;
    m_payloadSize = payloadSize;
    m_nStreams = nStreams;
    m_isHttp2 = isHttp2;
  }

  void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
    m_socket->SetSendCallback(MakeCallback(&SimpleHttpClient::HandleSend, this));
    Simulator::Schedule(Seconds(1.0), &SimpleHttpClient::Send, this);
  }

  void Send() {
    if (Simulator::Now().GetSeconds() >= 59.0) return;

    std::string payload(m_payloadSize, 'x');
    Ptr<Packet> packet = Create<Packet>((uint8_t*)payload.data(), payload.size());
    
    if (m_isHttp2) {
      // HTTP/2: Send multiple streams concurrently
      for (uint32_t i = 0; i < m_nStreams; ++i) {
        m_socket->Send(packet);
      }
    } else {
      // HTTP/1.1: Send sequentially
      m_socket->Send(packet);
    }
  }

  void HandleSend(Ptr<Socket> socket, uint32_t available) {
    if (Simulator::Now().GetSeconds() >= 59.0) return;

    if (available > 0) {
      std::string payload(m_payloadSize, 'x');
      Ptr<Packet> packet = Create<Packet>((uint8_t*)payload.data(), payload.size());
      socket->Send(packet);
    }
  }

private:
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_payloadSize;
  uint32_t m_nStreams;
  bool m_isHttp2;
};

int main(int argc, char *argv[]) {
  double errorRate = 0.0;
  bool isHttp2 = true;
  uint32_t payloadSize = 10000; // 10 KB
  uint32_t nStreams = 10;       // Number of concurrent streams for HTTP/2
  CommandLine cmd;
  cmd.AddValue("errorRate", "Packet loss rate", errorRate);
  cmd.AddValue("isHttp2", "Whether to use HTTP/2", isHttp2);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));
  NetDeviceContainer devices = p2p.Install(nodes);

  if (errorRate > 0) {
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(errorRate));
    em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
    devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint64_t totalBytes = 0;

  Ptr<SimpleHttpServer> serverApp = CreateObject<SimpleHttpServer>(&totalBytes);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(60.0));

  Ptr<SimpleHttpClient> clientApp = CreateObject<SimpleHttpClient>();
  clientApp->Setup(InetSocketAddress(interfaces.GetAddress(1), 8080), payloadSize, nStreams, isHttp2);
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(60.0));

  Simulator::Run();
  Simulator::Destroy();

  Time duration = serverApp->m_lastReceiveTime - serverApp->m_startTime;
  double throughput = (totalBytes * 8.0) / duration.GetSeconds() / 1e6;
  std::cout << "protocol=" << (isHttp2 ? "HTTP/2" : "HTTP/1.1")
            << ", loss=" << errorRate
            << ", total=" << totalBytes
            << ", throughput=" << throughput << " Mbps" << std::endl;

  return 0;
}