#include "config.h"
#include <memory>

using namespace std;

namespace nrpd
{
    class NrpdClient
    {
    public:
        NrpdClient();
        NrpdClient(NrpdConfig*);
    private:
        shared_ptr<NrpdConfig> m_config;
    };
}
