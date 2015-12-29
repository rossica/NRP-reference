#include <string>
#include <netinet/in.h>
#include <list>
#include <set>
#include <memory>
#include <mutex>
#include <atomic>

#include "protocol.h"
#include "clientmrucache.h"

#pragma once

using namespace std;

namespace nrpd
{
    class ServerRecord
    {
        public:
        union // In network byte order
        {
            unsigned char host6[16];
            unsigned char host4[4];
        };
        unsigned short port; // This is in network byte order
        int failureCount;
        time_t lastaccessTime;
        int retryTime;
        struct
        {
            unsigned char ipv6 : 1; // is server address ipv4 or ipv6?
            unsigned char probationary : 1; // is server proven good?
            unsigned char ip4Peers : 1; // does server support ipv4 peers?
            unsigned char ip6Peers : 1; // does server support ipv6 peers?
            unsigned char pubKey : 1; // does server support public key?
        };

        constexpr bool operator==(ServerRecord const& rhs);

        // Defines ip4 servers as less-than ip6 servers
        constexpr bool operator<(ServerRecord const& rhs);
    };

    class NrpdConfig
    {
    public:
        NrpdConfig();
        NrpdConfig(string*);

        unsigned short port();
        int defaultEntropyResponse();
        bool enableServer();
        bool enableClient();
        bool enablePeersResponse(nrpd_msg_type type);
        bool daemonize();
        int clientRequestInterval();
        int receiveTimeout();
        char* entropyServerAddress();
        int ActiveServerCount(nrpd_msg_type type);
        unique_ptr<unsigned char[]> GetServerList(nrpd_msg_type type, int count, int& outSize);

        // Alternates between returning servers on the probationary and active
        // server lists.
        // Callers MUST indicate the failure or success of the server by
        // calling IncrementServerFailCount or MarkServerSuccessful after the
        // server responds or timesout.
        ServerRecord& GetNextServer();

        // Adds servers to probationary list until they respond, or fail 5 times in a row
        bool AddServersFromMessage(pNrp_Header_Message msg);

        // Increments the fail count and may remove the server if it exceeds
        // the maximum fail count.
        // May delete the server pointed to by serv. Do not use serv after
        // calling this function.
        void IncrementServerFailCount(ServerRecord& serv);

        // Resets the fail count to 0, and may move the server to the active
        // server list, if it's on the probationary list.
        // May delete the server pointed to by serv. Do not use serv after
        // calling this function.
        void MarkServerSuccessful(ServerRecord& serv);

    private:
        string m_configPath;
        unsigned short m_port;
        bool m_enableServer;
        bool m_enableClient;
        shared_ptr<ClientMRUCache> m_bannedServers;
        list<ServerRecord> m_configuredServers;
        list<ServerRecord> m_activeServers;
        list<ServerRecord> m_probationaryServers;
        list<ServerRecord>::iterator m_activeIterator;
        list<ServerRecord>::iterator m_probationaryIterator;
        atomic<int> m_countIp6Servers;
        atomic<int> m_countIp4Servers;
        bool m_prevReturnedProbationary;
        char* m_entropyServerAddress;
        char* m_randomDevice;
        bool m_forkDaemon;
        bool m_enableIp4Peers;
        bool m_enableIp6Peers;
        mutex m_activeMutex;
        mutex m_probationaryMutex;

        int m_clientRequestIntervalSeconds;
        int m_clientReceiveTimeout;
        int m_defaultEntropyResponse;


    };
}
