#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <iostream>

using namespace ns3;

// 简单的QUIC包头结构
struct QuicHeader {
  uint32_t connectionId;
  uint8_t type; // 0=握手, 1=数据
  uint32_t streamId;
  uint32_t seq;
};

class QuicServer : public Application {
public:
  QuicServer() {}
  void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9000);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&QuicServer::HandleRead, this));
  }
  void HandleRead(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    QuicHeader header;
    packet->CopyData((uint8_t*)&header, sizeof(header));
    if (header.type == 0) { // 握手包
      std::cout << "Server: Received handshake, sending handshake ack\n";
      header.type = 0xFF; // ack
      Ptr<Packet> ack = Create<Packet>((uint8_t*)&header, sizeof(header));
      socket->SendTo(ack, 0, from);
    } else if (header.type == 1) {
      std::cout << "Server: Received data, stream=" << header.streamId << ", seq=" << header.seq << "\n";
    }
  }
private:
  Ptr<Socket> m_socket;
};

class QuicClient : public Application {
public:
  QuicClient(Address peer) : m_peer(peer) {}
  void StartApplication() override {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    Simulator::Schedule(Seconds(1.0), &QuicClient::SendHandshake, this);
  }
  void SendHandshake() {
    QuicHeader header = {1234, 0, 0, 0};
    Ptr<Packet> packet = Create<Packet>((uint8_t*)&header, sizeof(header));
    m_socket->SendTo(packet, 0, m_peer);
    m_socket->SetRecvCallback(MakeCallback(&QuicClient::HandleRead, this));
  }
  void HandleRead(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    QuicHeader header;
    packet->CopyData((uint8_t*)&header, sizeof(header));
    if (header.type == 0xFF) {
      std::cout << "Client: Handshake ack received, start sending data\n";
      Simulator::Schedule(Seconds(0.1), &QuicClient::SendData, this, 1);
    }
  }
  void SendData(uint32_t seq) {
    QuicHeader header = {1234, 1, 1, seq};
    Ptr<Packet> packet = Create<Packet>((uint8_t*)&header, sizeof(header));
    m_socket->SendTo(packet, 0, m_peer);
    if (seq < 10) {
      Simulator::Schedule(MilliSeconds(10), &QuicClient::SendData, this, seq+1);
    }
  }
private:
  Ptr<Socket> m_socket;
  Address m_peer;
};

int main(int argc, char* argv[]) {
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

  Ptr<QuicServer> serverApp = CreateObject<QuicServer>();
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(10.0));

  Ptr<QuicClient> clientApp = CreateObject<QuicClient>(InetSocketAddress(interfaces.GetAddress(1), 9000));
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(0.5));
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
