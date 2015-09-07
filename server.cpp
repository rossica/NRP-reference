#include "server.h"
#include "protocol.h"

#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/random.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

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

    NrpdServer::~NrpdServer()
    {
        if(m_socketfd)
        {
            close(m_socketfd);
        }

        if(m_randomfd > 0)
        {
            close(m_randomfd);
        }
    }

    int NrpdServer::InitializeServer()
    {

        sockaddr_in host_addr;

        // TODO: make this configurable
        m_socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(m_socketfd < 0)
        {
            // insert error code here
            return errno;
        }
        memset(&host_addr, 0, sizeof(sockaddr_in));

        host_addr.sin_port = htons(m_config->port());
        // TODO: get this from configuration
        host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        host_addr.sin_family = AF_INET;


        if(bind(m_socketfd, (sockaddr*) &host_addr, sizeof(sockaddr_in)) < 0)
        {
            // TODO: add logging
            return errno;
        }

        if((m_randomfd = open("/dev/urandom", O_RDONLY)) < 0)
        {
            // TODO: add logging
            return errno;
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

            unsigned char buffer[MAX_RESPONSE_MESSAGE_SIZE];
            unsigned char entropy[MAX_BYTE];


            memset(buffer, 0, sizeof(buffer));

            // Wait for connection
            if( (count = recvfrom(m_socketfd, buffer, MAX_REQUEST_MESSAGE_SIZE, 0, (sockaddr*) &srcAddr, &srcAddrLen)) < 0)
            {
                // TODO: log some error
                continue;
            }

            pNrp_Header_Request req = (pNrp_Header_Request) buffer;


            // validate packet
            if(!ValidateRequestPacket(req))
            {
                // ignore malformed packets
                printf("malformed packet\n");
                continue;
            }

            // check if client has requested recently

            // check available entropy

            // if not enough entropy: generate reject packet

            //if(syscall(SYS_getrandom,entropy, req->requestedEntropy, 0) < 0)
            if(read(m_randomfd, entropy, 8/* TODO: replace with parsed value */) < 0)
            {
                // TODO: log an error
                return errno;
            }

            count = 8 /* TODO: replace with parsed value */ + RESPONSE_HEADER_SIZE;
            if(!GenerateResponseEntropyMessage(8/* TODO: replace with parsed value */, entropy, sizeof(buffer), (pNrp_Header_Message) buffer))
            {
                return -1;
            }
            printf("generating response\n");

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
