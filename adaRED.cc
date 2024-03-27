/**
 * NOTE: These validation tests are same as provided in ns-2
 * (ns/tcl/test/test-suite-adaptive-red.tcl)
 *
 * In this code, tests 1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14 and 15 refer to tests
 * named red1, red1Adapt, red1ECN, fastlink, fastlinkECN, fastlinkAutowq, fastlinkAutothresh,
 * fastlinkAdaptive, fastlinkAllAdapt, fastlinkAllAdaptECN, fastlinkAllAdapt1, longlink,
 * longlinkAdapt and longlinkAdapt1, respectively in the ns-2 file
 * mentioned above.
 * 
 * Only: 2,4,10,11,12,14 use ARED q-discs
 */

/** Network topology for tests: 1, 2, 3 and 4
 *
 *    10Mb/s, 2ms                            10Mb/s, 4ms
 * n0--------------|                    |---------------n4
 *                 |    1.5Mbps, 20ms   |
 *                 n2------------------n3
 *    10Mb/s, 3ms  |  QueueLimit = 25   |    10Mb/s, 5ms
 * n1--------------|                    |---------------n5
 *
 */

/** Network topology for tests: 6, 7, 8, 9, 10, 11 and 12
 *
 *    100Mb/s, 2ms                          100Mb/s, 4ms
 * n0--------------|                    |---------------n4
 *                 |    15Mbps, 20ms    |
 *                 n2------------------n3
 *    100Mb/s, 3ms |  QueueLimit = 1000 |   100Mb/s, 5ms
 * n1--------------|                    |---------------n5
 *
 */

/** Network topology for tests: 13, 14 and 15
 *
 *    10Mb/s, 0ms                            10Mb/s, 2ms
 * n0--------------|                    |---------------n4
 *                 |    1.5Mbps, 100ms  |
 *                 n2------------------n3
 *    10Mb/s, 1ms  |  QueueLimit = 100  |    10Mb/s, 3ms
 * n1--------------|                    |---------------n5
 *
 */
#include "tutorial-app.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdaptiveRedTests");

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

std::stringstream aredQsize;
std::stringstream aredProactiveDrops;
std::stringstream aredProactiveMarks;
std::stringstream aredCWND;

/**
 * Check the queue disc size and write its stats to the output files.
 *
 * \param queue The queue to check.
 */
void CheckParameters(Ptr<QueueDisc> queue)
{
    uint32_t qSize = queue->GetCurrentSize().GetValue();
    
    QueueDisc::Stats st = queue->GetStats();
    uint32_t unforced_drop = st.GetNDroppedPackets(RedQueueDisc::UNFORCED_DROP);
    uint32_t unforced_mark = st.GetNMarkedPackets(RedQueueDisc::UNFORCED_MARK);


    // check queue disc size every 1/100 of a second
    Simulator::Schedule(Seconds(0.01), &CheckParameters, queue);


    std::ofstream fileAredQsize(aredQsize.str(), std::ios::out | std::ios::app);
    fileAredQsize << Simulator::Now().GetSeconds() << " " << qSize << std::endl;
    fileAredQsize.close();

    std::ofstream fileAredProactiveDrops(aredProactiveDrops.str(), std::ios::out | std::ios::app);
    fileAredProactiveDrops << Simulator::Now().GetSeconds() << " " << unforced_drop << std::endl;
    fileAredProactiveDrops.close();

    std::ofstream fileAredProactiveMarks(aredProactiveMarks.str(), std::ios::out | std::ios::app);
    fileAredProactiveMarks << Simulator::Now().GetSeconds() << " " << unforced_mark << std::endl;
    fileAredProactiveMarks.close();
}

static void CwndChange(uint32_t oldCwnd, uint32_t newCwnd)
{
    std::ofstream fileAredCWND(aredCWND.str(), std::ios::out | std::ios::app);
    fileAredCWND << Simulator::Now().GetSeconds() << " " << oldCwnd << " " << newCwnd << std::endl;
    fileAredCWND.close();
}

/**
 * Setup the apps.
 *
 * \param test The test number.
 */
void BuildAppsTest(uint32_t test)
{
    // SINK is in the right side
    uint16_t port = 50000;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    
    ApplicationContainer sinkApp = sinkHelper.Install(n3n4.Get(1));
    sinkApp.Start(Seconds(sink_start_time));
    sinkApp.Stop(Seconds(sink_stop_time));

    // Attaching sockets using helpers from sixth.cc
    // Clients are in left side

    uint32_t packetSize = 3000;
    uint32_t nPackets = 10000;
    Address sinkAddress(InetSocketAddress(i3i4.GetAddress(1), port));

    Ptr<Socket> n0Socket = Socket::CreateSocket(n0n2.Get(0), TcpSocketFactory::GetTypeId());
    Ptr<Socket> n1Socket = Socket::CreateSocket(n1n2.Get(0), TcpSocketFactory::GetTypeId());
    
    Ptr<TutorialApp> app0 = CreateObject<TutorialApp>();
    Ptr<TutorialApp> app1 = CreateObject<TutorialApp>();


    if (test == 6 || test == 7 || test == 8 || test == 9 || test == 10 || test == 12)
    {
        app0->Setup(n0Socket, sinkAddress, packetSize, nPackets, DataRate("100Mbps"));
        app1->Setup(n1Socket, sinkAddress, packetSize, nPackets, DataRate("100Mbps"));
    }
    else
    {
        app0->Setup(n0Socket, sinkAddress, packetSize, nPackets, DataRate("1Mbps"));
        app1->Setup(n1Socket, sinkAddress, packetSize, nPackets, DataRate("1Mbps"));
    }

    n0n2.Get(0)->AddApplication(app0);
    app0->SetStartTime(Seconds(client_start_time));
    app0->SetStopTime(Seconds(client_stop_time));

    n1n2.Get(0)->AddApplication(app1);
    app1->SetStartTime(Seconds(client_start_time));
    app1->SetStopTime(Seconds(client_stop_time));

    // Defining callbacks to CwndChange func
    n0Socket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange));
}


void RunSimulation(uint32_t aredTest, bool writeForPlot, TimeValue interval)
{
    LogComponentEnable("RedQueueDisc", LOG_LEVEL_INFO);

    std::string aredLinkDataRate = "1.5Mbps";
    std::string aredLinkDelay = "20ms";

    std::string pathOut;

    bool printAredStats = true;

    global_start_time = 0.0;
    sink_start_time = global_start_time;
    client_start_time = global_start_time + 1.5;
    global_stop_time = 7.0;
    sink_stop_time = global_stop_time + 3.0;
    client_stop_time = global_stop_time - 2.0;

    pathOut = "plots"; // directory to save plots

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

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno")); // 42 = headers size
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

    // ARED params
    Config::SetDefault("ns3::RedQueueDisc::ARED", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::Interval", interval); // Interval Param

    if (aredTest == 2) // test 2: red1Adapt
    {
        Config::SetDefault("ns3::RedQueueDisc::LInterm", DoubleValue(10));
        Config::SetDefault("ns3::RedQueueDisc::MaxSize", StringValue("25p"));
    }
    else if (aredTest == 4) // test 4: red1AdaptECN
    {
        Config::SetDefault("ns3::RedQueueDisc::LInterm", DoubleValue(10));
        Config::SetDefault("ns3::RedQueueDisc::MaxSize", StringValue("25p"));
        Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("On"));
        Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    }
    else if (aredTest == 10) // test 10: fastlinkAllAdapt
    {
        Config::SetDefault("ns3::RedQueueDisc::LInterm", DoubleValue(10));
    }
    else if (aredTest == 11) // test 11: fastlinkAllAdaptECN
    {
        Config::SetDefault("ns3::RedQueueDisc::UseHardDrop", BooleanValue(false));
        Config::SetDefault("ns3::RedQueueDisc::LInterm", DoubleValue(10));
        Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("On"));
        Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    }
    else if (aredTest == 12) // test 12: fastlinkAllAdapt1
    {
        Config::SetDefault("ns3::RedQueueDisc::LInterm", DoubleValue(10));
        Config::SetDefault("ns3::RedQueueDisc::TargetDelay", TimeValue(Seconds(0.2)));
    }
    else if (aredTest == 14) // test 14: longlinkAdapt
    {
        Config::SetDefault("ns3::RedQueueDisc::LInterm", DoubleValue(10));
        Config::SetDefault("ns3::RedQueueDisc::MaxSize", StringValue("100p"));
    }

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

    if (aredTest == 1 || aredTest == 2 || aredTest == 3 || aredTest == 4)
    {
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
    }
    else if (aredTest == 13 || aredTest == 14 || aredTest == 15)
    {
        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("0ms"));
        devn0n2 = p2p.Install(n0n2);
        tchPfifo.Install(devn0n2);

        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));
        devn1n2 = p2p.Install(n1n2);
        tchPfifo.Install(devn1n2);

        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue(aredLinkDataRate));
        p2p.SetChannelAttribute("Delay", StringValue("100ms"));
        devn2n3 = p2p.Install(n2n3);
        // only backbone link has ARED queue disc
        queueDiscs = tchRed.Install(devn2n3);

        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));
        devn3n4 = p2p.Install(n3n4);
        tchPfifo.Install(devn3n4);

        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("3ms"));
        devn3n5 = p2p.Install(n3n5);
        tchPfifo.Install(devn3n5);
    }
    else if (aredTest == 6 || aredTest == 7 || aredTest == 8 || aredTest == 9 || aredTest == 10 ||
             aredTest == 11 || aredTest == 12)
    {
        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));
        devn0n2 = p2p.Install(n0n2);
        tchPfifo.Install(devn0n2);

        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("3ms"));
        devn1n2 = p2p.Install(n1n2);
        tchPfifo.Install(devn1n2);

        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue("15Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue(aredLinkDelay));
        devn2n3 = p2p.Install(n2n3);
        // only backbone link has ARED queue disc
        queueDiscs = tchRed.Install(devn2n3);

        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("4ms"));
        devn3n4 = p2p.Install(n3n4);
        tchPfifo.Install(devn3n4);

        p2p.SetQueue("ns3::DropTailQueue");
        p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("5ms"));
        devn3n5 = p2p.Install(n3n5);
        tchPfifo.Install(devn3n5);
    }

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
        aredQsize << pathOut << "/" << "ared-qlen.plotme";
        aredProactiveDrops << pathOut << "/" << "ared-proactive-drops.plotme";
        aredProactiveMarks << pathOut << "/" << "ared-proactive-marks.plotme";
        aredCWND << pathOut << "/" << "ared-cwnd.plotme";

        remove(aredQsize.str().c_str());
        remove(aredProactiveDrops.str().c_str());
        remove(aredProactiveMarks.str().c_str());
        remove(aredCWND.str().c_str());

        Ptr<QueueDisc> queue = queueDiscs.Get(0);
        Simulator::ScheduleNow(&CheckParameters, queue);
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

    if (aredTest == 1 || aredTest == 2 || aredTest == 3 || aredTest == 4 || aredTest == 13)
    {
        if (st.GetNDroppedPackets(QueueDisc::INTERNAL_QUEUE_DROP) == 0)
        {
            std::cout << "There should be some drops due to queue full" << std::endl;
            exit(1);
        }
    }
    else
    {
        if (st.GetNDroppedPackets(QueueDisc::INTERNAL_QUEUE_DROP) != 0)
        {
            std::cout << "There should be zero drops due to queue full" << std::endl;
            exit(1);
        }
    }

    if (printAredStats)
    {
        std::cout << "*** ARED stats from Node 2 queue ***" << std::endl;
        std::cout << st << std::endl;
    }

    Simulator::Destroy();

    return;
}

int main(int argc, char* argv[])
{
    uint32_t aredTest = 2;
    bool writeForPlot = false;
    double interval = 0.5;

    // Configuration and command line parameter parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("testNumber", "Run test 1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14 or 15", aredTest);
    cmd.AddValue("writeForPlot", "Write results for plot (gnuplot)", writeForPlot);
    cmd.AddValue("interval", "Time interval to update m_curMaxP", interval);

    cmd.Parse(argc, argv);

    // Allowed test cases are: 2,4,10,11,12,14.
    bool allowedArray[15] = {false, true, false, true, false, false, false, false, false, true, true, true, false, true, false};

    if ((aredTest < 1) || !(allowedArray[int(aredTest)-1]) || (aredTest > 15))
    {
        std::cout << "Invalid test number. Supported tests are 2,4,10,11,12,14." << std::endl;
        exit(1);
    }
    std::cout << "Interval: " << interval << std::endl;

    RunSimulation(aredTest, writeForPlot, TimeValue(Seconds(interval)));

    return 0;
}
