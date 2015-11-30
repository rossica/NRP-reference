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


    unsigned int NrpdServer::CalculateMessageSize(unsigned int availableBytes, int messageSize, int& messageCount)
    {
        unsigned int responseSize = 0;

        // Enough room for a header?
        if(availableBytes < NRP_MESSAGE_HEADER_SIZE)
        {
            // no room for a header
            return responseSize;
        }

        // Enough room for the requested message?
        responseSize = NRP_MESSAGE_HEADER_SIZE + (messageSize * messageCount);
        if(availableBytes >= responseSize)
        {
            return responseSize;
        }

        // Calculate as many messages will fit
        int availableMessageCount = (availableBytes - NRP_MESSAGE_HEADER_SIZE) / messageSize;
        if(availableMessageCount == 0)
        {
            // Can't fit any messages; bail.
            return availableMessageCount;
        }

        responseSize = NRP_MESSAGE_HEADER_SIZE + (messageSize * availableMessageCount);
        messageCount = availableMessageCount;
        return responseSize;
    }


    int NrpdServer::CalculateRemainingBytes(int availableBytes, int rejCount)
    {
        if(rejCount > 0)
        {
            return availableBytes - (NRP_MESSAGE_HEADER_SIZE + (rejCount * sizeof(Nrp_Message_Reject)));
        }
        else
        {
            return availableBytes;
        }
    }

    unique_ptr<unsigned char[]> NrpdServer::GeneratePeersResponse(nrpd_msg_type type, int msgCount, int availableBytes, int& outResponseSize)
    {
        int size = 0;
        int dataSize = 0;
        unique_ptr<unsigned char[]> tempMsgBuffer;
        unique_ptr<unsigned char[]> data;

        if(type == ip6peers)
        {
            size = sizeof(Nrp_Message_Ip6Peer);
        }
        else
        {
            size = sizeof(Nrp_Message_Ip4Peer);
        }

        outResponseSize = 0;

        if(msgCount <= 0)
        {
            // Client didn't specify, so give them as many as will fit
            msgCount = m_config->ActiveServerCount(type);
        }
        else
        {
            msgCount = min(m_config->ActiveServerCount(type), msgCount);
        }

        if(msgCount == 0)
        {
            // No servers of requested type, fail
            // Note: this should be caught by GenenerateResponsePeersMessage, but
            // failing here saves the work of a memory allocation/deallocation
            return nullptr;
        }

        // Calculate size of response
        outResponseSize = CalculateMessageSize(availableBytes, size, msgCount);
        if(outResponseSize == 0)
        {
            // Not enough room for this message
            return nullptr;
        }

        // Allocate and generate response
        tempMsgBuffer = make_unique<unsigned char[]>(outResponseSize);

        data = m_config->GetServerList(type, msgCount, dataSize);

        if(GenerateResponsePeersMessage(type, msgCount, data.get(), outResponseSize, (pNrp_Header_Message) tempMsgBuffer.get()))
        {
            return tempMsgBuffer;
        }
        else
        {
            return nullptr;
        }
    }

    unique_ptr<unsigned char[]> NrpdServer::GenerateEntropyResponse(int size, int bytesRemaining, int& outResponseSize)
    {
        int actualSize = 0;
        int dataSize = 0;
        int readSize = 0;
        unique_ptr<unsigned char[]> tempMsgBuffer;
        unique_ptr<unsigned char[]> data;

        if(size <= 0)
        {
            size = m_config->defaultEntropyResponse();
        }

        actualSize = CalculateMessageSize(bytesRemaining, 1, size);
        outResponseSize = 0;

        if(actualSize == 0)
        {
            // Not enough space for the message at all
            return nullptr;
        }

        dataSize = actualSize - NRP_MESSAGE_HEADER_SIZE;

        data = make_unique<unsigned char []>(dataSize);

        readSize = read(m_randomfd, data.get(), dataSize);

        if(readSize < 0)
        {
            // TODO Log errno here
            return nullptr;
        }
        else if(readSize == 0)
        {
            // TODO log EOF here
            return nullptr;
        }
        else if(readSize < dataSize)
        {
            // If the read was less than requested, recalculate the message size
            actualSize = NRP_MESSAGE_HEADER_SIZE + readSize;
        }

        tempMsgBuffer = make_unique<unsigned char[]>(actualSize);

        if(GenerateResponseEntropyMessage(readSize, data.get(), actualSize, (pNrp_Header_Message) tempMsgBuffer.get()))
        {
            outResponseSize = actualSize;
            return tempMsgBuffer;
        }
        else
        {
            return nullptr;
        }
    }


    bool NrpdServer::ParseMessages(pNrp_Header_Request pkt, std::list<unique_ptr<unsigned char[]>>& msgs)
    {
        std::list<Nrp_Message_Reject> rejections;
        pNrp_Header_Message currentMsg;
        unique_ptr<unsigned char[]> tempMsgBuffer;

        // Only send as much data as will fit in one packet. Client can request more later.
        int bytesRemaining = m_mtu - sizeof(udphdr);
        int responseSize;
        int msgCount;

        if(bytesRemaining <= NRP_PACKET_HEADER_SIZE)
        {
            return false;
        }

        if(pkt == nullptr)
        {
            return false;
        }

        bytesRemaining -= NRP_PACKET_HEADER_SIZE;

        currentMsg = pkt->messages;

        // Iterate through all messages
        while(currentMsg < EndOfPacket(pkt) && CalculateRemainingBytes(bytesRemaining, rejections.size()) > 0)
        {
            responseSize = 0;

            switch(currentMsg->msgType)
            {
            case ip4peers:
            case ip6peers:
                // TODO: Check config for whether this is enabled
                tempMsgBuffer = GeneratePeersResponse((nrpd_msg_type) currentMsg->msgType, currentMsg->countOrSize, CalculateRemainingBytes(bytesRemaining, rejections.size()), responseSize);
                break;
            case entropy:
                tempMsgBuffer = GenerateEntropyResponse(currentMsg->countOrSize, CalculateRemainingBytes(bytesRemaining, rejections.size()), responseSize);
                break;
            case pubkey:
            case secureentropy:
                // TODO: check if configured for pubkey
                rejections.push_back({currentMsg->msgType, unsupported});
            default:
                // Invalid message; should have never gotten this far
                break;
            }

            if(tempMsgBuffer != nullptr)
            {
                msgs.push_back(move(tempMsgBuffer));
                bytesRemaining -= responseSize;
            }

            currentMsg = NextMessage(currentMsg);
        }

        // check for any rejections and add them to the list of messages
        if(!rejections.empty())
        {
            msgCount = rejections.size();

            // It's possible there's not enough room for rejections
            // Calculate if we can fit the entire rejection message
            // TODO: re-design this to guarantee room for rejection messages
            // possibly pre-calculate whether rejections are needed.
            responseSize = CalculateMessageSize(bytesRemaining, sizeof(Nrp_Message_Reject), msgCount);
            if(responseSize > 0)
            {
                // Allocate buffer for rejection messages
                tempMsgBuffer = make_unique<unsigned char[]>(responseSize);

                // generate rejection
                if(tempMsgBuffer != nullptr)
                {
                    pNrp_Message_Reject rejectMsg = GenerateRejectHeader(msgCount, (pNrp_Header_Message) tempMsgBuffer.get());
                    for(Nrp_Message_Reject& rej : rejections)
                    {
                        if((void*)rejectMsg >= NextMessage(tempMsgBuffer.get(), NRP_MESSAGE_HEADER_SIZE + (msgCount * sizeof(Nrp_Message_Reject))))
                        {
                            // Gross hack to prevent a buffer overflow of tempMsgBuffer when
                            // msgCount is less than rejections.size()
                            break;
                        }

                        rejectMsg = GenerateRejectMessage((nrpd_reject_reason) rej.reason, (nrpd_msg_type)rej.msgType, rejectMsg);
                    }
                }
                // Put rejections first
                msgs.push_front(move(tempMsgBuffer));
            }
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
