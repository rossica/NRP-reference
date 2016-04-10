#include "server.h"
#include "protocol.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


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
        //TODO: Eventually make this private.
    }

    NrpdServer::NrpdServer(shared_ptr<NrpdConfig> cfg) : m_config(cfg), m_state(notinitialized)
    {
    }

    NrpdServer::~NrpdServer()
    {
        m_state = destroying;

        if(m_socketfd > 0)
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
        sockaddr_storage hostStor = {0};
        int hostSize = 0;

        // IPv4 only
        if(m_config->enableServerIp4() && !m_config->enableServerIp6())
        {
            sockaddr_in& addr = (sockaddr_in&) hostStor;

            m_socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if(m_socketfd < 0)
            {
                // insert error code here
                return errno;
            }

            addr.sin_port = htons(m_config->serverPort());
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_family = AF_INET;

            hostSize = sizeof(addr);
        }
        // IPv6 or Dual-stack
        else if(m_config->enableServerIp6())
        {
            sockaddr_in6& addr = (sockaddr_in6&) hostStor;
            int v6only = 0;

            m_socketfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            if(m_socketfd < 0)
            {
                // insert error code here
                return errno;
            }

            addr.sin6_port = htons(m_config->serverPort());
            addr.sin6_addr = in6addr_any;
            addr.sin6_family = AF_INET6;

            hostSize = sizeof(addr);

            // Dual-Stack
            if(m_config->enableServerIp4())
            {
                if(setsockopt(m_socketfd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0)
                {
                    // TODO: log error
                    return errno;
                }
            }
        }
        else
        {
            // this shouldn't happen.
            // TODO: print error and exit.
        }

        if(bind(m_socketfd, (sockaddr*) &hostStor, hostSize) < 0)
        {
            // TODO: add logging
            return errno;
        }

        // TODO: Get MTU from the interface
        m_mtu = MAX_IP6_PACKET_SIZE;

        // TODO: Make random device configurable
        if((m_randomfd = open("/dev/urandom", O_RDONLY)) < 0)
        {
            // TODO: add logging
            return errno;
        }

        // Create recent clients hashmap
        m_recentClients = make_shared<MruCache<sockaddr_storage>>(CLIENT_MIN_RETRY_SECONDS);

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
            // failing here saves the work of allocating/deallocating tempMsgBuffer
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

        if(tempMsgBuffer == nullptr)
        {
            return nullptr;
        }

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
            size = m_config->defaultEntropySize();
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

        if(data == nullptr)
        {
            return nullptr;
        }

        // Read entropy from random device
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
            // Log this; shouldn't happen in practice
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


    bool NrpdServer::ParseMessages(pNrp_Header_Request pkt, int& outMessageLength, std::list<unique_ptr<unsigned char[]>>& msgs)
    {
        std::list<Nrp_Message_Reject> rejections;
        pNrp_Header_Message currentMsg;
        unique_ptr<unsigned char[]> tempMsgBuffer;

        // Only send as much data as will fit in one packet. Client can request more later.
        int bytesRemaining = m_mtu;
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

        outMessageLength = 0;

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
                if(m_config->enablePeersResponse((nrpd_msg_type) currentMsg->msgType))
                {
                    tempMsgBuffer = GeneratePeersResponse((nrpd_msg_type) currentMsg->msgType, currentMsg->countOrSize, CalculateRemainingBytes(bytesRemaining, rejections.size()), responseSize);
                }
                else
                {
                    rejections.push_back({currentMsg->msgType, unsupported});
                }
                break;
            case entropy:
                tempMsgBuffer = GenerateEntropyResponse(currentMsg->countOrSize, CalculateRemainingBytes(bytesRemaining, rejections.size()), responseSize);
                break;
            case certchain:
            case signkey:
            case encryptionkey:
            case secureentropy:
                // TODO: check if configured for signcert
                rejections.push_back({currentMsg->msgType, unsupported});
                break;
            default:
                // Unknown message type; reject
                rejections.push_back({currentMsg->msgType, unsupported});
                break;
            }

            if(tempMsgBuffer != nullptr)
            {
                msgs.push_back(move(tempMsgBuffer));
                bytesRemaining -= responseSize;
                outMessageLength += responseSize;
            }

            currentMsg = NextMessage(currentMsg);
        }

        // check for any rejections and add them to the list of messages
        if(!rejections.empty())
        {
            msgCount = rejections.size();

            // Calculate if we can fit the entire rejection message
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
                            // This should never occur in practice.
                            // TODO: log if this happens. Shouldn't ever happen
                            break;
                        }

                        rejectMsg = GenerateRejectMessage((nrpd_reject_reason) rej.reason, (nrpd_msg_type)rej.msgType, rejectMsg);
                    }

                    // Put rejections first
                    msgs.push_front(move(tempMsgBuffer));
                    outMessageLength += responseSize;
                }
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
            int count = 0;
            int messageLength;
            // TODO: make sure this is cleared every iteration, even on failure
            std::list<unique_ptr<unsigned char[]>> msgs;
            pNrp_Header_Message msg;

            unsigned char buffer[MAX_RESPONSE_MESSAGE_SIZE];


            memset(buffer, 0, sizeof(buffer));

            // Wait for connection
            if( (count = recvfrom(m_socketfd, buffer, MAX_REQUEST_MESSAGE_SIZE, 0, (sockaddr*) &srcAddr, &srcAddrLen)) < 0)
            {
                // TODO: log some error
                continue;
            }

            pNrp_Header_Request req = (pNrp_Header_Request) buffer;

            NrpdLog::LogString("Server: packet received");


            // validate packet
            if(!ValidateRequestPacket(req))
            {
                // ignore malformed packets
                NrpdLog::LogString("Server: packet failed validation");
                continue;
            }

            // check if client has requested recently
            if(m_recentClients->IsPresentAdd(srcAddr))
            {
                // Ignore this client. They've talked to us too recently
                NrpdLog::LogString("Server: Client seen too recently. Ignoring");
                continue;
            }

            // Set the "MTU" based on the IP protocol of the client.
            // This controls the number and size of messages in the response.
            if(IsAddressIp4(srcAddr))
            {
                m_mtu = MAX_IP4_PACKET_SIZE;
            }
            else
            {
                m_mtu = MAX_IP6_PACKET_SIZE;
            }

            // parse messages in request
            if(!ParseMessages(req, messageLength, msgs))
            {
                // TODO: log error
                NrpdLog::LogString("Server: failed to parse client request");
                continue;
            }

            // Add the packet header to the length.
            messageLength += sizeof(Nrp_Header_Packet);

            assert(messageLength <= m_mtu);

            // generate packet header
            msg = GeneratePacketHeader(messageLength, response, msgs.size(), (pNrp_Header_Packet) buffer);

            // copy messages into buffer
            // Note: Potential perf improvement could be had by using sendmsg
            for(auto& buf : msgs)
            {
                pNrp_Header_Message msgptr = (pNrp_Header_Message) buf.get();
                count = ntohs(msgptr->length);

                memcpy(msg, buf.get(), count);

                // advance the pointer to the end of the message
                msg = NextMessage(msg);
            }

            NrpdLog::LogString("Server: sending response");

            // send generated packet
            if( (count = sendto(m_socketfd, buffer, messageLength, 0, (sockaddr*) &srcAddr, srcAddrLen)) < 0)
            {
                // TODO: log some error
                NrpdLog::LogString("Server: failed to send to client");
                continue;
            }

            // Clean up the list
            msgs.clear();
        }

        return EXIT_SUCCESS;
    }

    void NrpdServer::ServerThread(shared_ptr<NrpdServer> server)
    {
        // TODO: Do something with the return value here
        server->ServerLoop();
    }
}
