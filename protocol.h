/* This file defines protocol-specific functions and structures */

#pragma once

namespace nrpd
{
    enum nrpd_msg_type
    {
        request_entropy = 1,
        response_entropy = 2,
        response_reject = 4
    };

    enum nrpd_reject_reason
    {
        unspecified = 0,
        busy,
        nrpd_reject_reason_max
    };

    typedef struct _NRP_HEADER_REQUEST
    {
        unsigned short length;
        unsigned char msgType;
        unsigned char requestedEntropy;
    } Nrp_Header_Request, *pNrp_Header_Request;

    typedef struct _NRP_HEADER_RESPONSE
    {
        unsigned short length;
        unsigned char msgType;
        unsigned char entropyLength;
        unsigned char entropy[];
    } Nrp_Header_Response, *pNrp_Header_Response;

    typedef struct _NRP_HEADER_REJECT
    {
        unsigned short length;
        unsigned char msgType;
        unsigned char reason;
    } Nrp_Header_Reject, *pNrp_Header_Reject;

#define MAX_REQUEST_MESSAGE_SIZE sizeof(Nrp_Header_Request)
#define MAX_RESPONSE_MESSAGE_SIZE (sizeof(Nrp_Header_Response) + 256)
#define MAX_REJECT_MESSAGE_SIZE sizeof(Nrp_Header_Reject)
#define RESPONSE_HEADER_SIZE sizeof(Nrp_Header_Response)

    bool ValidateRequestEntropyPacket(pNrp_Header_Request pkt);

    bool ValidateResponseEntropyPacket(pNrp_Header_Response pkt);

    bool ValidateRejectPacket(pNrp_Header_Reject pkt);

    bool GenerateRequestEntropyPacket(unsigned char requestedEntropy, pNrp_Header_Request pkt);

    bool GenerateResponseEntropyPacket(unsigned char entropyLength, unsigned char* entropy, unsigned int bufferSize, pNrp_Header_Response buffer);

    bool GenerateRejectPacket(nrpd_reject_reason reason, pNrp_Header_Reject pkt);
}
