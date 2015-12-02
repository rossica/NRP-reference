#include "config.h"
#include "protocol.h"
#include <memory>
#include <list>

#pragma once

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
        static void ServerThread(shared_ptr<NrpdServer> server);
    private:
        enum NrpdServerState
        {
            notinitialized,
            initialized,
            running,
            stopping
        };

        shared_ptr<NrpdConfig> m_config;
        NrpdServerState m_state;
        int m_socketfd;
        int m_randomfd;
        int m_mtu;

        // Parse incoming request messages from a client and generate responses
        // as appropriate.
        bool ParseMessages(pNrp_Header_Request pkt, int& outMessageLength, std::list<unique_ptr<unsigned char[]>>& msgs);

        // Calculate maximal byte size for a message, given remaining space
        // in the response packet.
        // Returns the size of the message with header.
        // Returns 0 if there's not enough space for the message.
        // messageCount will be updated with the count of messages,
        // of size messageSize, that fit in availableBytes.
        unsigned int CalculateMessageSize(unsigned int availableBytes, int messageSize, int& messageCount);

        // Calculate a running total of bytes available based on the number of
        // rejection messages already generated.
        int CalculateRemainingBytes(int availableBytes, int rejCount);

        // Parse a peers request message and generate a peers response
        unique_ptr<unsigned char[]> GeneratePeersResponse(nrpd_msg_type type, int msgCount, int availableBytes, int& outResponseSize);

        // Parse an entropy request message and generate an entropy response
        unique_ptr<unsigned char[]> GenerateEntropyResponse(int size, int bytesRemaining, int& outResponseSize);

    };
}
