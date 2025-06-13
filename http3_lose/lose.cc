#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Http3LossSim");

int main(int argc, char *argv[]) {
    double errorRate = 0.0;
    CommandLine cmd;
    cmd.AddValue("errorRate", "Packet error rate", errorRate);
    cmd.Parse(argc, argv);

    std::vector<double> lossRates = {errorRate};
    std::string bandwidth = "10Mbps";
    uint32_t packetSize = 1200;
    uint32_t numStreams = 3;
    double duration = 10.0;
    std::vector<std::string> summaryResults;

    for (double loss : lossRates) {
        std::cout << "\n=== 测试丢包率 = " << (loss * 100) << "% ===" << std::endl;

        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
        p2p.SetChannelAttribute("Delay", StringValue("10ms"));
        NetDeviceContainer devices = p2p.Install(nodes);

        // 使用 BurstErrorModel 来模拟更真实的丢包情况
        Ptr<BurstErrorModel> em = CreateObject<BurstErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(loss));
        em->SetAttribute("BurstSize", StringValue("ns3::UniformRandomVariable[Min=1|Max=3]"));
        devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        std::vector<ApplicationContainer> senders, sinks;
        std::vector<Ptr<PacketSink>> sinkPtrs;

        for (uint32_t i = 0; i < numStreams; ++i) {
            uint16_t port = 9000 + i;
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
            sinkApp.Stop(Seconds(1.0 + duration + 1));
            sinks.push_back(sinkApp);

            sinkPtrs.push_back(DynamicCast<PacketSink>(sinkApp.Get(0)));
        }

        Simulator::Stop(Seconds(1.0 + duration + 1));
        Simulator::Run();

        std::ostringstream oss;
        for (size_t i = 0; i < sinkPtrs.size(); ++i) {
            double throughput = sinkPtrs[i]->GetTotalRx() * 8.0 / duration / 1000.0; // kbps
            std::cout << "流" << (i+1) << "总接收: " << sinkPtrs[i]->GetTotalRx() << " bytes, 平均吞吐量: " << throughput << " kbps" << std::endl;
            oss << "流" << (i+1) << "总接收=" << sinkPtrs[i]->GetTotalRx() << ", 吞吐量=" << throughput << " kbps; ";
        }
        oss << "丢包率=" << (loss * 100) << "%";
        summaryResults.push_back(oss.str());

        Simulator::Destroy();
    }

    std::cout << "\n===== 汇总结果 =====" << std::endl;
    for (const auto& line : summaryResults) {
        std::cout << line << std::endl;
    }

    return 0;
}