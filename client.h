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
        static void ClientThread(shared_ptr<NrpdClient> client);
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
        int m_socketfd4;
        int m_socketfd6;
        int m_randomfd;

        NrpdClientState m_state;

        // Construct a request packet in buffer, using server to determine
        // which message types are supported.
        //
        // Client determines the order of preference for responses, by requesting
        // messages in the order of preference. Ordering matters because the server
        // attempts to ensure important messages fit in a single response packet
        // while less important messages are ignored.
        // The recommended order is:
        //
        // (pubkey/secureentropy) > entropy > (ip4peers,ip6peers)
        //
        // Reject messages will always be first in response packets, if the server
        // sends any. (clients cannot request reject messages, hence their omission
        // from the above list of precedence)
        //
        // If the server rejects any of the message types, they are removed from
        // the order of preference, and wont be requested in subsequent requests
        // made to that server. Entropy messages cannot be rejected; ever server
        // must support that message at a minimum.
        bool ConstructRequest(ServerRecord const& server, unsigned int bufSize, unsigned char* buffer, int& outPktSize);

        // Call Connect() on the address supplied by server.
        bool ConnectServer(ServerRecord const& server);

        // Zero out part of the entropy, in place, so an eavesdropper
        // doesn't know which entropy was consumed.
        bool ScrambleEntropy(size_t bufSize, unsigned char* entropy);

        // Parse entropy response message and write obtained entropy to system
        // PRNG to increase system entropy
        bool ConsumeEntropy(size_t bufSize, unsigned char* entropy);

        // Parse reject message, and disable rejected capabilities in server
        bool ParseRejectMessage(ServerRecord& server, pNrp_Header_Message msg);

        // Parse response message received from server
        bool ParseResponse(ServerRecord& server, int bufSize, unsigned char* buffer);

    };
}
