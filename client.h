#include "config.h"
#include <memory>

using namespace std;

namespace nrpd
{
    class NrpdClient
    {
    public:
        NrpdClient();
        NrpdClient(shared_ptr<NrpdConfig>);
        int ClientLoop();
        int InitializeClient();
    private:
        shared_ptr<NrpdConfig> m_config;
        int m_socketfd;
        
    };
}
