// Choosing test 2 from adaptive-red-tests.cc

/** Network topology:
 *
 *    10Mb/s, 2ms                            10Mb/s, 4ms
 * n0--------------|                    |---------------n4
 *                 |    1.5Mbps, 20ms   |
 *                 n2------------------n3
 *    10Mb/s, 3ms  |  QueueLimit = 25   |    10Mb/s, 5ms
 * n1--------------|                    |---------------n5
 *
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdaptiveRedTests");

uint32_t checkTimes;     //!< Number of times the queues have been checked.
double avgQueueDiscSize; //!< Average QueueDisc size.

// The times
double global_start_time; //!< Global start time
double global_stop_time;  //!< Global stop time.
double sink_start_time;   //!< Sink start time.
double sink_stop_time;    //!< Sink stop time.
double client_start_time; //!< Client start time.
double client_stop_time;  //!< Client stop time.

NodeContainer n0n2; //!< Nodecontainer n0 + n2.
NodeContainer n1n2; //!< Nodecontainer n1 + n2.
NodeContainer n2n3; //!< Nodecontainer n2 + n3.
NodeContainer n3n4; //!< Nodecontainer n3 + n4.
NodeContainer n3n5; //!< Nodecontainer n3 + n5.

Ipv4InterfaceContainer i0i2; //!< IPv4 interface container i0 + i2.
Ipv4InterfaceContainer i1i2; //!< IPv4 interface container i1 + i2.
Ipv4InterfaceContainer i2i3; //!< IPv4 interface container i2 + i3.
Ipv4InterfaceContainer i3i4; //!< IPv4 interface container i3 + i4.
Ipv4InterfaceContainer i3i5; //!< IPv4 interface container i3 + i5.

std::stringstream filePlotQueueDisc;    //!< Output file name for queue disc size.
std::stringstream filePlotQueueDiscAvg; //!< Output file name for queue disc average.

/**
 * Check the queue disc size and write its stats to the output files.
 *
 * \param queue The queue to check.
 */
void
CheckQueueDiscSize(Ptr<QueueDisc> queue)
{
    uint32_t qSize = queue->GetCurrentSize().GetValue();

    avgQueueDiscSize += qSize;
    checkTimes++;

    // check queue disc size every 1/100 of a second
    Simulator::Schedule(Seconds(0.01), &CheckQueueDiscSize, queue);

    std::ofstream fPlotQueueDisc(filePlotQueueDisc.str(), std::ios::out | std::ios::app);
    fPlotQueueDisc << Simulator::Now().GetSeconds() << " " << qSize << std::endl;
    fPlotQueueDisc.close();

    std::ofstream fPlotQueueDiscAvg(filePlotQueueDiscAvg.str(), std::ios::out | std::ios::app);
    fPlotQueueDiscAvg << Simulator::Now().GetSeconds() << " " << avgQueueDiscSize / checkTimes
                      << std::endl;
    fPlotQueueDiscAvg.close();
}

/**
 * Setup the apps.
 *
 * \param test The test number.
 */
void
BuildAppsTest(uint32_t test)
{
    // SINK is in the right side
    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(n3n4.Get(1));
    sinkApp.Start(Seconds(sink_start_time));
    sinkApp.Stop(Seconds(sink_stop_time));

    // Connection one
    // Clients are in left side
    /*
     * Create the OnOff applications to send TCP to the server
     * onoffhelper is a client that send data to TCP destination
     */
    OnOffHelper clientHelper1("ns3::TcpSocketFactory", Address());
    clientHelper1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper1.SetAttribute("PacketSize", UintegerValue(1000));

    // Connection two
    OnOffHelper clientHelper2("ns3::TcpSocketFactory", Address());
    clientHelper2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper2.SetAttribute("PacketSize", UintegerValue(1000));

    // ForTest2
    clientHelper1.SetAttribute("DataRate", DataRateValue(DataRate("10Mb/s")));
    clientHelper2.SetAttribute("DataRate", DataRateValue(DataRate("10Mb/s")));


    ApplicationContainer clientApps1;
    AddressValue remoteAddress(InetSocketAddress(i3i4.GetAddress(1), port));
    clientHelper1.SetAttribute("Remote", remoteAddress);
    clientApps1.Add(clientHelper1.Install(n0n2.Get(0)));
    clientApps1.Start(Seconds(client_start_time));
    clientApps1.Stop(Seconds(client_stop_time));

    ApplicationContainer clientApps2;
    clientHelper2.SetAttribute("Remote", remoteAddress);
    clientApps2.Add(clientHelper2.Install(n1n2.Get(0)));
    clientApps2.Start(Seconds(client_start_time));
    clientApps2.Stop(Seconds(client_stop_time));
}

int
main(int argc, char* argv[])
{
    LogComponentEnable("RedQueueDisc", LOG_LEVEL_INFO);

    uint32_t aredTest;
    std::string aredLinkDataRate = "1.5Mbps";
    std::string aredLinkDelay = "20ms";

    std::string pathOut;
    bool writeForPlot = false;

    bool printAredStats = true;

    global_start_time = 0.0;
    sink_start_time = global_start_time;
    client_start_time = global_start_time + 1.5;
    global_stop_time = 7.0;
    sink_stop_time = global_stop_time + 3.0;
    client_stop_time = global_stop_time - 2.0;

    // Configuration and command line parameter parsing
    aredTest = 2;
    // Will only save in the directory if enable opts below
    pathOut = "."; // Current directory
    CommandLine cmd(__FILE__);
    cmd.AddValue("pathOut",
                 "Path to save results from --writeForPlot/--writePcap/--writeFlowMonitor",
                 pathOut);
    cmd.AddValue("writeForPlot", "Write results for plot (gnuplot)", writeForPlot);


    cmd.Parse(argc, argv);

    NS_LOG_INFO("Create nodes");
    NodeContainer c;
    c.Create(6);
    Names::Add("N0", c.Get(0));
    Names::Add("N1", c.Get(1));
    Names::Add("N2", c.Get(2));
    Names::Add("N3", c.Get(3));
    Names::Add("N4", c.Get(4));
    Names::Add("N5", c.Get(5));
    n0n2 = NodeContainer(c.Get(0), c.Get(2));
    n1n2 = NodeContainer(c.Get(1), c.Get(2));
    n2n3 = NodeContainer(c.Get(2), c.Get(3));
    n3n4 = NodeContainer(c.Get(3), c.Get(4));
    n3n5 = NodeContainer(c.Get(3), c.Get(5));

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    // 42 = headers size
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1000 - 42));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));

    uint32_t meanPktSize = 1000;

    // RED params
    NS_LOG_INFO("Set RED params");
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", StringValue("1000p"));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(meanPktSize));
    Config::SetDefault("ns3::RedQueueDisc::Wait", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::Gentle", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(0.002));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(5));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(15));

    // ForTest2 : red1Adapt
    Config::SetDefault("ns3::RedQueueDisc::ARED", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::LInterm", DoubleValue(10));
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", StringValue("25p"));
    

    NS_LOG_INFO("Install internet stack on all nodes.");
    InternetStackHelper internet;
    internet.Install(c);

    TrafficControlHelper tchPfifo;
    uint16_t handle = tchPfifo.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    tchPfifo.AddInternalQueues(handle, 3, "ns3::DropTailQueue", "MaxSize", StringValue("1000p"));

    TrafficControlHelper tchRed;
    tchRed.SetRootQueueDisc("ns3::RedQueueDisc",
                            "LinkBandwidth",
                            StringValue(aredLinkDataRate),
                            "LinkDelay",
                            StringValue(aredLinkDelay));

    NS_LOG_INFO("Create channels");
    PointToPointHelper p2p;

    NetDeviceContainer devn0n2;
    NetDeviceContainer devn1n2;
    NetDeviceContainer devn2n3;
    NetDeviceContainer devn3n4;
    NetDeviceContainer devn3n5;

    QueueDiscContainer queueDiscs;

    // ForTest2
    p2p.SetQueue("ns3::DropTailQueue");
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    devn0n2 = p2p.Install(n0n2);
    tchPfifo.Install(devn0n2);

    p2p.SetQueue("ns3::DropTailQueue");
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("3ms"));
    devn1n2 = p2p.Install(n1n2);
    tchPfifo.Install(devn1n2);

    p2p.SetQueue("ns3::DropTailQueue");
    p2p.SetDeviceAttribute("DataRate", StringValue(aredLinkDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(aredLinkDelay));
    devn2n3 = p2p.Install(n2n3);
    // only backbone link has ARED queue disc
    queueDiscs = tchRed.Install(devn2n3);

    p2p.SetQueue("ns3::DropTailQueue");
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("4ms"));
    devn3n4 = p2p.Install(n3n4);
    tchPfifo.Install(devn3n4);

    p2p.SetQueue("ns3::DropTailQueue");
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));
    devn3n5 = p2p.Install(n3n5);
    tchPfifo.Install(devn3n5);
    

    NS_LOG_INFO("Assign IP Addresses");
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    i0i2 = ipv4.Assign(devn0n2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    i1i2 = ipv4.Assign(devn1n2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    i2i3 = ipv4.Assign(devn2n3);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    i3i4 = ipv4.Assign(devn3n4);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    i3i5 = ipv4.Assign(devn3n5);

    // Set up the routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    BuildAppsTest(aredTest);

    if (writeForPlot)
    {
        filePlotQueueDisc << pathOut << "/"
                          << "ared-queue-disc.plotme";
        filePlotQueueDiscAvg << pathOut << "/"
                             << "ared-queue-disc_avg.plotme";

        remove(filePlotQueueDisc.str().c_str());
        remove(filePlotQueueDiscAvg.str().c_str());
        Ptr<QueueDisc> queue = queueDiscs.Get(0);
        Simulator::ScheduleNow(&CheckQueueDiscSize, queue);
    }

    Simulator::Stop(Seconds(sink_stop_time));
    Simulator::Run();

    QueueDisc::Stats st = queueDiscs.Get(0)->GetStats();

    if (st.GetNDroppedPackets(RedQueueDisc::UNFORCED_DROP) == 0 &&
        st.GetNMarkedPackets(RedQueueDisc::UNFORCED_MARK) == 0)
    {
        std::cout << "There should be some unforced drops or marks" << std::endl;
        exit(1);
    }

    // ForTest2
    if (st.GetNDroppedPackets(QueueDisc::INTERNAL_QUEUE_DROP) == 0)
    {
        std::cout << "There should be some drops due to queue full" << std::endl;
        exit(1);
    }

    if (printAredStats)
    {
        std::cout << "*** ARED stats from Node 2 queue ***" << std::endl;
        std::cout << st << std::endl;
    }

    Simulator::Destroy();

    return 0;
}
