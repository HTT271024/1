#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include <iostream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Http3MultiplexingSim");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);


    std::string bandwidth = "10Mbps";
    double loss = 0.01;
    uint32_t packetSize = 1200; // QUIC typical MTU
    uint32_t numStreams = 3;

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer devices = p2p.Install(nodes);

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(loss));
    devices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    std::vector<ApplicationContainer> senders, sinks;
    std::vector<Ptr<PacketSink>> sinkPtrs;

    for (uint32_t i = 0; i < numStreams; ++i) {
        uint16_t port = 9000 + i;
        // Use UDP to simulate QUIC
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        onoff.SetConstantRate(DataRate(bandwidth), packetSize);
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer sender = onoff.Install(nodes.Get(0));
        sender.Start(Seconds(1.0));
        sender.Stop(Seconds(10.0));
        senders.push_back(sender);

        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
        sinkApp.Start(Seconds(0.5));
        sinkApp.Stop(Seconds(11.0));
        sinks.push_back(sinkApp);

        sinkPtrs.push_back(DynamicCast<PacketSink>(sinkApp.Get(0)));
    }

    // Real-time monitoring for each stream
    for (int t = 1; t <= 11; ++t) {
        Simulator::Schedule(Seconds(t), [sinkPtrs, t]() {
            std::cout << "At " << t << "s:";
            for (size_t i = 0; i < sinkPtrs.size(); ++i) {
                std::cout << " Stream" << (i+1) << "=" << sinkPtrs[i]->GetTotalRx() << " bytes";
            }
            std::cout << std::endl;
        });
    }

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    std::cout << "\n✅ Finally, all streams are received: ";
    for (size_t i = 0; i < sinkPtrs.size(); ++i) {
        std::cout << "stream" << (i+1) << "=" << sinkPtrs[i]->GetTotalRx() << " bytes; ";
    }
    std::cout << std::endl;

    Simulator::Destroy();
    return 0;
}