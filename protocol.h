/* This file defines protocol-specific functions and structures */

#pragma once

namespace nrpd
{
    enum nrpd_msg_type
    {
        nrpd_msg_type_min = 0,
        request = 1,            // Request a response from a server
        response = 2,           // Respond to a request
        response_msg_min = 3,
        reject = 3,             // Reject a request
        request_msg_min = 4,
        ip4peers = 4,           // Valid IP4 hosts that speak NRP
        entropy = 5,            // Random data
        ip6peers = 6,           // Valid IP6 hosts that speak NRP
        certchain = 7,          // X509 certificate chain for establishing trust in server.
        signkey = 8,            // Server's public key for signing secure entropy.
        encryptionkey = 9,      // Server's public key for encrypting secure entropy.
        secureentropy = 10,     // Entropy encrypted with a one-time key.
        nrpd_msg_type_max
    };

    enum nrpd_reject_reason
    {
        unspecified = 0,    // generic, server-side error
        busy,               // congestion control; clients should back off 1.25x
        shuttingdown,       // server is shutting down
        unsupported,        // requested option not configured; don't request again
        nrpd_reject_reason_max
    };

    typedef struct _NRP_HEADER_MESSAGE
    {
        unsigned short length;
        unsigned char msgType;
        unsigned char countOrSize;
        unsigned char content[];
    } Nrp_Header_Message, *pNrp_Header_Message;

    typedef struct _NRP_HEADER_PACKET
    {
        unsigned short length;
        unsigned char msgType;
        unsigned char msgCount;
        Nrp_Header_Message messages[];
    } Nrp_Header_Packet,   *pNrp_Header_Packet,
      Nrp_Header_Request,  *pNrp_Header_Request,
      Nrp_Header_Response, *pNrp_Header_Response;

    typedef struct _NRP_MESSAGE_REJECT
    {
        unsigned char msgType;
        unsigned char reason;
    } Nrp_Message_Reject, *pNrp_Message_Reject;

    typedef struct _NRP_MESSAGE_IP4PEER
    {
        unsigned char ip[4];
        unsigned short port;
    } Nrp_Message_Ip4Peer, *pNrp_Message_Ip4Peer;

    typedef struct _NRP_MESSAGE_ENTROPY
    {
        unsigned char entropy[];
    } Nrp_Message_Entropy, *pNrp_Message_Entropy;

    typedef struct _NRP_MESSAGE_IP6PEER
    {
        unsigned char ip[16];
        unsigned short port;
    } Nrp_Message_Ip6Peer, *pNrp_Message_Ip6Peer;

    typedef struct _NRP_MESSAGE_CERTCHAIN_REQUEST
    {
        unsigned short requestChunk;
    } Nrp_Message_CertChain_Request, *pNrp_Message_CertChain_Request;

    typedef struct _NRP_MESSAGE_CERTCHAIN_RESPONSE_IP4
    {
        unsigned short chunk;
        unsigned short totalChunks;
        unsigned char data[464];
        unsigned char hash[32];
    } Nrp_Message_CertChain_Response4, *pNrp_Message_CertChain_Response4;

    typedef struct _NRP_MESSAGE_CERTCHAIN_RESPONSE_IP6
    {
        unsigned short chunk;
        unsigned short totalChunks;
        unsigned char data[964];
        unsigned char hash[32];
    } Nrp_Message_CertChain_Response6, *pNrp_Message_CertChain_Response6;

    typedef struct _NRP_MESSAGE_SIGNKEY_RESPONSE
    {
        unsigned char key[32];
        char epiryTime[21];
        unsigned char keyId;
        unsigned char signature[];
    } Nrp_Message_SignKey_Response, *pNrp_Message_SignKey_Response;

    typedef struct _NRP_MESSAGE_ENCRYPTIONKEY_RESPONSE
    {
        unsigned char key[32];
        char expiryTime[21];
        unsigned char keyId;
        unsigned char signature[];
    } Nrp_Message_EncryptionKey_Response, *pNrp_Message_EncryptionKey_Response;

    typedef struct _NRP_MESSAGE_SECUREENTROPY_REQUEST
    {
        unsigned char clientKey[32];
        unsigned char keyId;
    } Nrp_Message_SecureEntropy_Request, *pNrp_Message_SecureEntropy_Request;

    typedef struct _NRP_MESSAGE_SECUREENTROPY_RESPONSE
    {
        unsigned char entropy[128];
        unsigned char signature[];
    } Nrp_Message_SecureEntropy_Response, *pNrp_Message_SecureEntropy_Response;


#define MAX_BYTE (255)
#define MAX_REQUEST_MESSAGE_SIZE (65507)
#define MAX_RESPONSE_MESSAGE_SIZE (65507) // Realistically should be less than MTU
#define MAX_REJECT_MESSAGE_SIZE sizeof(Nrp_Header_Message) + (MAX_BYTE * sizeof(Nrp_Message_Reject))
#define DEFAULT_ENTROPY_SIZE (8)
#define MAX_ENTROPY_SIZE (512)
#define NRP_PACKET_HEADER_SIZE ((int) sizeof(Nrp_Header_Packet))
#define NRP_MESSAGE_HEADER_SIZE ((int) sizeof(Nrp_Header_Message))
#define RESPONSE_HEADER_SIZE NRP_PACKET_HEADER_SIZE
#define CLIENT_MIN_RETRY_SECONDS (60)
#define CLIENT_RESPONSE_TIMEOUT_SECONDS (30)
#define CLIENT_MAX_SERVER_TIMEOUT_COUNT (5)
#define MAX_IP6_PACKET_SIZE (1236)
#define MAX_IP4_PACKET_SIZE (532)

    // Advance message pointer by length of message
    pNrp_Header_Message NextMessage(pNrp_Header_Message msg);

    // Advance message pointer by a number of bytes
    pNrp_Header_Message NextMessage(unsigned char* msg, unsigned short count);

    // Calculate end of packet
    void* EndOfPacket(pNrp_Header_Packet pkt);

    // Validate the internal messages' headers in request/reponse packets
    bool ValidateMessageHeader(pNrp_Header_Message hdr, bool isRequest);

    // Validate the request packet headers
    bool ValidateRequestPacket(pNrp_Header_Request pkt);

    // Validate the response packet headers
    bool ValidateResponsePacket(pNrp_Header_Response pkt);

    // Validate the reject message
    bool ValidateRejectMessage(pNrp_Header_Message hdr);

    // Generates an entropy request message
    // Returns a pointer to the end of message on success, nullptr otherwise.
    pNrp_Header_Message GenerateRequestEntropyMessage(unsigned char requestedEntropy, pNrp_Header_Message msg);

    // Generates an entropy response message
    // Returns a pointer to the end of the message on success, nullptr otherwise.
    pNrp_Header_Message GenerateResponseEntropyMessage(unsigned char entropyLength, unsigned char* entropy, unsigned int bufferSize, pNrp_Header_Message buffer);

    // Generates a peer request message
    // Returns a pointer to the end of the message on success, nullptr otherwise.
    pNrp_Header_Message GenerateRequestPeersMessage(nrpd_msg_type ipType, unsigned char countOfPeers, pNrp_Header_Message buffer);

    // Generates a peer response message
    // Returns a pointer to the end of the message on success, nullptr otherwise.
    pNrp_Header_Message GenerateResponsePeersMessage(nrpd_msg_type ipType, unsigned char countOfPeers, unsigned char* ListOfPeers, unsigned int bufferSize, pNrp_Header_Message buffer);

    // Generates a reject header
    // Returns a pointer to the end of the header on success, nullptr otherwise.
    pNrp_Message_Reject GenerateRejectHeader(unsigned char count, pNrp_Header_Message hdr);

    // Generate a reject message
    // Returns a pointer to the end of the message on success, nullptr otherwise.
    pNrp_Message_Reject GenerateRejectMessage(nrpd_reject_reason reason, nrpd_msg_type message, pNrp_Message_Reject hdr);

    // Generate a packet header
    // Returns a pointer to the end of the message on success, nullptr otherwise.
    pNrp_Header_Message GeneratePacketHeader(unsigned short length, nrpd_msg_type type, unsigned char msgCount, pNrp_Header_Packet buffer);

}
