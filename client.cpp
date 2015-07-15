#include "client.h"
#include <memory>

using namespace std;

namespace nrpd
{
    NrpdClient::NrpdClient()
    {
    }

    NrpdClient::NrpdClient(NrpdConfig* conf) : m_config(conf)
    {
    }
}
