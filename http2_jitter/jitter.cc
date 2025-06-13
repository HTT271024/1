#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Http2JitterTest");

class SimpleHttp2Server : public Application {
public:
  SimpleHttp2Server(uint64_t* total) : m_started(false), m_totalBytes(total) {}

  bool m_started;
  Time m_startTime;
  Time m_lastReceiveTime;

  void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
    m_socket->Bind(local);
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                MakeCallback(&SimpleHttp2Server::HandleAccept, this));
  }

  void HandleAccept(Ptr<Socket> s, const Address &from) {
    s->SetRecvCallback(MakeCallback(&SimpleHttp2Server::HandleRead, this));
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

class SimpleHttp2Client : public Application {
public:
  void Setup(Address address, uint32_t payloadSize, uint32_t nStreams, double jitterMs) {
    m_peer = address;
    m_payloadSize = payloadSize;
    m_nStreams = nStreams;
    m_jitterMs = jitterMs;
    m_jitterVar = CreateObject<UniformRandomVariable>();
    m_jitterVar->SetAttribute("Min", DoubleValue(-m_jitterMs));
    m_jitterVar->SetAttribute("Max", DoubleValue(m_jitterMs));
  }

  void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
    for (uint32_t i = 0; i < m_nStreams; ++i) {
      Simulator::Schedule(MilliSeconds(i * 10), &SimpleHttp2Client::StartStream, this);
    }
  }

  void StartStream() {
    if (Simulator::Now().GetSeconds() >= 59.0) return;
    
    for (uint32_t i = 0; i < 1000; ++i) {
      SendPacket();
    }
    
    Simulator::Schedule(MilliSeconds(100), &SimpleHttp2Client::StartStream, this);
  }

  void SendPacket() {
    if (Simulator::Now().GetSeconds() >= 59.0) return;
    std::string payload(m_payloadSize, 'x');
    Ptr<Packet> packet = Create<Packet>((uint8_t*)payload.data(), payload.size());
    double jitter = m_jitterVar->GetValue();
    Simulator::Schedule(MilliSeconds(jitter), &SimpleHttp2Client::DelayedSend, this, packet);
  }

  void DelayedSend(Ptr<Packet> packet) {
    if (Simulator::Now().GetSeconds() < 60.0) {
      m_socket->Send(packet);
    }
  }

private:
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_payloadSize;
  uint32_t m_nStreams;
  double m_jitterMs;
  Ptr<UniformRandomVariable> m_jitterVar;
};

int main(int argc, char *argv[]) {
  double jitter = 0.0;
  uint32_t payloadSize = 1000;  // 1KB
  uint32_t nStreams = 20;       // 20 个并发流
  CommandLine cmd;
  cmd.AddValue("jitter", "Application layer jitter (ms)", jitter);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));
  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint64_t totalBytes = 0;

  Ptr<SimpleHttp2Server> serverApp = CreateObject<SimpleHttp2Server>(&totalBytes);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(60.0));

  Ptr<SimpleHttp2Client> clientApp = CreateObject<SimpleHttp2Client>();
  clientApp->Setup(InetSocketAddress(interfaces.GetAddress(1), 8080), payloadSize, nStreams, jitter);
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(60.0));

  Simulator::Run();
  Simulator::Destroy();

  Time duration = serverApp->m_lastReceiveTime - serverApp->m_startTime;
  double throughput = (totalBytes * 8.0) / duration.GetSeconds() / 1e6;
  std::cout << "jitter=" << jitter
            << ", total=" << totalBytes
            << ", throughput=" << throughput << " Mbps" << std::endl;

  return 0;
} 