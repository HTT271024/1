#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <map>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HTTP2App");

// HTTP/2 Frame Type
enum FrameType { HEADERS, DATA, PUSH_PROMISE };

// HTTP/2 Frame
struct HTTP2Frame {
    uint32_t streamId;
    FrameType type;
    uint32_t length;
    std::string payload;
};
// 简单序列化/反序列化
Ptr<Packet> SerializeFrame(const HTTP2Frame& frame) {
    Ptr<Packet> p = Create<Packet>((uint8_t*)frame.payload.data(), frame.payload.size());
    // 这里可扩展为自定义Header
    return p;
}

// 多路复用 Session
class HTTP2Session : public Object {
public:
    HTTP2Session(Ptr<Socket> socket) : m_socket(socket) {}
    void SendFrame(const HTTP2Frame& frame) {
        Ptr<Packet> p = SerializeFrame(frame);
        m_socket->Send(p);
        m_streams[frame.streamId]; // 确保流存在
    }
    void OnReceive(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        while ((packet = socket->Recv())) {
            // 这里只做简单演示，实际应反序列化
            NS_LOG_INFO("Server: received " << packet->GetSize() << " bytes");
        }
    }
    void OpenStream(uint32_t streamId) {
        m_streams[streamId] = false;
    }
    void CloseStream(uint32_t streamId) {
        m_streams[streamId] = true;
    }
    std::map<uint32_t, bool> m_streams;
    Ptr<Socket> m_socket;
};

// 客户端应用
class HTTP2ClientApp : public Application {
public:
    HTTP2ClientApp() {}
    void Setup(Address address, uint32_t nStreams) {
        m_peer = address;
        m_nStreams = nStreams;
    }
    virtual void StartApplication() {
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
        m_socket->Connect(m_peer);
        m_session = CreateObject<HTTP2Session>(m_socket);
        m_socket->SetRecvCallback(MakeCallback(&HTTP2Session::OnReceive, m_session));
        Simulator::Schedule(Seconds(0.1), &HTTP2ClientApp::SendRequests, this);
    }
    void SendRequests() {
        for (uint32_t i = 1; i <= m_nStreams; ++i) {
            HTTP2Frame frame;
            frame.streamId = i;
            frame.type = HEADERS;
            frame.length = 100 * 0.3; // 头部压缩
            frame.payload = "headers";
            m_session->SendFrame(frame);

            frame.type = DATA;
            frame.length = 100000; // 10万字节
            frame.payload = std::string(100000, 'x'); // 10万字节的 'x'
            m_session->SendFrame(frame);
        }
    }
private:
    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_nStreams;
    Ptr<HTTP2Session> m_session;
};

// 服务器应用
class HTTP2ServerApp : public Application {
public:
    HTTP2ServerApp() {}
    virtual void StartApplication() {
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
        m_socket->Bind(local);
        m_socket->Listen();
        m_socket->SetAcceptCallback(
            MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
            MakeCallback(&HTTP2ServerApp::HandleAccept, this));
    }
    void HandleAccept(Ptr<Socket> s, const Address &from) {
        s->SetRecvCallback(MakeCallback(&HTTP2ServerApp::HandleRead, this));
    }
    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        while ((packet = socket->Recv())) {
            NS_LOG_INFO("Server: received " << packet->GetSize() << " bytes");
            // 简单模拟 Server Push
            HTTP2Frame push;
            push.streamId = 100;
            push.type = PUSH_PROMISE;
            push.length = 50;
            push.payload = "push resource";
            socket->Send(SerializeFrame(push));
        }
    }
private:
    Ptr<Socket> m_socket;
};

int main(int argc, char *argv[]) {
    LogComponentEnable("HTTP2App", LOG_LEVEL_INFO);

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

    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

    Ptr<HTTP2ServerApp> serverApp = CreateObject<HTTP2ServerApp>();
    nodes.Get(1)->AddApplication(serverApp);
    serverApp->SetStartTime(Seconds(0.0));
    serverApp->SetStopTime(Seconds(10.0));

    Ptr<HTTP2ClientApp> clientApp = CreateObject<HTTP2ClientApp>();
    clientApp->Setup(serverAddress, 3); // 3 个 stream
    nodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(1.0));
    clientApp->SetStopTime(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
