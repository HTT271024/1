#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Http3BandwidthSim");

int main(int argc, char *argv[]) {
    std::string bandwidth = "10Mbps";
    double loss = 0.01;
    uint32_t packetSize = 1200; // QUIC typical MTU
    uint32_t numStreams = 3;
    double delay = 10.0; // ms
    double duration = 10.0;

    CommandLine cmd;
    cmd.AddValue("bandwidth", "Link bandwidth", bandwidth);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    p2p.SetChannelAttribute("Delay", StringValue(std::to_string(delay) + "ms"));
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
        sender.Stop(Seconds(1.0 + duration));
        senders.push_back(sender);

        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
        sinkApp.Start(Seconds(0.5));
        sinkApp.Stop(Seconds(2.0 + duration));
        sinks.push_back(sinkApp);

        sinkPtrs.push_back(DynamicCast<PacketSink>(sinkApp.Get(0)));
    }

    Simulator::Stop(Seconds(2.0 + duration));
    Simulator::Run();

    double totalThroughput = 0;
    std::cout << "bandwidth: " << bandwidth << std::endl;
    for (size_t i = 0; i < sinkPtrs.size(); ++i) {
        double rx = sinkPtrs[i]->GetTotalRx() * 8.0 / duration / 1000.0; // kbps
        totalThroughput += rx;
        std::cout << "stream" << (i+1) << "_throughput: " << std::fixed << std::setprecision(2) << rx << " kbps" << std::endl;
    }
    std::cout << "total_throughput: " << std::fixed << std::setprecision(2) << totalThroughput << " kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}
