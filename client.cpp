#include "client.h"
#include "protocol.h"
#include "stdhelpers.h"
#include "log.h"
#include <memory>
#include <chrono>
#include <ratio>
#include <cmath>

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

        if(m_socketfd4 > 0)
        {
            close(m_socketfd4);
        }

        if(m_socketfd6 > 0)
        {
            close(m_socketfd6);
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
        sockaddr_in6& hostAddr6 = (sockaddr_in6&) stor;

        if(m_config->enableClientIp6())
        {
            m_socketfd6 = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            if(m_socketfd6 < 0)
            {
                // TODO: log error here
                return errno;
            }

            // Set send timeout
            if(setsockopt(m_socketfd6, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeval)) < 0)
            {
                // TODO: log error
                return errno;
            }

            // Bind to the socket
            hostAddr6.sin6_addr = IN6ADDR_ANY_INIT;
            hostAddr6.sin6_family = AF_INET6;

            if(bind(m_socketfd6, (sockaddr*) &stor, sizeof(hostAddr6)))
            {
                // TODO: add logging
                return errno;
            }
        }

        if(m_config->enableClientIp4())
        {
            m_socketfd4 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if(m_socketfd4 < 0)
            {
                // TODO: log error here
                return errno;
            }

            // Set send timeout
            if(setsockopt(m_socketfd4, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeval)) < 0)
            {
                // TODO: log error
                return errno;
            }

            // Bind to the socket
            hostAddr.sin_addr.s_addr = INADDR_ANY;
            hostAddr.sin_family = AF_INET;

            if(bind(m_socketfd4, (sockaddr*) &stor, sizeof(hostAddr6)))
            {
                // TODO: add logging
                return errno;
            }
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
    bool NrpdClient::ConstructRequest(ServerRecord const& server, unsigned int bufSize, unsigned char* buffer, int& outPktSize)
    {
        int msgCount = 0;
        int msgSize = sizeof(Nrp_Header_Request);
        pNrp_Header_Request pkt = (pNrp_Header_Request) buffer;
        pNrp_Header_Message msg = pkt->messages;

        // Calculate maximum request message for this server
        int serverMsgSupportCount = 2; // For the packet and entropy headers
        serverMsgSupportCount += ((server.signkey) ? 1 : 0);
        serverMsgSupportCount += ((server.ip4Peers) ? 1 : 0);
        serverMsgSupportCount += ((server.ip6Peers) ? 1 : 0);
        // Check size of the buffer is large enough to hold the request
        if(bufSize < (sizeof(Nrp_Header_Message) * serverMsgSupportCount))
        {
            // TODO: log here
            return false;
        }


        // 1. request signing cert info or secure entropy from the server
        // (if signing cert info is already obtained)
        if(server.signkey)
        {
            // if server has signkey info
                //if server.cert->NearExpiration()
                    // TODO: construct new signkey request message
                // if server has certchain info
                    // if server has encryption key info
                        // TODO: construct a secureentropy request message
                    // else
                        // TODO: construct encryptionkey request message
                // else
                    // TODO: construct a certchain request message
            // else
                // TODO: Construct a signkey request message
        }

        // 2. Request entropy from the server
        msg = GenerateRequestEntropyMessage(m_config->defaultEntropySize(), msg);
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

        if(msg != nullptr)
        {
            outPktSize = msgSize;
            return true;
        }
        else
        {
            // TODO: log here
            return false;
        }
    }


    bool NrpdClient::ConnectServer(ServerRecord const& server)
    {
        sockaddr_storage sendServerAddr;
        bool result = true;
        int error = 0;

        if(server.ipv6)
        {
            sendServerAddr.ss_family = AF_INET6;
            sockaddr_in6& servAddr6 = (sockaddr_in6&) sendServerAddr;
            servAddr6.sin6_port = server.port;
            memcpy(&(servAddr6.sin6_addr), server.host6, sizeof(servAddr6.sin6_addr));

            if((error = connect(m_socketfd6, (sockaddr*) &sendServerAddr, sizeof(sockaddr_in6))) < 0)
            {
                // TODO: log error
                NrpdLog::LogString("Client: failed to connect IPv6 server");
                error = errno;
                result = false;
            }
        }
        else
        {
            sendServerAddr.ss_family = AF_INET;
            sockaddr_in& servAddr4 = (sockaddr_in&) sendServerAddr;
            servAddr4.sin_port = server.port;
            memcpy(&(servAddr4.sin_addr), server.host4, sizeof(servAddr4.sin_addr));

            if((error = connect(m_socketfd4, (sockaddr*) &sendServerAddr, sizeof(sockaddr_in))) < 0)
            {
                // TODO: log error
                NrpdLog::LogString("Client: failed to connect IPv4 server");
                error = errno;
                result = false;
            }
        }

        return result;
    }


    bool NrpdClient::ScrambleEntropy(size_t bufSize, unsigned char* entropy)
    {
        array<unsigned char, 64> secret;
        int count = 0;
        bool success = true;

        if(entropy == nullptr || bufSize > MAX_ENTROPY_SIZE)
        {
            return false;
        }

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

        int bytes = std::ceil(bufSize / 8.0f);

        if( (count = read(m_randomfd, secret.data(), bytes)) <= 0)
        {
            // Error reading from random device.
            // TODO: log error
            NrpdLog::LogString("Client: failed to read from random device");

            success = false;
        }
        else
        {
            // Actually do the work of zeroing out parts of the entropy
            for(unsigned int idx = 0; idx < bufSize; idx++)
            {
                entropy[idx] *= ((secret[idx / 8] >> (idx % 8)) & 0x1);
            }
        }

        // Clear secret memory
        memset(secret.data(), 0, secret.size());

        return success;
    }


    bool NrpdClient::ConsumeEntropy(size_t bufSize, unsigned char* entropy)
    {
        // Allocated on the stack so it gets overwritten naturally
        int count = 0;
        bool success = true;

        if(entropy == nullptr || bufSize > MAX_ENTROPY_SIZE)
        {
            return false;
        }

        // write entropy to random device.
        if( (count = write(m_randomfd, entropy, bufSize)) <= 0)
        {
            // Error writing entropy
            // TODO: log error
            NrpdLog::LogString("Client: failed to write to random device");
            // This is serious enough to signal failure
            success = false;
        }

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
                    server.retryTime *= 1.25;
                    break;
                case shuttingdown:
                    server.shuttingdown = true;
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
                        case signkey:
                            server.signkey = false;
                            break;
                        default:
                            // server rejected an unknown message type as unsupported
                            // this shouldn't happen.
                            // (how can a client request a message type it
                            //  doesn't even recognize?)
                            // TODO: log here
                            break;
                    }
                    break;
                default:
                    // TODO: log here. Server used an unrecognized rejection
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

        while((unsigned char*) msg < (buffer + bufSize)
              && msg < EndOfPacket((pNrp_Header_Packet) buffer)
              && msgCount < ((pNrp_Header_Packet) buffer)->msgCount)
        {
            switch(msg->msgType)
            {
            case entropy:
                NrpdLog::LogString("Client: entropy message");
                if(!ScrambleEntropy(msg->countOrSize, msg->content))
                {
                    // Proceed with consuming the entropy without modification;
                    // this should almost never occur, and if it does, it's not
                    // serious enough to block consumption of entropy.
                    // TODO: log here
                }
                if(!ConsumeEntropy(msg->countOrSize, msg->content))
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
                NrpdLog::LogString("Client: received reject message");
                if(!ParseRejectMessage(server, msg))
                {
                    // TODO: log here
                }
                break;
            case signkey:
            case certchain:
            case encryptionkey:
            case secureentropy:
                // TODO: implement these
                break;
            default:
                    break;
            }

            msgCount++;
            msg = NextMessage(msg);
        }

        return true;
    }


    int NrpdClient::ClientLoop()
    {
        m_state = running;
        ssize_t count;
        int error;
        unique_ptr<array<unsigned char, MAX_RESPONSE_MESSAGE_SIZE>> buffer = make_unique<array<unsigned char,MAX_RESPONSE_MESSAGE_SIZE>>();
        pNrp_Header_Packet pkt = (pNrp_Header_Packet) buffer->data();
        int requestSize;
        int socketfd;

        while(m_state == running)
        {
            // Reset variables
            socketfd = 0;
            error = 0;
            count = 0;
            requestSize = 0;

            // Wait a second before trying the next server; don't use
            // this loop as a busy wait and eat CPU for no reason.
            this_thread::sleep_for(1s);

            // Request next server from config
            ServerRecord& server = m_config->GetNextServer();

            // Check that it's been enough time since the last request was made
            // to this server
            if(chrono::steady_clock::now() <= (server.lastaccessTime + server.retryTime))
            {
                // continue to next server. Don't update this one
                continue;
            }

            // Build request based on configuration and known rejections from server (if any)
            if(!ConstructRequest(server, buffer->size(), buffer->data(), requestSize))
            {
                // TODO: log error
                continue;
            }

            // Connect() to the server to simplify communication
            if(!ConnectServer(server))
            {
                // TODO: log error
                // try again?
                continue;
            }

            // Set socketfd for local use
            if(server.ipv6)
            {
                socketfd = m_socketfd6;
            }
            else
            {
                socketfd = m_socketfd4;
            }

            auto firstTimePoint = chrono::high_resolution_clock::now();

            NrpdLog::LogString("Client: Sending request");

            // send request packet to server
            if( (count = send(socketfd, buffer->data(), requestSize, 0)) < 0)
            {
                // If packet exceeds MTU, reset MTU and continue
                // TODO: log error
                continue;
            }

            error = count;

            // receive response
            if( (count = recv(socketfd, buffer->data(), buffer->size(), 0)) < 0)
            {
                error = errno;
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

                NrpdLog::LogString("Client: failed to receive response");
                continue;
            }

            NrpdLog::LogString("Client: Response received");

            auto secondTimePoint = chrono::high_resolution_clock::now();

            // Validate received packet
            if(!ValidateResponsePacket(pkt))
            {
                // TODO: log error
                NrpdLog::LogString("Client: Response failed validation");
                // Increment server fail count, remove from list if last fail
                m_config->IncrementServerFailCount(server);
                continue;
            }

            // Parse received message and perform appropriate actions
            // based on received messages e.g.
            //   write received entropy to system RNG.
            //   add peers to config
            if(!ParseResponse(server, buffer->size(), buffer->data()))
            {
                // TODO log here
                NrpdLog::LogString("Client: Response failed parsing");
                continue;
            }

            NrpdLog::LogString("Client: Response processed");

            // Mark the server as successful
            m_config->MarkServerSuccessful(server);

            // If the implementation-defined high-resolution clock offers
            // microsecond resolution or better, we can use the timing delays
            // between when this code sends a packet and when it is actually
            // sent on the wire, and the delays between when a packet arrives
            // and when this code actually receives the data, to scrape up a
            // few extra bits of real entropy that an attacker cannot control.
            //
            // We need microsecond or better resolution for this to measure
            // the tiny processing delays that provide a few bits of entropy.
            // If the resolution is too low, then this will only measure the
            // network latency, which is something the attacker can know.
            //
            // This is highly recommended, if a high-enough resolution clock
            // is available in the implementation.
            // Since this is implemented as a compile-time compare between
            // static ratio types, any decently smart optimizer will optimize
            // away this code if it doesn't meet the bar.
            if(std::ratio_less_equal<chrono::high_resolution_clock::period, std::micro>::value)
            {
                auto firstDuration = firstTimePoint.time_since_epoch();
                auto secondDuration = secondTimePoint.time_since_epoch();

                auto begin = firstDuration.count();
                auto end = secondDuration.count();

                // N.B. Some implementations lie about the resolution of their
                // clocks, and simply alias the system clock.
                // Since we don't want to be fooled and write predictable
                // entropy to the random device, make a check here before
                // writing the data.
                // If the clock provides microsecond or greater accuracy, then
                // the difference modulo 10000 will provide the microsecond or
                // smaller differences. If this is always 0, the implementation
                // is lying. (it may be legitimately 0, and we accept the risk
                // of occasionally discarding good data).
                // If this value is non-zero, the implementation is good.
                if((end - begin) % 10000)
                {
                    auto timeEntropy = begin ^ end;

                    NrpdLog::LogString("Client: adding time entropy");

                    // Write entropy to pool
                    write(m_randomfd, &timeEntropy, sizeof(timeEntropy));

                    timeEntropy = 0L;
                }
            }
        }

        return 0;
    }

    void NrpdClient::ClientThread(shared_ptr<NrpdClient> client)
    {
        // TODO: Log return value?
        client->ClientLoop();
    }
}
