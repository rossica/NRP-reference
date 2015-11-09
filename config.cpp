#include <string>
#include <cstring>
#include "config.h"
#include "protocol.h"

using namespace std;

namespace nrpd
{
    NrpdConfig::NrpdConfig()
    {
        m_port = 8080;
        m_configPath = "";
        m_enableServer = true;
        m_enableClient = true;
        m_entropyServerAddress = nullptr;
        m_clientRequestIntervalSeconds = 60;
        m_clientReceiveTimeout = 5;
    }

    NrpdConfig::NrpdConfig(string* path)
    {
        m_port = 8080;
        m_configPath = *path;
    }

    unsigned short NrpdConfig::port()
    {
        return m_port;
    }

    bool NrpdConfig::enableServer()
    {
        return m_enableServer;
    }

    bool NrpdConfig::enableClient()
    {
        return m_enableClient;
    }

    int NrpdConfig::clientRequestInterval()
    {
        return m_clientRequestIntervalSeconds;
    }

    int NrpdConfig::receiveTimeout()
    {
        return m_clientReceiveTimeout;
    }

    int NrpdConfig::activeServerCount(bool ipv6)
    {
        int count = 0;
        // The list of servers should never get large enough that iterating
        // to calculate this every call will be a burden.
        for(ServerRecord rec : m_activeServers)
        {
            if(rec.ipv6 == ipv6)
            {
                count +=1;
            }
        }

        return count;
    }

    unique_ptr<unsigned char[]> NrpdConfig::GetServerList(bool ipv6, int count, int& out_size)
    {
        // Optimization: Is this calculation necessary?
        int actualCount = activeServerCount(ipv6);
        unique_ptr<unsigned char[]> srvlist;
        pNrp_Message_Ip4Peer ip4Msg;
        pNrp_Message_Ip6Peer ip6Msg;
        int size;

        if(ipv6)
        {
            size = min(actualCount, count) * sizeof(Nrp_Message_Ip6Peer);
        }
        else
        {
            size = min(actualCount, count) * sizeof(Nrp_Message_Ip4Peer);
        }

        srvlist = make_unique<unsigned char[]>(size);

        if(ipv6)
        {
            ip6Msg = (pNrp_Message_Ip6Peer) srvlist.get();
        }
        else
        {
            ip4Msg = (pNrp_Message_Ip4Peer) srvlist.get();
        }

        // TODO: find a way to rotate through this list so as not to return
        // the same servers every time.

        for(ServerRecord& rec : m_activeServers)
        {
            if(rec.ipv6 == ipv6)
            {
                if(ipv6)
                {
                    memcpy(ip6Msg->ip, rec.host6, sizeof(ip6Msg->ip));
                    ip6Msg->port = rec.port;
                    ip6Msg++;
                }
                else
                {
                    memcpy(ip4Msg->ip, rec.host4, sizeof(ip4Msg->ip));
                    ip4Msg->port = rec.port;
                    ip4Msg++;
                }
            }
        }

        return srvlist;
    }
}
