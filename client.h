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
        ~NrpdClient();
        int ClientLoop();
        int InitializeClient();
    private:
        enum NrpdClientState
        {
            notinitialized,
            initialized,
            running,
            stopping,
            destroying
        };

        shared_ptr<NrpdConfig> m_config;
        int m_socketfd;
        int m_randomfd;

        NrpdClientState m_state;

        // Construct a request packet in buffer, using server to determine
        // which message types are supported.
        bool ConstructRequest(ServerRecord const& server, int bufSize, unsigned char* buffer);

        // Parse entropy response message and write obtained entropy to system
        // PRNG to increase system entropy
        bool ParseEntropyMessage(pNrp_Header_Message msg);

        // Parse reject message, and disable rejected capabilities in server
        bool ParseRejectMessage(ServerRecord& server, pNrp_Header_Message msg);

        // Parse response message received from server
        bool ParseResponse(ServerRecord& server, int bufSize, unsigned char* buffer);

    };
}
