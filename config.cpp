#include <string>
#include "config.h"

using namespace std;

namespace nrpd
{
    NrpdConfig::NrpdConfig()
    {
        m_port = 8080;
        m_configPath = "";
    }

    NrpdConfig::NrpdConfig(string* path)
    {
        m_port = 8080;
        m_configPath = *path;
    }
}
