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
        m_entropyServerAddress = nullptr;
        m_obfuscateResponse = false;
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

    bool NrpdConfig::obfuscateResponse()
    {
        return m_obfuscateResponse;
    }

    int NrpdConfig::clientRequestInterval()
    {
        return m_clientRequestIntervalSeconds;
    }

    int NrpdConfig::receiveTimeout()
    {
        return m_clientReceiveTimeout;
    } 
}
