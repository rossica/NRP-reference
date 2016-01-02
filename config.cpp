#include <string>
#include <cstring>
#include "config.h"
#include "protocol.h"

using namespace std;

namespace nrpd
{

/// ServerRecord member functions ///

    constexpr bool ServerRecord::operator==(ServerRecord const& rhs) const
    {
        if(ipv6 != rhs.ipv6)
        {
            return false;
        }

        if(port != rhs.port)
        {
            return false;
        }

        if(ipv6)
        {
            if(memcmp(host6, rhs.host6, sizeof(host6)) != 0)
            {
                return false;
            }
        }
        else
        {
            if(memcmp(host4, rhs.host4, sizeof(host4)) != 0)
            {
                return false;
            }
        }
        return true;
    }

    constexpr bool ServerRecord::operator<(ServerRecord const& rhs) const
    {
        if(ipv6 != rhs.ipv6)
        {
            if(ipv6)
            {
                // Assumption: rhs.ipv6 must be false
                return false;
            }
            else
            {
                // Assumption: rhs.ipv6 must be true
                return true;
            }
        }

        if(ipv6)
        {
            if(memcmp(host6, rhs.host6, sizeof(host6)) < 0)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if(memcmp(host4, rhs.host4, sizeof(host4)) < 0)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    void ServerRecord::Initialize()
    {
        memset(host6, 0, sizeof(host6));
        port = 0;
        failureCount = 0;
        lastaccessTime = 0;
        retryTime = 0;
        flags = 0xFFFF;
    }


/// NrpdConfig member functions ///

    NrpdConfig::NrpdConfig()
    {
        m_port = 8080;
        m_configPath = "";
        m_enableServer = true;
        m_enableClient = true;
        m_clientRequestIntervalSeconds = CLIENT_MIN_RETRY_SECONDS;
        m_clientReceiveTimeout = CLIENT_RESPONSE_TIMEOUT_SECONDS;
        m_defaultEntropyResponse = DEFAULT_ENTROPY_SIZE;
        // Bad servers are banned for 24hrs
        m_bannedServers = make_shared<ClientMRUCache>(60*60*24);
        m_activeIterator = m_activeServers.end();
        m_probationaryIterator = m_probationaryServers.end();
        m_prevReturnedProbationary = true;
        m_countIp6Servers = -1;
        m_countIp4Servers = -1;
        m_clientEnableIp4 = true;
        m_clientEnableIp6 = true;
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

    bool NrpdConfig::enablePeersResponse(nrpd_msg_type type)
    {
        if(type == ip4peers)
        {
            return m_enableIp4Peers;
        }

        if(type == ip6peers)
        {
            return m_enableIp6Peers;
        }

        return false;
    }

    int NrpdConfig::clientRequestInterval()
    {
        return m_clientRequestIntervalSeconds;
    }

    int NrpdConfig::receiveTimeout()
    {
        return m_clientReceiveTimeout;
    }

    bool NrpdConfig::enableClientIp4()
    {
        return m_clientEnableIp4;
    }

    bool NrpdConfig::enableClientIp6()
    {
        return m_clientEnableIp6;
    }

    int NrpdConfig::ActiveServerCount(nrpd_msg_type type)
    {
        bool ipv6;
        int count = 0;

        if(type != ip4peers && type != ip6peers)
        {
            return 0;
        }

        ipv6 = (type == ip6peers) ? true : false;

        // Optimization to use already-calculated value
        if(ipv6 && (m_countIp6Servers > -1))
        {
            return m_countIp6Servers;
        }
        else if(!ipv6 && (m_countIp4Servers > -1))
        {
            return m_countIp4Servers;
        }


        {
            // Hold lock until done iterating over active server list
            lock_guard<mutex> lock(m_activeMutex);

            // The list of servers should never get large enough that iterating
            // to calculate this every call will be a burden.
            for(auto& rec : m_activeServers)
            {
                if(rec.second.ipv6 == ipv6)
                {
                    count +=1;
                }
            }
        } // end lock scope

        if(ipv6)
        {
            m_countIp6Servers = count;
        }
        else
        {
            m_countIp4Servers = count;
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

        if(srvlist == nullptr)
        {
            return nullptr;
        }

        outSize = size;

        if(ipv6)
        {
            ip6Msg = (pNrp_Message_Ip6Peer) srvlist.get();
        }
        else
        {
            ip4Msg = (pNrp_Message_Ip4Peer) srvlist.get();
        }

        {
            // Hold lock until done iterating over active server list
            lock_guard<mutex> lock(m_activeMutex);

            // TODO: find a way to rotate through this list so as not to return
            // the same servers every time.
            for(auto& rec : m_activeServers)
            {
                if(rec.second.ipv6 == ipv6)
                {
                    if(ipv6)
                    {
                        memcpy(ip6Msg->ip, rec.second.host6, sizeof(ip6Msg->ip));
                        ip6Msg->port = rec.second.port;
                        ip6Msg++;
                    }
                    else
                    {
                        memcpy(ip4Msg->ip, rec.second.host4, sizeof(ip4Msg->ip));
                        ip4Msg->port = rec.second.port;
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
        } // end lock scope

        return srvlist;
    }


    ServerRecord& NrpdConfig::GetNextServer()
    {
        // (Re-)Initialize or increment iterator
        if(m_activeIterator == m_activeServers.end())
        {
            m_activeIterator = m_activeServers.begin();
        }
        else
        {
            m_activeIterator = next(m_activeIterator);
        }

        // (Re-)Initialize or increment iterator
        if(m_probationaryIterator == m_probationaryServers.end())
        {
            m_probationaryIterator = m_probationaryServers.begin();
        }
        else
        {
            m_probationaryIterator = next(m_probationaryIterator);
        }

        // Return a server from either the active server list or probationary
        // server list depending on which was returned last time and which
        // has servers in it to return.

        if(m_prevReturnedProbationary)
        {
            // Previously returned server was on the probationary server list.
            // Try to return one from the active servers list now.
            if(m_activeIterator != m_activeServers.end())
            {
                m_prevReturnedProbationary = !m_prevReturnedProbationary;
                return (*m_activeIterator).second;
            }
            else if(m_probationaryIterator != m_probationaryServers.end())
            {
                // There are no servers in the active list
                // Try the probationary list
                return *m_probationaryIterator;
            }
            else
            {
                // There are no servers in either list; this shouldn't happen
                // There is guaranteed to be at least one server in the list
                // of configured servers.
                // TODO: Log here

                return m_configuredServers.front();
            }
        }
        else
        {
            // Previously returned a server from the active server list.
            // Try to return one from the probationary list now.
            if(m_probationaryIterator != m_probationaryServers.end())
            {
                m_prevReturnedProbationary = !m_prevReturnedProbationary;
                return *m_probationaryIterator;
            }
            else if(m_activeIterator != m_activeServers.end())
            {
                // There are no probationary servers, so just return one
                // from the active list.
                return (*m_activeIterator).second;
            }
            else
            {
                // There are no server in either list; this shouldn't happen.
                // There is guaranteed to be at least one server in the list
                // of configured servers.
                // TODO: log here

                return m_configuredServers.front();
            }
        }
    }


    void NrpdConfig::IncrementServerFailCount(ServerRecord& serv)
    {
        serv.failureCount += 1;
        serv.lastaccessTime = time(nullptr);

        // Server has failed too many times, remove it
        if(serv.failureCount + 1 >= CLIENT_MAX_SERVER_TIMEOUT_COUNT)
        {
            // Add server to banned list by copying IP address to temp sockaddr_storage
            sockaddr_storage stor;
            sockaddr_in& ip4Addr = (sockaddr_in&) stor;
            sockaddr_in6& ip6Addr = (sockaddr_in6&) stor;

            if(serv.ipv6)
            {
                stor.ss_family = AF_INET6;
                memcpy(&(ip6Addr.sin6_addr), serv.host6, sizeof(ip6Addr.sin6_addr));
            }
            else
            {
                stor.ss_family = AF_INET;
                memcpy(&(ip4Addr.sin_addr), serv.host4, sizeof(ip4Addr.sin_addr));
            }

            m_bannedServers->Add(stor);

            // remove from probationary list
            if(serv.probationary)
            {
                // Make sure the iterator matches before removal by iterator
                if(serv == *m_probationaryIterator)
                {
                    lock_guard<mutex> lock(m_probationaryMutex);

                    if(serv.ipv6)
                    {
                        m_countIp6Servers -= 1;
                    }
                    else
                    {
                        m_countIp4Servers -= 1;
                    }

                    auto tempIterator = prev(m_probationaryIterator);
                    m_probationaryServers.erase(m_probationaryIterator);
                    m_probationaryIterator = tempIterator;
                }
                else
                {
                    // Somehow the iterators got out of sync with parameter;
                    // this shouldn't have happened.
                    // Remove the server anyway.
                    // TODO: Log here
                    lock_guard<mutex> lock(m_probationaryMutex);

                    m_probationaryServers.remove(serv);
                }
            }
            else // remove from active list
            {
                // Make sure the iterator matches before removal by iterator
                if(serv == (*m_activeIterator).second)
                {
                    lock_guard<mutex> lock(m_activeMutex);

                    if(serv.ipv6)
                    {
                        m_countIp6Servers -= 1;
                    }
                    else
                    {
                        m_countIp4Servers -= 1;
                    }
                    auto tempIterator = prev(m_activeIterator);
                    m_activeServers.erase(m_activeIterator);
                    m_activeIterator = tempIterator;
                }
                else
                {
                    // Iterator out of sync with parameter;
                    // this shouldn't have happened.
                    // Remove the server anyway.
                    // TODO: log here
                    lock_guard<mutex> lock(m_activeMutex);

                    m_activeServers.erase(serv);
                }
            }
        }
    }


    void NrpdConfig::MarkServerSuccessful(ServerRecord& serv)
    {
        // Reset the server failure count
        serv.failureCount = 0;
        serv.lastaccessTime = time(nullptr);

        // If on the probationary server list, move to the active server list
        if(serv.probationary == true)
        {
            serv.probationary = false;

            {
                lock_guard<mutex> lock(m_activeMutex);
                // Add to the active server list under lock
                auto res = m_activeServers.emplace(serv, serv);

                // If a new element was added successfully
                if(res.second == true)
                {
                    if(serv.ipv6)
                    {
                        m_countIp6Servers += 1;
                    }
                    else
                    {
                        m_countIp4Servers += 1;
                    }
                }

            } // end lock scope


            // Remove from the probationary server list
            // First, check that the iterator points to the same object
            if(serv == *m_probationaryIterator)
            {
                lock_guard<mutex> lock(m_probationaryMutex);

                auto tempIterator = prev(m_probationaryIterator);
                m_probationaryServers.erase(m_probationaryIterator);
                m_probationaryIterator = tempIterator;
            }
            else
            {
                // This shouldn't have happened.
                // TODO: log here
                lock_guard<mutex> lock(m_probationaryMutex);

                m_probationaryServers.remove(serv);
            }
        }
    }


    bool NrpdConfig::AddServersFromMessage(pNrp_Header_Message msg)
    {
        sockaddr_storage stor;
        ServerRecord rec;

        if(msg == nullptr)
        {
            return false;
        }

        if(msg->msgType != ip4peers && msg->msgType != ip6peers)
        {
            return false;
        }

        if(msg->msgType == ip4peers)
        {
            pNrp_Message_Ip4Peer ip4Msg = (pNrp_Message_Ip4Peer) msg->content;

            sockaddr_in& v4addr = (sockaddr_in&) stor;
            stor.ss_family = AF_INET;

            for(int i = 0; i < msg->countOrSize; i++, ip4Msg++)
            {
                // Copy IP into sockaddr struct to test whether it is banned
                memcpy(&(v4addr.sin_addr), ip4Msg->ip, sizeof(v4addr.sin_addr));

                if(m_bannedServers->IsPresent(stor))
                {
                    // Server is banned, don't add to the list
                    continue;
                }

                // Create ServerRecord to test if the server already exists
                rec.Initialize();
                rec.ipv6 = false;
                memcpy(rec.host4, ip4Msg->ip, sizeof(rec.host4));
                rec.port = ip4Msg->port;
                rec.retryTime = m_clientRequestIntervalSeconds;

                if(m_activeServers.find(rec) != m_activeServers.end())
                {
                    // Server already in active servers list, don't add.
                    continue;
                }

                // Server is not banned or already added, add it to
                // probationary list.
                m_probationaryServers.push_back(rec);
            }
        }
        else // ip6 peers
        {
            pNrp_Message_Ip6Peer ip6Msg = (pNrp_Message_Ip6Peer) msg->content;

            sockaddr_in6& v6addr = (sockaddr_in6&) stor;
            stor.ss_family = AF_INET6;

            for(int i = 0; i < msg->countOrSize; i++, ip6Msg++)
            {
                // Copy IP address into sockaddr to test whether it is banned
                memcpy(&(v6addr.sin6_addr), ip6Msg->ip, sizeof(v6addr.sin6_addr));

                if(m_bannedServers->IsPresent(stor))
                {
                    // Server is banned. Continue with the next one.
                    continue;
                }

                // Create ServerRecord to test if this server was already
                // added to the client.
                rec.Initialize();
                rec.ipv6 = true;
                memcpy(rec.host6, ip6Msg->ip, sizeof(rec.host6));
                rec.port = ip6Msg->port;
                rec.retryTime = m_clientRequestIntervalSeconds;

                if(m_activeServers.find(rec) != m_activeServers.end())
                {
                    // Server already active in list; don't add it.
                    continue;
                }

                // Server is not banned or already added, add it to
                // probationary list
                m_probationaryServers.push_back(rec);
            }

        }

        return true;
    }
}
