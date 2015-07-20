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
        unsigned short port();
        bool enableServer();
        bool enableClient();
        bool obfuscateResponse();
    private: 
        string m_configPath;
        unsigned short m_port;
        bool m_enableServer;
        bool m_enableClient;
        //list<char*> m_serverAddresses;
        char* m_serverAddress;
        bool m_obfuscateResponse;
        
        
    };
}
