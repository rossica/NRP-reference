#include "client.h"
#include "protocol.h"
#include <memory>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

using namespace std;

namespace nrpd
{
    NrpdClient::NrpdClient()
    {
    }

    NrpdClient::NrpdClient(shared_ptr<NrpdConfig> conf) : m_config(conf)
    {
    }

    int NrpdClient::InitializeClient()
    {
        timeval timeout = { (__time_t) (m_config->receiveTimeout()), 0};

        m_socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(m_socketfd < 0)
        {
            // TODO: log error here
            return errno;
        }

        // Set send timeout
        if(setsockopt(m_socketfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeval)) < 0)
        {
            // TODO: log error
            return errno;
        }

        return 0;
    }

    int NrpdClient::ClientLoop()
    {

        ssize_t count = 0;
        unsigned char buffer[MAX_RESPONSE_MESSAGE_SIZE];
        sockaddr_storage sendServerAddr, receiveServerAddr;
        socklen_t sendServerAddrLen = sizeof(sendServerAddr), receiveServerAddrLen = 0;

        while(1)
        {
            // TODO: create sendServerAddr from config
            // Create request packet
            // send request packet to configured server
            if( (count = sendto(m_socketfd, buffer, MAX_REQUEST_MESSAGE_SIZE, 0, (sockaddr*) &sendServerAddr, sendServerAddrLen)) < 0)
            {
                // TODO: log error
                return errno;
            }
            // receive response
            count = recvfrom(m_socketfd, buffer, sizeof(buffer), 0, (sockaddr*) &receiveServerAddr, &receiveServerAddrLen);
            // validate send and receive server addresses are the same
            // if not the same, listen again.
            if(count < 0)
            {
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // timed out waiting for response
                    // increment server fail count
                    // if server fail count is max fail count, remove from list.
                }
                else
                {
                    // TODO: log error
                    return errno;
                }
            }

            // if configured, test received entropy for randomness
            //// if it doesn't pass test, discard and continue
            //// if paranoid, increment server fail count and remove from list if it reached max failures.

            // write received entropy to system RNG.

            // sleep for configured interval.
            sleep(m_config->clientRequestInterval());
        }
        
        return 0;
    }
}
