#include <string>
#include <cstring>
#include "config.h"
#include "protocol.h"

using namespace std;

namespace nrpd
{

/// ServerRecord member functions ///

    constexpr bool ServerRecord::operator==(ServerRecord const& rhs)
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

    constexpr bool ServerRecord::operator<(ServerRecord const& rhs)
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


/// NrpdConfig member functions ///

    NrpdConfig::NrpdConfig()
    {
        m_port = 8080;
        m_configPath = "";
        m_enableServer = true;
        m_enableClient = true;
        m_entropyServerAddress = nullptr;
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
            for(ServerRecord& rec : m_activeServers)
            {
                if(rec.ipv6 == ipv6)
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
                return *m_activeIterator;
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
                return *m_activeIterator;
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
            // remove from probationary list
            if(serv.probationary == true)
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
                if(serv == *m_activeIterator)
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

                    m_activeServers.remove(serv);
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

            // TODO: don't add duplicates to the active server list
            {
                lock_guard<mutex> lock(m_activeMutex);
                // Add to the active server list under lock
                m_activeServers.push_back(serv);

                if(serv.ipv6)
                {
                    m_countIp6Servers += 1;
                }
                else
                {
                    m_countIp4Servers += 1;
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
}
