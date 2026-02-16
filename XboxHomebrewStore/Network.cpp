#include "Network.h"
#include "Debug.h"

int Network::Init()
{
    XNetStartupParams xnsp;
    WSADATA wsaData;
    XNADDR xna;
    int timeout = 0;

    memset(&xnsp, 0, sizeof(xnsp));
    xnsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
    xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;

    xnsp.cfgPrivatePoolSizeInPages = 64;
    xnsp.cfgEnetReceiveQueueLength = 16;
    xnsp.cfgIpFragMaxSimultaneous = 16;
    xnsp.cfgIpFragMaxPacketDiv256 = 32;
    xnsp.cfgSockMaxSockets = 64;

    xnsp.cfgSockDefaultRecvBufsizeInK = 64;
    xnsp.cfgSockDefaultSendBufsizeInK = 64;


    XNetStartup(&xnsp);
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    Debug::Print("Waiting for network...\n");

    do {
        if (XNetGetTitleXnAddr(&xna) != XNET_GET_XNADDR_PENDING) {
            break;
        }
        Sleep(100);
        timeout += 100;
    } while (timeout < 10000);

    if (timeout >= 10000) {
        Debug::Print("Network timeout!\n");
        return -1;
    }

    Debug::Print("Network ready. IP: %d.%d.%d.%d\n",
        xna.ina.S_un.S_un_b.s_b1,
        xna.ina.S_un.S_un_b.s_b2,
        xna.ina.S_un.S_un_b.s_b3,
        xna.ina.S_un.S_un_b.s_b4);

    return 0;
}