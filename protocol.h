/* This file defines protocol-specific functions and structures */

namespace nrpd
{
    enum nrpd_msg_type
    {
        request_entropy = 1,
        response_entropy = 2,
        response_reject = 4
    };

    typedef struct _NRP_HEADER_COMMON 
    {
        unsigned short length;
        unsigned char msgType;
    } Nrp_Header_Common, *pNrp_Header_Common;

    typedef struct _NRP_HEADER_REQUEST
    {
        //Nrp_Header_Common nrpHeader;
        unsigned short length;
        //unsigned char msgType;
        unsigned short requestedEntropy;
    } Nrp_Header_Request, *pNrp_Header_Request;

    typedef struct _NRP_HEADER_RESPONSE
    {
        Nrp_Header_Common nrpHeader;
        unsigned short entropyLength;
        unsigned char entropy[];
    } Nrp_Header_Response, *pNrp_Header_Response;

    typedef struct _NRP_HEADER_REJECT
    {
        Nrp_Header_Common nrpHeader;
        unsigned char reason;
    } Nrp_Header_Reject, *pNrp_Header_Reject;

#define MAX_REQUEST_MESSAGE_SIZE sizeof(Nrp_Header_Request)
}
