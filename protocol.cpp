/* This file implements protocol-specific functionality */


#include "protocol.h"

#include <string.h>

#include <arpa/inet.h>


using namespace std;
namespace nrpd
{
    bool ValidateMessageSize(pNrp_Header_Message hdr, size_t msgSize)
    {
        // The length, minus the header, should be equal to the number of
        // messages times the size of those messages.
        if((ntohs(hdr->length) - sizeof(*hdr)) != (hdr->countOrSize * msgSize))
        {
            return false;
        }
        return true;
    }


    bool ValidateMessageHeader(const pNrp_Header_Message hdr, bool isRequest)
    {
        if(hdr == nullptr)
        {
            return false;
        }

        if(ntohs(hdr->length) >= MAX_RESPONSE_MESSAGE_SIZE)
        {
            return false;
        }

        // Validate message type is appropriate for type of packet
        if(hdr->msgType >= nrpd_msg_type::nrpd_msg_type_max ||
           hdr->msgType < (
                   (isRequest) ? nrpd_msg_type::request_msg_min
                                : nrpd_msg_type::response_msg_min))
        {
            return false;
        }

        // Validate size and count agree
        switch(hdr->msgType)
        {
        case nrpd_msg_type::reject:
            // Request messages can't reject
            if (isRequest)
            {
                return false;
            }
            else if(!ValidateMessageSize(hdr, sizeof(Nrp_Message_Reject)))
            {
                return false;
            }
            break;
        case nrpd_msg_type::ip4peers:
            // Request messages have no content, just header
            if (isRequest && !ValidateMessageSize(hdr, 0))
            {
                return false;
            }
            else if(!isRequest && !ValidateMessageSize(hdr, sizeof(Nrp_Message_Ip4Peer)))
            {
                return false;
            }
            break;
        case nrpd_msg_type::entropy:
            // Request messages have no content, just header
            if (isRequest && !ValidateMessageSize(hdr, 0))
            {
                return false;
            }
            else if(!isRequest && !ValidateMessageSize(hdr, sizeof(unsigned char)))
            {
                return false;
            }
            break;
        case nrpd_msg_type::ip6peers:
            // Request messages have no content, just header
            if (isRequest && !ValidateMessageSize(hdr, 0))
            {
                return false;
            }
            else if(!isRequest && !ValidateMessageSize(hdr, sizeof(Nrp_Message_Ip6Peer)))
            {
                return false;
            }
            break;
        case nrpd_msg_type::pubkey:
        case nrpd_msg_type::secureentropy:
        default:
            // Not a valid type, fail.
            return false;
        }

        return true;
    }


    bool ValidatePacket(pNrp_Header_Packet pkt, bool isRequest)
    {
        if(pkt == nullptr)
        {
            return false;
        }

        if(ntohs(pkt->length)
           > ((isRequest) ? MAX_REQUEST_MESSAGE_SIZE
                          : MAX_RESPONSE_MESSAGE_SIZE))
        {
            return false;
        }

        // Must be a response or response
        if(pkt->msgType
           != ((isRequest) ? nrpd_msg_type::request : nrpd_msg_type::response))
        {
            return false;
        }

        // Can't request or respond with nothing
        if(pkt->msgCount == 0)
        {
            return false;
        }

        // Verify count of messages and packet size
        const unsigned char* ptr = (unsigned char*) pkt->messages;
        int msgCount = 0;

        while(ptr < ((unsigned char*) pkt) + ntohs(pkt->length))
        {
            pNrp_Header_Message msgPtr = (pNrp_Header_Message) ptr;

            // Validate message header is appropriate for packet type
            if(!ValidateMessageHeader(msgPtr, isRequest))
            {
                return false;
            }

            // Go to next message
            ptr += ntohs(msgPtr->length);
            ++msgCount;
        }

        // Verify message count matches
        if(pkt->msgCount != msgCount)
        {
            return false;
        }

        // Verify packet length matches
        if(ptr > ((unsigned char*) pkt) + ntohs(pkt->length))
        {
            return false;
        }

        return true;
    }


    bool ValidateRequestPacket(pNrp_Header_Request pkt)
    {

        return ValidatePacket(pkt, true);
    }


    bool ValidateResponsePacket(pNrp_Header_Response pkt)
    {
        return ValidatePacket(pkt, false);
    }


    bool ValidateRejectMessage(pNrp_Header_Message hdr)
    {
        if(hdr == nullptr)
        {
            return false;
        }

        // Too long of a reject message
        if(ntohs(hdr->length) > MAX_REJECT_MESSAGE_SIZE)
        {
            return false;
        }

        // Not a reject message
        if(hdr->msgType != nrpd_msg_type::reject)
        {
            return false;
        }

        // Can't reject with zero messages
        if(hdr->countOrSize == 0)
        {
            return false;
        }

        // The length of this message is not a multiple of reject messages
        if((ntohs(hdr->length) - sizeof(*hdr)) % sizeof(Nrp_Message_Reject) != 0)
        {
            return false;
        }

        int idx;
        pNrp_Message_Reject msg;

        for(idx = 0, msg = (pNrp_Message_Reject) hdr->content;
            idx < hdr->countOrSize;
            ++idx, ++msg)
        {

            // Validate rejection is for a valid message type
            if (msg->msgType >= nrpd_msg_type::nrpd_msg_type_max ||
                msg->msgType < nrpd_msg_type::request_msg_min)
            {
                return false;
            }

            // Validate rejection reason is in range
            if(msg->reason >= nrpd_reject_reason::nrpd_reject_reason_max)
            {
                return false;
            }
        }

        return true;
    }


    pNrp_Header_Message GenerateRequestEntropyMessage(unsigned char requestedEntropy, pNrp_Header_Message hdr)
    {
        if(hdr == nullptr)
        {
            return nullptr;
        }

        hdr->length = htons(sizeof(Nrp_Header_Message));
        hdr->msgType = nrpd_msg_type::entropy;
        hdr->countOrSize = requestedEntropy;

        return (pNrp_Header_Message) hdr->content;
    }


    pNrp_Header_Message GenerateResponseEntropyMessage(unsigned char entropyLength, unsigned char* entropy, unsigned int bufferSize, pNrp_Header_Message buffer)
    {
        if(entropy == nullptr)
        {
            return nullptr;
        }

        if(buffer == nullptr)
        {
            return nullptr;
        }

        if(entropyLength == 0)
        {
            return nullptr;
        }

        if(bufferSize < ( sizeof(Nrp_Header_Message) + entropyLength ))
        {
            return nullptr;
        }

        buffer->length = htons(sizeof(Nrp_Header_Message) + entropyLength);
        buffer->msgType = nrpd_msg_type::entropy;
        buffer->countOrSize = entropyLength;

        memcpy(buffer->content, entropy, entropyLength);

        return (pNrp_Header_Message) (buffer->content + entropyLength);
    }


    pNrp_Header_Message GenerateRequestPeersMessage(nrpd_msg_type ipType, unsigned char countOfPeers, pNrp_Header_Message buffer)
    {
        if(buffer == nullptr)
        {
            return nullptr;
        }

        if(ipType != nrpd_msg_type::ip4peers && ipType != nrpd_msg_type::ip6peers)
        {
            return nullptr;
        }

        buffer->msgType = ipType;
        buffer->countOrSize = countOfPeers;
        buffer->length = htons(sizeof(Nrp_Header_Message));

        return (pNrp_Header_Message) buffer->content;
    }


    pNrp_Header_Message GenerateResponsePeersMessage(nrpd_msg_type ipType, unsigned char countOfPeers, unsigned char* ListOfPeers, unsigned int bufferSize, pNrp_Header_Message buffer)
    {
        unsigned int contentSize;

        if(buffer == nullptr)
        {
            return nullptr;
        }

        if(ListOfPeers == nullptr)
        {
            return nullptr;
        }

        if(ipType != nrpd_msg_type::ip4peers && ipType != nrpd_msg_type::ip6peers)
        {
            return nullptr;
        }

        if(countOfPeers == 0)
        {
            return nullptr;
        }

        // Calculate message size
        contentSize = countOfPeers * ((ipType == nrpd_msg_type::ip4peers)
                                            ? sizeof(Nrp_Message_Ip4Peer)
                                            : sizeof(Nrp_Message_Ip6Peer));

        if(bufferSize < sizeof(Nrp_Header_Message) + contentSize)
        {
            return nullptr;
        }

        buffer->msgType = ipType;
        buffer->countOrSize = countOfPeers;
        buffer->length = htons(sizeof(Nrp_Header_Message) + contentSize);

        memcpy(buffer->content, ListOfPeers, contentSize);

        return (pNrp_Header_Message) (buffer->content + contentSize);
    }


    // TODO: Make this support more than one rejection
    pNrp_Header_Message GenerateRejectMessage(nrpd_reject_reason reason, nrpd_msg_type message, pNrp_Header_Message hdr)
    {
        if(reason >= nrpd_reject_reason::nrpd_reject_reason_max)
        {
            return nullptr;
        }

        if(hdr == nullptr)
        {
            return nullptr;
        }

        hdr->length = htons(sizeof(Nrp_Header_Message) + sizeof(Nrp_Message_Reject));
        hdr->msgType = nrpd_msg_type::reject;
        hdr->countOrSize = 1;
        pNrp_Message_Reject msg = (pNrp_Message_Reject) hdr->content;

        msg->msgType =  message;
        msg->reason = reason;

        return (pNrp_Header_Message) (hdr->content + sizeof(Nrp_Message_Reject));
    }


    pNrp_Header_Message GeneratePacketHeader(unsigned short length, nrpd_msg_type type, unsigned char msgCount, pNrp_Header_Packet buffer)
    {
        if(buffer == nullptr)
        {
            return nullptr;
        }

        // Only response and request can be packet header types
        if(type != nrpd_msg_type::request && type != nrpd_msg_type::response)
        {
            return nullptr;
        }

        if(length > ((type == nrpd_msg_type::request) ? MAX_REQUEST_MESSAGE_SIZE : MAX_RESPONSE_MESSAGE_SIZE))
        {
            return nullptr;
        }

        if(msgCount == 0)
        {
            return nullptr;
        }

        buffer->length = htons(length);
        buffer->msgType = type;
        buffer->msgCount = msgCount;

        return buffer->messages;
    }
}
