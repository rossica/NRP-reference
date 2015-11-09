#include <string>
#include <netinet/in.h>
#include <list>
#include <memory>

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
        bool ipv6;
        unsigned short port; // This is in network byte order
        int failureCount;
        long long lastaccessTime;
    };

    class NrpdConfig
    {
    public:
        NrpdConfig();
        NrpdConfig(string*);

        unsigned short port();
        bool enableServer();
        bool enableClient();
        bool daemonize();
        int clientRequestInterval();
        int receiveTimeout();
        char* entropyServerAddress();
        int activeServerCount(bool ipv6);
        unique_ptr<unsigned char[]> GetServerList(bool ipv6, int count, int& out_size);

    private:
        string m_configPath;
        unsigned short m_port;
        bool m_enableServer;
        bool m_enableClient;
        list<ServerRecord> m_configuredServers;
        list<ServerRecord> m_activeServers;
        list<ServerRecord> m_probationaryServers;
        char* m_entropyServerAddress;
        char* m_randomDevice;
        bool m_forkDaemon;

        int m_clientRequestIntervalSeconds;
        int m_clientReceiveTimeout;


    };
}
