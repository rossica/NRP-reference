#include "server.h"
#include "protocol.h"

#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <memory>

using namespace std;

namespace nrpd
{
    NrpdServer::NrpdServer()
    {
    }

    NrpdServer::NrpdServer(shared_ptr<NrpdConfig> cfg) : m_config(cfg)
    {
    }

    int NrpdServer::InitializeServer()
    {

        int bindReturn;
        sockaddr_in host_addr;

        // TODO: make this configurable
        m_socketfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(m_socketfd < 0)
        {
            // insert error code here
            return m_socketfd;
        }
        memset(&host_addr, 0, sizeof(sockaddr_in));

        host_addr.sin_port = htons(m_config->port());
        // TODO: get this from configuration
        host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        host_addr.sin_family = AF_INET;


        if((bindReturn = bind(m_socketfd, (sockaddr*) &host_addr, sizeof(sockaddr_in))) < 0)
        {
            // TODO: add logging
            return bindReturn;
        }

        return EXIT_SUCCESS;
    }

    int NrpdServer::ServerLoop()
    {
        while(1)
        {
            sockaddr_storage srcAddr;
            socklen_t srcAddrLen = sizeof(srcAddr);
            ssize_t count;
            char buffer[65507];

            memset(buffer, 0, sizeof(buffer));

            // Wait for connection
            if( (count = recvfrom(m_socketfd, buffer, MAX_REQUEST_MESSAGE_SIZE, 0, (sockaddr*) &srcAddr, &srcAddrLen)) < 0)
            {
                // TODO: log some error
                continue;
            }

            // probably really dumb
            printf("%d %s\n", MAX_REQUEST_MESSAGE_SIZE, buffer);
            pNrp_Header_Request req = (pNrp_Header_Request) buffer;
            printf("%.02x %.02x\n", req->/*nrpHeader.*/length, /*req->nrpHeader.msgType,*/ req->requestedEntropy);


            // validate packet
            // if invalid: generate reject packet
            // check available entropy
            // if enough entropy: generate response packet
            // if not enough entropy: generate reject packet
            // send generated packet
            if( (count = sendto(m_socketfd, buffer, count, 0, (sockaddr*) &srcAddr, srcAddrLen)) < 0)
            {
                // TODO: log some error
                continue;
            }
        }

        return EXIT_SUCCESS;
    }
}
