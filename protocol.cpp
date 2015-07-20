/* This file implements protocol-specific functionality */


#include "protocol.h"

#include <string.h>

#include <arpa/inet.h>


using namespace std;
namespace nrpd
{
    bool ValidateRequestEntropyPacket(pNrp_Header_Request pkt)
    {
        if(pkt == nullptr)
        {
            return false;
        }

        if(ntohs(pkt->length) != MAX_REQUEST_MESSAGE_SIZE)
        {
            return false;
        }

        if(pkt->msgType != nrpd_msg_type::request_entropy)
        {
            return false;
        }

        if(pkt->requestedEntropy == 0)
        {
            return false;
        }

        return true;
    }

    bool ValidateResponseEntropyPacket(pNrp_Header_Response pkt)
    {
        if(pkt == nullptr)
        {
            return false;
        }

        if(ntohs(pkt->length) > MAX_RESPONSE_MESSAGE_SIZE)
        {
            return false;
        }

        if(pkt->msgType != nrpd_msg_type::response_entropy)
        {
            return false;
        }

        if(pkt->entropyLength == 0)
        {
            return false;
        }

        return true;
    }

    bool ValidateRejectPacket(pNrp_Header_Reject pkt)
    {
        if(pkt == nullptr)
        {
            return false;
        }

        if(ntohs(pkt->length) > MAX_REJECT_MESSAGE_SIZE)
        {
            return false;
        }

        if(pkt->msgType != nrpd_msg_type::response_reject)
        {
            return false;
        }

        if(pkt->reason >= nrpd_reject_reason::nrpd_reject_reason_max)
        {
            return false;
        }

        return true;
    }

    bool GenerateRequestEntropyPacket(unsigned char requestedEntropy, pNrp_Header_Request pkt)
    {
        if(requestedEntropy == 0)
        {
            return false;
        }

        if(pkt == nullptr)
        {
            return false;
        }

        pkt->length = htons(MAX_REQUEST_MESSAGE_SIZE);
        pkt->msgType = nrpd_msg_type::request_entropy;
        pkt->requestedEntropy = requestedEntropy;

        return true;
    }

    bool GenerateResponseEntropyPacket(unsigned char entropyLength, unsigned char* entropy, unsigned int bufferSize, pNrp_Header_Response buffer)
    {
        if(entropy == nullptr)
        {
            return false;
        }

        if(buffer == nullptr)
        {
            return false;
        }

        if(entropyLength == 0)
        {
            return false;
        }

        if(bufferSize < ( RESPONSE_HEADER_SIZE + entropyLength ))
        {
            return false;
        }

        buffer->length = htons(RESPONSE_HEADER_SIZE + entropyLength);
        buffer->msgType = nrpd_msg_type::response_entropy;
        buffer->entropyLength = entropyLength;

        memcpy(buffer->entropy, entropy, entropyLength);

        return true;
    }

    bool GenerateRejectPacket(nrpd_reject_reason reason, pNrp_Header_Reject pkt)
    {
        if(reason >= nrpd_reject_reason::nrpd_reject_reason_max)
        {
            return false;
        }

        if(pkt == nullptr)
        {
            return false;
        }

        pkt->length = htons(MAX_REJECT_MESSAGE_SIZE);
        pkt->msgType = nrpd_msg_type::response_reject;
        pkt->reason = reason;

        return true;
    }
}

