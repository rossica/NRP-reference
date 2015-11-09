#include "server.h"
#include "protocol.h"

#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/random.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>


using namespace std;

namespace nrpd
{
    NrpdServer::NrpdServer()
    {
    }

    NrpdServer::NrpdServer(shared_ptr<NrpdConfig> cfg) : m_config(cfg), m_state(notinitialized)
    {
    }

    NrpdServer::~NrpdServer()
    {
        m_state = stopping;

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

        // TODO: Get MTU from the interface
        m_mtu = 1500;

        // TODO: Make random device configurable
        if((m_randomfd = open("/dev/urandom", O_RDONLY)) < 0)
        {
            // TODO: add logging
            return errno;
        }


        m_state = initialized;
        return EXIT_SUCCESS;
    }


    int NrpdServer::CalculateMessageSize(int availableBytes, int messageSize, int& messageCount)
    {
        int responseSize = 0;

        // Enough room for a header?
        if(availableBytes < sizeof(Nrp_Header_Message))
        {
            // no room for a header
            return responseSize;
        }

        // Enough room for the requested message?
        responseSize = sizeof(Nrp_Header_Message) + (messageSize * messageCount);
        if(availableBytes >= responseSize)
        {
            return responseSize;
        }

        // Calculate as many messages will fit
        int availableMessageCount = (availableBytes - sizeof(Nrp_Header_Message)) / messageSize;
        if(availableMessageCount == 0)
        {
            // Can't fit any messages; bail.
            return availableMessageCount;
        }

        responseSize = sizeof(Nrp_Header_Message) + (messageSize * availableMessageCount);
        messageCount = availableMessageCount;
        return responseSize;
    }


    bool NrpdServer::ParseMessages(pNrp_Header_Request pkt, std::list<unique_ptr<unsigned char[]>>& msgs)
    {
        std::list<Nrp_Message_Reject> rejections;
        pNrp_Header_Message currentMsg;
        unique_ptr<unsigned char[]> tempMsgBuffer;
        unique_ptr<unsigned char[]> data;

        // Only send as much data as will fit in one packet. Client can request more later.
        int bytesRemaining = m_mtu - sizeof(udphdr);
        int dataSize;
        int responseSize;
        int msgCount;

        if(bytesRemaining <= sizeof(Nrp_Header_Packet))
        {
            return false;
        }

        if(pkt == nullptr)
        {
            return false;
        }

        currentMsg = pkt->messages;

        // Iterate through all messages
        while(currentMsg < EndOfPacket(pkt) && bytesRemaining > 0)
        {
            responseSize = 0;
            msgCount = 0;
            dataSize = 0;

            switch(currentMsg->msgType)
            {
            case ip4peers:
                // Parse request
                msgCount = min(m_config->activeServerCount(false), (int)currentMsg->countOrSize);

                if(msgCount == 0)
                {
                    // Client didn't specify, so give them as many as will fit
                    msgCount = m_config->activeServerCount(false);
                }

                // Calculate size of response
                responseSize = CalculateMessageSize(bytesRemaining, sizeof(Nrp_Message_Ip4Peer), msgCount);
                if(responseSize == 0)
                {
                    // Not enough room for this message
                    break;
                }

                // Allocate and generate response
                tempMsgBuffer = make_unique<unsigned char[]>(responseSize);

                data = m_config->GetServerList(false, msgCount, dataSize);

                GenerateResponsePeersMessage(ip4peers, msgCount, data.get(), dataSize, (pNrp_Header_Message) tempMsgBuffer.get());
                break;
            case ip6peers:
            case entropy:
                break;
            case pubkey:
            case secureentropy:
                // TODO: check if configured for pubkey
                // TODO: Does this create a static struct, or a new one each time?
                rejections.push_front({currentMsg->msgType, unsupported});
            default:
                break;
            }

            bytesRemaining -= responseSize;
            currentMsg = NextMessage(currentMsg);
            // BUGBUG: How to guarantee enough room for reject messages?
        }

        // check for any rejections and add them to the list of messages
        if(!rejections.empty())
        {
            // Allocate buffer for rejection messages
            (
               tempMsgBuffer = make_unique<unsigned char[]>(
                                  sizeof(Nrp_Header_Message) +
                                  (sizeof(Nrp_Message_Reject) * rejections.size())));

            // generate rejection
            if(tempMsgBuffer != nullptr)
            {
                pNrp_Message_Reject rejectMsg = GenerateRejectHeader(rejections.size(), (pNrp_Header_Message) tempMsgBuffer.get());
                for(Nrp_Message_Reject rej : rejections)
                {
                    rejectMsg = GenerateRejectMessage((nrpd_reject_reason) rej.reason, (nrpd_msg_type)rej.msgType, rejectMsg);
                }
            }
            // Put rejections first
            msgs.push_front(move(tempMsgBuffer));
        }

        return true;
    }

    int NrpdServer::ServerLoop()
    {
        if(m_state == initialized)
        {
            m_state = running;
        }
        else
        {
            return EXIT_FAILURE;
        }

        while(m_state == running)
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

            // parse messages in request

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

    void NrpdServer::ServerThread(shared_ptr<NrpdServer> server)
    {
        // TODO: Do something with the return value here
        server->ServerLoop();
    }
}
