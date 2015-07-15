#include "config.h"
#include <memory>

using namespace std;

namespace nrpd
{
    class NrpServer
    {
    public:
        NrpServer();
    private:
        shared_ptr<NrpdConfig> m_config;

    };
}
