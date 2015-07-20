#include <string>
#include "config.h"

using namespace std;

namespace nrpd
{
    NrpdConfig::NrpdConfig()
    {
        m_port = 8080;
        m_configPath = "";
        m_enableServer = true;
        m_enableClient = true;
        m_serverAddress = nullptr;
        m_obfuscateResponse = false;
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

    bool NrpdConfig::obfuscateResponse()
    {
        return m_obfuscateResponse;
    } 
}
