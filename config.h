#include <string>
#include <netinet/in.h>

#pragma once

using namespace std;

namespace nrpd
{
    class NrpdConfig
    {
    public:
        NrpdConfig();
        NrpdConfig(string*);
        short port();
    private: 
        string m_configPath;
        short m_port;
        bool m_enableServer;
        bool m_enableClient;
        //list<sockaddr_in> m_serverAddresses;
        char* m_serverAddress;
        
        
    };
}
