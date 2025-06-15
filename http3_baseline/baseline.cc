#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Http3BaselineSim");

class Http3Simulator {
private:
    std::vector<Ptr<PacketSink>> sinkPtrs;
    std::vector<uint64_t> lastRxBytes;
    std::vector<uint64_t> totalTxBytes;
    std::vector<double> lastRxTime;
    double startTime;
    double stopTime;
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;

public:
    Http3Simulator(double start, double stop) : startTime(start), stopTime(stop) {
        flowMonitor = flowHelper.InstallAll();
    }

    void AddSink(Ptr<PacketSink> sink) {
        sinkPtrs.push_back(sink);
        lastRxBytes.push_back(0);
        totalTxBytes.push_back(0);
        lastRxTime.push_back(startTime);
    }

    void UpdateTxBytes(uint32_t streamId, Ptr<const Packet> packet) {
        if (streamId < totalTxBytes.size()) {
            totalTxBytes[streamId] += packet->GetSize();
        }
    }

    void PrintResults(double duration, const std::string& bandwidth, double delay, double loss) {
        std::cout << "\n=== Test Results ===" << std::endl;
        double totalThroughput = 0;
        double totalTxThroughput = 0;
        
        // Print detailed info for each stream
        for (size_t i = 0; i < sinkPtrs.size(); ++i) {
            uint64_t totalRx = sinkPtrs[i]->GetTotalRx();
            double rxThroughput = totalRx * 8.0 / duration / 1000.0; // kbps
            double txThroughput = totalTxBytes[i] * 8.0 / duration / 1000.0; // kbps
            totalThroughput += rxThroughput;
            totalTxThroughput += txThroughput;
            
            std::cout << "Stream " << (i+1) << ":" << std::endl
                     << "  Received: " << totalRx << " bytes (" << std::fixed << std::setprecision(2) 
                     << rxThroughput << " kbps)" << std::endl
                     << "  Sent: " << totalTxBytes[i] << " bytes (" << std::fixed << std::setprecision(2)
                     << txThroughput << " kbps)" << std::endl;
        }

        // Print overall statistics
        std::cout << "\nOverall Statistics:" << std::endl
                 << "  Total received throughput: " << std::fixed << std::setprecision(2) << totalThroughput << " kbps" << std::endl
                 << "  Total sent throughput: " << std::fixed << std::setprecision(2) << totalTxThroughput << " kbps" << std::endl
                 << "  Network parameters: " << bandwidth << ", " << delay << "ms, " << (loss * 100) << "%" << std::endl;

        // Print flow monitor statistics
        flowMonitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
        FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
        
        std::cout << "\nFlow Monitor Statistics:" << std::endl;
        for (auto &stat : stats) {
            std::cout << "  Flow " << stat.first << ":" << std::endl
                     << "    Lost packets: " << stat.second.lostPackets << std::endl
                     << "    Average delay: " << stat.second.delaySum.GetSeconds() / stat.second.rxPackets << "s" << std::endl
                     << "    Jitter: " << stat.second.jitterSum.GetSeconds() / stat.second.rxPackets << "s" << std::endl;
        }
    }
};

int main(int argc, char *argv[]) {
    // Default parameters
    std::string bandwidth = "10Mbps";
    double delay = 10.0;  // ms
    double loss = 0.0;    // 0%
    uint32_t packetSize = 1200; // QUIC typical MTU
    uint32_t numStreams = 3;
    double duration = 30.0;  // Simulation time
    double startTime = 1.0;  // Start time
    double stopTime = startTime + duration;  // End time

    CommandLine cmd;
    cmd.AddValue("bandwidth", "Link bandwidth", bandwidth);
    cmd.AddValue("delay", "Link delay in milliseconds", delay);
    cmd.AddValue("loss", "Packet loss rate (0-1)", loss);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("Http3BaselineSim", LOG_LEVEL_WARN);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);
    LogComponentEnable("PacketSink", LOG_LEVEL_WARN);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    p2p.SetChannelAttribute("Delay", StringValue(std::to_string(delay) + "ms"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));
    NetDeviceContainer devices = p2p.Install(nodes);

    // Set packet loss model
    if (loss > 0) {
        Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(loss));
        devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    std::vector<ApplicationContainer> senders, sinks;
    Http3Simulator simulator(startTime, stopTime);

    // Calculate per-stream rate
    std::string bwValue = bandwidth.substr(0, bandwidth.size() - 4);
    double bwNum = std::stod(bwValue);
    double perStreamBw = bwNum / numStreams;
    std::string perStreamBwStr = std::to_string(perStreamBw) + "Mbps";

    for (uint32_t i = 0; i < numStreams; ++i) {
        uint16_t port = 9000 + i;
        
        // Create and start receiver
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
        sinkApp.Start(Seconds(startTime - 0.5));
        sinkApp.Stop(Seconds(stopTime + 1));
        sinks.push_back(sinkApp);
        
        Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(sinkApp.Get(0));
        simulator.AddSink(sinkPtr);

        // Create and start sender
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        onoff.SetConstantRate(DataRate(perStreamBwStr), packetSize);
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer sender = onoff.Install(nodes.Get(0));
        sender.Start(Seconds(startTime));
        sender.Stop(Seconds(stopTime));
        senders.push_back(sender);

        // Monitor sender data
        Ptr<OnOffApplication> onoffApp = DynamicCast<OnOffApplication>(sender.Get(0));
        onoffApp->TraceConnectWithoutContext("Tx", MakeCallback(&Http3Simulator::UpdateTxBytes, &simulator, i));
    }

    Simulator::Stop(Seconds(stopTime + 1));
    Simulator::Run();

    // Print results
    simulator.PrintResults(duration, bandwidth, delay, loss);

    Simulator::Destroy();
    return 0;
} 