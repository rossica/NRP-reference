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
        m_defaultEntropyResponse = DEFAULT_ENTROPY_SIZE;
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

    int NrpdConfig::defaultEntropyResponse()
    {
        return m_defaultEntropyResponse;
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

    int NrpdConfig::ActiveServerCount(nrpd_msg_type type)
    {
        bool ipv6;

        if(type != ip4peers && type != ip6peers)
        {
            return 0;
        }

        ipv6 = (type == ip6peers) ? true : false;

        int count = 0;
        // The list of servers should never get large enough that iterating
        // to calculate this every call will be a burden.
        for(ServerRecord& rec : m_activeServers)
        {
            if(rec.ipv6 == ipv6)
            {
                count +=1;
            }
        }

        return count;
    }

    unique_ptr<unsigned char[]> NrpdConfig::GetServerList(nrpd_msg_type type, int count, int& outSize)
    {
        unique_ptr<unsigned char[]> srvlist;
        pNrp_Message_Ip4Peer ip4Msg;
        pNrp_Message_Ip6Peer ip6Msg;
        int size;
        int itr = 0;
        int actualCount;
        bool ipv6;

        if((type != ip4peers && type != ip6peers) || count <= 0)
        {
            return nullptr;
        }

        ipv6 = (type == ip6peers) ? true : false;

        // Optimization: Is this calculation necessary?
        actualCount = ActiveServerCount(type);

        if(actualCount <= 0)
        {
            return nullptr;
        }

        actualCount = min(actualCount, count);

        if(ipv6)
        {
            size = actualCount * sizeof(Nrp_Message_Ip6Peer);
        }
        else
        {
            size = actualCount* sizeof(Nrp_Message_Ip4Peer);
        }

        srvlist = make_unique<unsigned char[]>(size);

        outSize = size;

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
                itr++;
            }

            // Exit the loop when requested count of servers are copied
            if(itr >= actualCount)
            {
                break;
            }
        }

        return srvlist;
    }
}
