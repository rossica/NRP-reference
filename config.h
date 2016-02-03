#include <string>
#include <netinet/in.h>
#include <list>
#include <set>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <initializer_list>

#include "protocol.h"
#include "mrucache.h"

#pragma once



namespace nrpd
{
    class ServerRecord
    {
        public:
        ServerRecord() = default;

        ServerRecord(const unsigned char* ip, bool isIPv6, unsigned short port);

        // Convenience constructor, for testing.
        ServerRecord(initializer_list<unsigned char> l, unsigned short port);

        ServerRecord(pNrp_Message_Ip4Peer peer);

        ServerRecord(pNrp_Message_Ip6Peer peer);

        union // In network byte order
        {
            unsigned char host6[16];
            unsigned char host4[4];
        };
        unsigned short port; // This is in network byte order
        int failureCount;
        chrono::steady_clock::time_point lastaccessTime;
        chrono::seconds retryTime; // how many seconds since lastaccessTime to wait

        struct
        {
            unsigned short ipv6 : 1; // is server address ipv4 or ipv6?
            unsigned short probationary : 1; // is server proven good?
            unsigned short ip4Peers : 1; // does server support ipv4 peers?
            unsigned short ip6Peers : 1; // does server support ipv6 peers?
            unsigned short pubKey : 1; // does server support public key?
            unsigned short shuttingdown : 1; // Server sent shutdown message.
            unsigned short reserved : 10; // reserved for future flags.
        };


        // Initializes the ServerRecord with all support enabled, and all other
        // fields set to zeroes.
        void Initialize();

        // Only compares ip addresses
        bool operator==(ServerRecord const& rhs) const;

        // Defines ip4 servers as less-than ip6 servers
        bool operator<(ServerRecord const& rhs) const;
    };
}

// Note: this should be in stdhelper.h, but it would introduce a circular-
// dependency between stdhelper.h and config.h, so instead it's defined here.
namespace std
{
    template<>
    struct hash<nrpd::ServerRecord>
    {
        typedef nrpd::ServerRecord argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& s) const
        {
            result_type result = 0ull;
            // Only hashes the address, no other fields
            if(s.ipv6)
            {
                for(unsigned int i = 0; i < sizeof(s.host6); i++)
                {
                    result = result ^ (std::hash<unsigned char>()(s.host6[i]) << i);
                }
            }
            else
            {
                for(unsigned int i = 0; i < sizeof(s.host4); i++)
                {
                    result = result ^ (std::hash<unsigned char>()(s.host4[i]) << i);
                }
            }

            return result;
        }
    };
}

using namespace std;

namespace nrpd
{

    class NrpdConfig
    {
    public:
        NrpdConfig();
        NrpdConfig(string*);

        unsigned short serverPort();
        int defaultEntropySize();
        bool enableServer();
        bool enableClient();
        bool enablePeersResponse(nrpd_msg_type type);
        bool enableClientIp4();
        bool enableClientIp6();
        bool enableServerIp4();
        bool enableServerIp6();
        bool daemonize();
        int clientRequestInterval();
        int receiveTimeout();
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
        shared_ptr<MruCache<ServerRecord>> m_bannedServers;
        list<ServerRecord> m_configuredServers;
        set<ServerRecord> m_activeServers;
        list<ServerRecord> m_probationaryServers;
        set<ServerRecord>::iterator m_activeIterator;
        list<ServerRecord>::iterator m_probationaryIterator;
        atomic<int> m_countIp6Servers;
        atomic<int> m_countIp4Servers;
        bool m_prevReturnedProbationary;
        string m_randomDevice;
        bool m_forkDaemon;
        bool m_enableIp4Peers;
        bool m_enableIp6Peers;
        mutex m_activeMutex;
        mutex m_probationaryMutex;
        bool m_clientEnableIp4;
        bool m_clientEnableIp6;
        bool m_serverEnableIp4;
        bool m_serverEnableIp6;

        int m_clientRequestIntervalSeconds;
        int m_clientReceiveTimeout;
        int m_defaultEntropySize;


    };
}
