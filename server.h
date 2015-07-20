#include "config.h"
#include <memory>

using namespace std;

namespace nrpd
{
    class NrpdServer
    {
    public:
        NrpdServer();
        NrpdServer(shared_ptr<NrpdConfig> cfg);
        ~NrpdServer();
        int InitializeServer();
        int ServerLoop();
    private:
        shared_ptr<NrpdConfig> m_config;
        int m_socketfd;
        int m_randomfd;
        int m_randomMinfd;
        int m_randomAvailfd;

    };
}
