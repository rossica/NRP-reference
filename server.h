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

        bool ParseMessages(pNrp_Header_Request pkt, std::list<unique_ptr<unsigned char[]>>& msgs);


        // Calculate maximal byte size for a message, given remaining space
        // in the response packet.
        // Returns 0 if there's not enough space for the message.
        // messageCount will be updated with the numbers of messages that fit
        int CalculateMessageSize(int availableBytes, int messageSize, int& messageCount);

    };
}
