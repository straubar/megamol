/*
 * testdiscovery.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */

#include "testdiscovery.h"
#include "vislib/ClusterDiscoveryService.h"
#include "vislib/NetworkInformation.h"
#include "vislib/SystemInformation.h"
#include "vislib/Trace.h"

#include <iostream>

#include "testhelper.h"


class MyListener : public vislib::net::ClusterDiscoveryListener {
    
    virtual void OnNodeFound(const vislib::net::ClusterDiscoveryService& src, 
        const vislib::net::SocketAddress& addr);

    virtual void OnNodeLost(const vislib::net::ClusterDiscoveryService& src,
        const vislib::net::SocketAddress& addr, const NodeLostReason reason);

    virtual void OnUserMessage(const vislib::net::ClusterDiscoveryService& src,
        const vislib::net::SocketAddress& sender, const UINT16 msgType, 
        const BYTE *msgBody);
};


/*
 * MyListener::OnNodeFound
 */
void MyListener::OnNodeFound(const vislib::net::ClusterDiscoveryService& src, 
        const vislib::net::SocketAddress& addr) {
    const char *WELCOME_MESSAGE = "Okaerinasai";
    const char *SINGLE_MESSAGE = "Single message";
    std::cout << addr.ToStringA() << " was found for \"" << src.GetName() 
        << "\"" << std::endl;

    std::cout << "Now I know ";
    for (SIZE_T i = 0; i < src.CountPeers(); i++) {
        std::cout << std::endl << " " << src[i].ToStringA();
    }
    std::cout << std::endl;

    const_cast<vislib::net::ClusterDiscoveryService&>(src).SendUserMessage(
        vislib::net::ClusterDiscoveryService::MSG_TYPE_USER, 
        WELCOME_MESSAGE, ::strlen(WELCOME_MESSAGE) + 1);

    if (src.CountPeers() > 0) {
        const_cast<vislib::net::ClusterDiscoveryService&>(src).SendUserMessage(
            src[0],
            vislib::net::ClusterDiscoveryService::MSG_TYPE_USER + 1, 
            SINGLE_MESSAGE, ::strlen(SINGLE_MESSAGE) + 1);
    }
}


/*
 * MyListener::OnNodeLost
 */
void MyListener::OnNodeLost(const vislib::net::ClusterDiscoveryService& src,
        const vislib::net::SocketAddress& addr, const NodeLostReason reason) {
    std::cout << addr.ToStringA() 
        << ((reason == LOST_EXPLICITLY) ? " disconnected" : " was lost")
        << " from \"" << src.GetName() << "\"" << std::endl;
}

/*
 * MyListener::OnUserMessage
 */
void MyListener::OnUserMessage(const vislib::net::ClusterDiscoveryService& src,
        const vislib::net::SocketAddress& sender, const UINT16 msgType, 
        const BYTE *msgBody) {
    std::cout << "Received user message " << msgType << " (\""
        << reinterpret_cast<const char *>(msgBody) << "\") from "
        << sender.ToStringA() << std::endl;
}


void TestClusterDiscoveryService(void) {
    using namespace vislib;
    using namespace vislib::net;
    using namespace vislib::sys;

    UINT oldLevel = Trace::GetInstance().GetLevel();
    //Trace::GetInstance().SetLevel(Trace::LEVEL_ERROR);

    MyListener myListener;

    // TODO: MIDL_uhyper_crowbar
    Socket::Startup();

    ClusterDiscoveryService cds("testnet", 
        SocketAddress(SocketAddress::FAMILY_INET, IPAddress(SystemInformation::ComputerNameA()), 12345),
        IPAddress("129.69.215.255"));
    cds.AddListener(&myListener);
    cds.Start();
    Thread::Sleep(60 * 1000);
    cds.Stop();

    Trace::GetInstance().SetLevel(oldLevel);
    Socket::Cleanup();
}
