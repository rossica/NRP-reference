#include "client.h"
#include "protocol.h"
#include "stdhelpers.h"
#include <memory>
#include <chrono>
#include <ratio>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

using namespace std;

namespace nrpd
{
    NrpdClient::NrpdClient()
    {
    }

    NrpdClient::NrpdClient(shared_ptr<NrpdConfig> conf) : m_config(conf), m_state(notinitialized)
    {
    }

    NrpdClient::~NrpdClient()
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

    int NrpdClient::InitializeClient()
    {
        timeval timeout = { (__time_t) (m_config->receiveTimeout()), 0};
        sockaddr_storage stor = {0};
        sockaddr_in& hostAddr = (sockaddr_in&) stor;

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

        // Bind to the socket
        hostAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        hostAddr.sin_family = AF_INET;

        if(bind(m_socketfd, (sockaddr*) &stor, sizeof(sockaddr_in)))
        {
            // TODO: add logging
            return errno;
        }

        // TODO: Make random device configurable
        if((m_randomfd = open("/dev/urandom", O_RDWR)) < 0)
        {
            // TODO: add logging
            return errno;
        }

        m_state = initialized;

        return 0;
    }

    // Client determines the order of preference for responses, by requesting
    // messages in the order of preference. Ordering matters because it
    // attempts to ensure important messages fit in the single response packet
    // while less important messages are ignored.
    // The recommended order is:
    //
    // (pubkey/secureentropy) > entropy > (ip4peers,ip6peers)
    //
    // Reject messages will always be first, if the server sends any. (clients
    // cannot request reject messages, hence their omission from the above)
    //
    // If the server rejects any of the message types, they are removed from
    // the order of preference, and wont be requested in subsequent requests
    // made to that server. Entropy messages cannot be rejected; ever server
    // must support that message at a minimum.
    bool NrpdClient::ConstructRequest(ServerRecord const& server, int bufSize, unsigned char* buffer)
    {
        int msgCount = 0;
        int msgSize = sizeof(Nrp_Header_Request);
        pNrp_Header_Request pkt = (pNrp_Header_Request) buffer;
        pNrp_Header_Message msg = pkt->messages;

        // 1. request public key info or secure entropy from the server
        // (if public key info is already obtained)
        if(server.pubKey)
        {
            // if server has pubkey info
                // TODO: construct a secureentropy request message
            // else if server.cert->NearExpiration()
                // TODO: construct new pubkey request message
            // else
                // TODO: Construct a pubkey request message
        }

        // 2. Request entropy from the server
        msg = GenerateRequestEntropyMessage(DEFAULT_ENTROPY_SIZE, msg);
        msgCount++;
        msgSize += sizeof(Nrp_Header_Message);

        // 3.a. Request ip4 peers (only if the client is configured for ipv4)
        if(server.ip4Peers && m_config->enableClientIp4())
        {
            // Request as many peers as the server has
            msg = GenerateRequestPeersMessage(ip4peers, 0, msg);
            msgCount++;
            msgSize += sizeof(Nrp_Header_Message);
        }

        // 3.b. Request ip6 peers (only if the client is configured for ipv6)
        if(server.ip6Peers && m_config->enableClientIp6())
        {
            // Request as many peers as the server has
            msg = GenerateRequestPeersMessage(ip6peers, 0, msg);
            msgCount++;
            msgSize += sizeof(Nrp_Header_Message);
        }

        msg = GeneratePacketHeader(msgSize, request, msgCount, pkt);

        return (msg != nullptr);
    }

    bool NrpdClient::ParseEntropyMessage(pNrp_Header_Message msg)
    {
        if(msg == nullptr || msg->msgType != entropy)
        {
            return false;
        }

        array<unsigned char, 256> scratch;
        array<unsigned char, 32> secret;
        int count = 0;
        bool success = true;

        // Don't trust the entropy from a server completely, so only accept
        // part of the input.
        // To do this, read some randomness from the random device and use it
        // to zero-out part of the received entropy.
        //
        // Initially, an attacker may be able to guess which parts of entropy
        // are accepted by the client (when the inherent entropy of the system
        // is low, e.g. at boot), but as time goes on and more entropy is
        // collected, this becomes cumbersome for attackers to keep track of.
        //
        // We try to minimize the amount of entropy used for this by only
        // reading as many bits as there are bytes of entropy in the response
        // from the random device. A 1 will allow us to keep that byte, and a 0
        // will be zeroed out.
        // There will always need to be at least 1 byte for less than 8 bytes
        // of entropy.

        int bits = (msg->countOrSize / 8) + 1;

        // Copy received entropy into a scratch buffer where it is modified
        memcpy(scratch.data(), msg->content, msg->countOrSize);

        if( (count = read(m_randomfd, secret.data(), bits)) < 0)
        {
            // Error reading from random device.
            // TODO: log error

            // Proceed with consuming the entropy without modification;
            // this should almost never occur, and if it does, it's not
            // serious enough to block consumption of entropy.
        }
        else
        {
            // Actually do the work of zeroing out parts of the entropy
            for(int idx = 0; idx < msg->countOrSize; idx++)
            {
                scratch[idx] *= ((secret[idx / 8] >> (idx % 8)) & 0x1);
            }
        }

        // write entropy to random device.
        if( (count = write(m_randomfd, scratch.data(), msg->countOrSize)) <= 0)
        {
            // Error writing entropy
            // TODO: log error
            // This is serious enough to signal failure
            success = false;
        }

        // Clear scratch and secret memory
        memset(scratch.data(), 0, scratch.size());
        memset(secret.data(), 0, secret.size());

        return success;
    }

    bool NrpdClient::ParseRejectMessage(ServerRecord& server, pNrp_Header_Message msg)
    {
        if(msg == nullptr)
        {
            return false;
        }

        pNrp_Message_Reject rej = (pNrp_Message_Reject) msg->content;
        int rejCount = 0;

        // Iterate through rejection messages
        while(rejCount < msg->countOrSize)
        {
            switch(rej->reason)
            {
                case busy:
                    // Server is busy, increase time until next contact
                    server.retryTime = server.retryTime * 1.25;
                    break;
                case shuttingdown:
                    // TODO: mark the server as shutting down?
                    break;
                case unsupported:
                    switch(rej->msgType)
                    {
                        case ip4peers:
                            server.ip4Peers = false;
                            break;
                        case ip6peers:
                            server.ip6Peers = false;
                            break;
                        case pubkey:
                            server.pubKey = false;
                            break;
                        default:
                            // server rejected an unknown message type as unsupported
                            // this shouldn't happen.
                            // TODO: log here
                            break;
                    }
                    break;
                default:
                    // TODO: log here. Server rejected an unknown message type
                    // This is weird.
                    break;
            }

            rej++;
            rejCount++;
        }

        return true;
    }

    bool NrpdClient::ParseResponse(ServerRecord& server, int bufSize, unsigned char* buffer)
    {
        pNrp_Header_Message msg = ((pNrp_Header_Packet) buffer)->messages;
        int msgCount = 0;

        while(msg < EndOfPacket((pNrp_Header_Packet) buffer) && msgCount < ((pNrp_Header_Packet) buffer)->msgCount)
        {
            switch(msg->msgType)
            {
            case entropy:
                if(!ParseEntropyMessage(msg))
                {
                    // TODO: log here
                }
                break;
            case ip4peers:
                if(m_config->enableClientIp4())
                {
                    if(!m_config->AddServersFromMessage(msg))
                    {
                        // TODO: log here
                    }
                }
                break;
            case ip6peers:
                if(m_config->enableClientIp6())
                {
                    if(!m_config->AddServersFromMessage(msg))
                    {
                        // TODO: log here
                    }
                }
                break;
            case reject:
                if(!ParseRejectMessage(server, msg))
                {
                    // TODO: log here
                }
                break;
            case pubkey:
            case secureentropy:
                // TODO: implement these
                break;
            default:
                    break;
            }

            msgCount++;
            msg = NextMessage(msg);
        }

        return false;
    }

    int NrpdClient::ClientLoop()
    {
        m_state = running;
        ssize_t count = 0;
        unique_ptr<unsigned char[]> buffer = make_unique<unsigned char[]>(MAX_RESPONSE_MESSAGE_SIZE);
        sockaddr_storage sendServerAddr;
        pNrp_Header_Packet pkt = nullptr;

        while(m_state == running)
        {
            // Request next server from config
            ServerRecord& server = m_config->GetNextServer();

            // Check that it's been enough time since the last request was made
            // to this server
            if(time(nullptr) <= server.lastaccessTime + server.retryTime)
            {
                // continue to next server. Don't update this one
                continue;
            }

            // Build request based on configuration and known rejections from server (if any)
            if(!ConstructRequest(server, MAX_RESPONSE_MESSAGE_SIZE, buffer.get()))
            {
                // TODO: log error
                continue;
            }

            // Connect() to the server to simplify communication
            if(server.ipv6)
            {
                sockaddr_in6& servAddr6 = (sockaddr_in6&) sendServerAddr;
                servAddr6.sin6_port = ntohs(server.port);
                memcpy(&(servAddr6.sin6_addr), server.host6, sizeof(servAddr6.sin6_addr));

                if(connect(m_socketfd, (sockaddr*) &sendServerAddr, sizeof(sockaddr_in6)) < 0)
                {
                    // TODO: log error
                    // try again?
                    continue;
                }
            }
            else
            {
                sockaddr_in& servAddr4 = (sockaddr_in&) sendServerAddr;
                servAddr4.sin_port = ntohs(server.port);
                memcpy(&(servAddr4.sin_addr), server.host4, sizeof(servAddr4.sin_addr));
                if(connect(m_socketfd, (sockaddr*) &sendServerAddr, sizeof(sockaddr_in)) < 0)
                {
                    // TODO: log error
                    // try again?
                    continue;
                }
            }

            auto first = chrono::high_resolution_clock::now();

            // send request packet to server
            if( (count = send(m_socketfd, buffer.get(), MAX_REQUEST_MESSAGE_SIZE, 0)) < 0)
            {
                // If packet exceeds MTU, reset MTU and continue
                // TODO: log error
                continue;
            }

            // receive response
            if( (count = recv(m_socketfd, buffer.get(), MAX_RESPONSE_MESSAGE_SIZE, 0)) < 0)
            {
                // If an error occurred listening for a response
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // timed out waiting for response
                    // increment server fail count
                    // if server fail count is max fail count, remove from list.
                    m_config->IncrementServerFailCount(server);
                }
                else
                {
                    // TODO: log error
                }
                continue;
            }

            auto second = chrono::high_resolution_clock::now();

            // Validate received packet
            pkt = (pNrp_Header_Packet) buffer.get();

            if(!ValidateResponsePacket(pkt))
            {
                // TODO: log error
                // Increment server fail count, remove from list if last fail
                m_config->IncrementServerFailCount(server);
                continue;
            }

            // Parse received message and perform appropriate actions
            // based on received messages e.g.
            //   write received entropy to system RNG.
            //   add peers to config
            ParseResponse(server, MAX_RESPONSE_MESSAGE_SIZE, buffer.get());

            // Mark the server as successful
            m_config->MarkServerSuccessful(server);

            // If the implementation-defined high-resolution clock offers
            // nanosecond resolution or better, we can use the timing delays
            // between when this code sends a packet and when it is actually
            // sent on the wire, and the delays between when a packet arrives
            // and when this code actually receives the data, to scrape up a
            // few extra bits of real entropy that an attacker cannot control.
            //
            // We need microsecond or better resolution for this to measure
            // the tiny processing delays that provide a few bits of entropy.
            // If the resolution is too low, then this will only measure the
            // network latency, which is something the attack can know.
            //
            // This is highly recommended, if a high-enough resolution clock
            // is available in the implementation. If not, this may be
            // omitted.
            if(chrono::high_resolution_clock::period::num <= std::micro::num
               && chrono::high_resolution_clock::period::den >= std::micro::den)
            {
                auto firstDuration = first.time_since_epoch();
                auto secondDuration = second.time_since_epoch();

                auto begin = firstDuration.count();
                auto end = secondDuration.count();

                auto timeEntropy = begin ^ end;

                // Write entropy to pool
                write(m_randomfd, &timeEntropy, sizeof(timeEntropy));

                timeEntropy = 0L;
            }
        }

        return 0;
    }
}
