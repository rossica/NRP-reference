#include <iostream>
#include <memory>
#include <list>
#include <string.h>
#include <arpa/inet.h>

#include "../protocol.h"

// This is a hack-y way to accomplish this, but I don't philosophically agree
// with the alternatives.
#define private public
#include "../server.h"
#undef private

using namespace std;
using namespace nrpd;

void TestPlatformAlignment()
{
    struct {short a; unsigned char b;} test3;
    struct {short a; unsigned char c[3];} test5;

    if (sizeof(test5) == 6 && sizeof(test3) == 4)
    {
        cout << "Your compiler/platform is 2-byte aligned" << endl
             << "You shouldn't have any problems..." << endl;
    }
    else if(sizeof(test5) == 8 && sizeof(test3) == 4)
    {
        cout << "Your compiler/platform is 4-byte aligned." << endl
             << "This will require changing the ip4peer, ip6peer, and reject structs" << endl;
    }
    else if(sizeof(test5) == 8 && sizeof(test3) == 8)
    {
        cout << "Your compiler/platform is 8-byte aligned." << endl
             << "This will require changing the ip4peer, ip6peer, and reject structs" << endl;
    }
    else
    {
        cout << "Your compiler/platform is " << sizeof(test5) << "-byte aligned." << endl;
        cout << "This will require careful effort. Good luck!" << endl;
    }
}

bool TestStructSizes()
{
    Nrp_Header_Packet pkt;
    Nrp_Header_Message msg;
    Nrp_Message_Reject rej;
    Nrp_Message_Ip4Peer peer4;
    Nrp_Message_Ip6Peer peer6;
    Nrp_Message_Entropy ent;
    bool result = true;

    cout << "Size of Nrp_Header_Packet: " << sizeof(pkt);
    if(sizeof(pkt) !=
        (sizeof(pkt.length)
        + sizeof(pkt.msgType)
        + sizeof(pkt.msgCount)
        + sizeof(pkt.messages)))
    {
        result = false;
        cout << endl << "Expected: ";
        cout << (sizeof(pkt.length)
                + sizeof(pkt.msgType)
                + sizeof(pkt.msgCount)
                + sizeof(pkt.messages));
    }
    cout << endl;

    cout << "Size of Nrp_Header_Message: " << sizeof(msg);
    if(sizeof(msg) !=
        (sizeof(msg.length)
        + sizeof(msg.msgType)
        + sizeof(msg.countOrSize)
        + sizeof(msg.content)))
    {
        result = false;
        cout << endl << "Expected: ";
        cout << (sizeof(msg.length)
                + sizeof(msg.msgType)
                + sizeof(msg.countOrSize)
                + sizeof(msg.content));
    }
    cout << endl;

    cout << "Size of Nrp_Message_Reject: " << sizeof(rej);
    if(sizeof(rej) != (sizeof(rej.msgType) + sizeof(rej.reason)))
    {
        result = false;
        cout << endl << "Expected: ";
        cout << (sizeof(rej.msgType) + sizeof(rej.reason));
    }
    cout << endl;

    cout << "Size of Nrp_Message_Ip4Peer: " << sizeof(peer4);
    if(sizeof(peer4) != (sizeof(peer4.ip) + sizeof(peer4.port)))
    {
        result = false;
        cout << endl << "Expected: ";
        cout << (sizeof(peer4.ip) + sizeof(peer4.port));
    }
    cout << endl;

    cout << "Size of Nrp_Message_Ip6Peer: " << sizeof(peer6);
    if(sizeof(peer6) != (sizeof(peer6.ip) + sizeof(peer6.port)))
    {
        result = false;
        cout << endl << "Expected: ";
        cout << (sizeof(peer6.ip) + sizeof(peer6.port));
    }
    cout << endl;

    cout << "Size of Nrp_Message_Entropy: " << sizeof(ent);
    if(sizeof(ent) != sizeof(ent.entropy))
    {
        result = false;
        cout << endl << "Expected: ";
        cout << sizeof(ent.entropy);
    }
    cout << endl;

    if(result)
    {
        cout << "All structure sizes are as expected!" << endl << endl;
    }
    else
    {
        TestPlatformAlignment();
    }

    return result;
}

bool TestProtocolCreateRequest()
{
    char buffer[1024];
    pNrp_Header_Message msg, nextmsg;
    unsigned int msgCount = 3;
    unsigned int size = sizeof(Nrp_Header_Packet) + (msgCount * sizeof(Nrp_Header_Message));

    // TODO: Pull packet generation into a helper function.

    /// TEST VALIDATE VALID REQUEST PACKET ///

    msg = GeneratePacketHeader(size, nrpd_msg_type::request, msgCount, (pNrp_Header_Packet)buffer);

    if(msg == nullptr)
    {
        cout << "Failed to generate packet header." << endl;
        return false;
    }

    msg = GenerateRequestEntropyMessage(0, msg);

    if(msg == nullptr)
    {
        cout << "Failed to generate entropy message." << endl;
        return false;
    }

    msg = GenerateRequestPeersMessage(nrpd_msg_type::ip4peers, 0, msg);

    if(msg == nullptr)
    {
        cout << "Failed to genenerate ip4 peers request." << endl;
        return false;
    }

    msg = GenerateRequestPeersMessage(nrpd_msg_type::ip6peers, 0, msg);

    if(msg == nullptr)
    {
        cout << "Failed to generate ip6 peers request." << endl;
        return false;
    }

    if(!ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to validate request packet." << endl;
        return false;
    }


    /// TEST REJECT MALFORMED REQUEST HEADER ///

    // Make size 1 byte too short
    GeneratePacketHeader(size - 1, nrpd_msg_type::request, msgCount, (pNrp_Header_Packet) buffer);


    if(ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to reject too short request packet." << endl;
        return false;
    }

    // Make size 1 byte too long
    GeneratePacketHeader(size + 1, nrpd_msg_type::request, msgCount, (pNrp_Header_Packet) buffer);

    if(ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to reject too long request packet." << endl;
        return false;
    }

    // One fewer message than the count of msgs
    GeneratePacketHeader(size, nrpd_msg_type::request, msgCount + 1, (pNrp_Header_Packet) buffer);

    if(ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to reject missing message request packet." << endl;
        return false;
    }

    // One extra message than the count of messages
    GeneratePacketHeader(size, nrpd_msg_type::request, msgCount - 1, (pNrp_Header_Packet) buffer);

    if(ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to reject extra message request packet." << endl;
        return false;
    }


    // Clear the buffer for negative test
    memset(buffer, 0, sizeof(buffer));

    /// TEST BAD PEER4 MESSAGE ///

    msgCount = 1;
    size = sizeof(Nrp_Header_Packet) + (msgCount * sizeof(Nrp_Header_Message));

    msg = GeneratePacketHeader(size, nrpd_msg_type::request, msgCount, (pNrp_Header_Packet)buffer);

    if(msg == nullptr)
    {
        cout << "Failed to generate packet header. (" << __LINE__ << ")" << endl;
        return false;
    }

    if(nullptr == GenerateRequestPeersMessage(nrpd_msg_type::ip4peers, 0, msg))
    {
        cout << "Failed to genenerate ip4 peers request." << endl;
        return false;
    }

    // Test with length too long by one
    msg->length = htons(ntohs(msg->length) + 1);

    if(ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to reject too long peer4 request." << endl;
        return false;
    }

    // Length less than 1
    msg->length = htons(ntohs(msg->length) - 2);

    if(ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to reject too short peer4 request." << endl;
        return false;
    }

    // Clear the buffer
    memset(buffer, 0, sizeof(buffer));


    /// TEST BAD PEER6 MESSAGE ///

    msg = GeneratePacketHeader(size, nrpd_msg_type::request, msgCount, (pNrp_Header_Packet)buffer);

    if(msg == nullptr)
    {
        cout << "Failed to generate packet header. (" << __LINE__ << ")" << endl;
        return false;
    }

    if(nullptr == GenerateRequestPeersMessage(nrpd_msg_type::ip6peers, 0, msg))
    {
        cout << "Failed to genenerate ip6 peers request." << endl;
        return false;
    }

    // Test with length too long by one
    msg->length = htons(ntohs(msg->length) + 1);

    if(ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to reject too long peer6 request." << endl;
        return false;
    }

    // Length less than 1
    msg->length = htons(ntohs(msg->length) - 2);

    if(ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to reject too short peer6 request." << endl;
        return false;
    }

    // Clear the buffer
    memset(buffer, 0, sizeof(buffer));

    /// TEST TWO BAD MESSAGES AND CORRECT HEADER ///
    msgCount = 2;
    size = sizeof(Nrp_Header_Packet) + (msgCount * sizeof(Nrp_Header_Message));

    msg = GeneratePacketHeader(size, nrpd_msg_type::request, msgCount, (pNrp_Header_Packet)buffer);

    if(msg == nullptr)
    {
        cout << "Failed to generate packet header. (" << __LINE__ << ")" << endl;
        return false;
    }

    nextmsg = GenerateRequestPeersMessage(nrpd_msg_type::ip6peers, 0, msg);

    if(nextmsg == nullptr)
    {
        cout << "Failed to genenerate ip6 peers request." << endl;
        return false;
    }

    // Test with ip6peers length too long by one
    msg->length = htons(ntohs(msg->length) + 1);

    msg = nextmsg;

    nextmsg = GenerateRequestPeersMessage(nrpd_msg_type::ip4peers, 0, msg);

    if(nextmsg == nullptr)
    {
        cout << "Failed to genenerate ip4 peers request." << endl;
        return false;
    }

    // Test with ip4peers length too short by one
    msg->length = htons(ntohs(msg->length) - 1);

    if(ValidateRequestPacket((pNrp_Header_Request) buffer))
    {
        cout << "Failed to reject request with two bad messages." << endl;
        return false;
    }



    /// ALL TESTS PASSED ///
    cout << "All request packet creation/validation tests passed!" << endl << endl;
    return true;
}

bool TestProtocolCreateResponse()
{
    unsigned char buffer[1024], entropy[8];
    pNrp_Header_Message msg;
    pNrp_Message_Reject rej;
    unsigned int msgCount = 4;
    unsigned int countIp4addr = 2;
    unsigned int countEntropy = 8;
    unsigned int countIp6addr = 4;
    unsigned char listOfIp4Addresses[] = {10,0,0,1,21,179,
                                          192,168,1,4,0,79};
    unsigned char listOfIp6Addresses[] = {
        254,128,0,0,222,145,227,95,86,47,48,252,77,183,48,125,00,89,
        254,128,0,0,176,173,150,208,144,78,160,46,190,211,201,37,44,2,
        254,128,0,0,253,241,121,188,225,255,175,74,149,213,98,95,99,184,
        254,128,0,0,176,114,240,173,96,145,208,254,103,191,71,194,121,20};

    unsigned int size = sizeof(Nrp_Header_Packet)
                        + (msgCount * sizeof(Nrp_Header_Message))
                        + (countIp4addr * sizeof(Nrp_Message_Ip4Peer))
                        + (countEntropy)
                        + (countIp6addr * sizeof(Nrp_Message_Ip6Peer))
                        + sizeof(Nrp_Message_Reject);
    unsigned int remainingSize = size;

    /// TEST VALID RESPONSE PACKET HEADER CREATION ///

    msg = GeneratePacketHeader(size, nrpd_msg_type::response, msgCount, (pNrp_Header_Packet) buffer);

    if(msg == nullptr)
    {
        cout << "Failed to create response packet header. (" << __LINE__ << ")" << endl;
    }

    remainingSize -= sizeof(Nrp_Header_Packet);

    msg = GenerateResponsePeersMessage(nrpd_msg_type::ip4peers, countIp4addr, listOfIp4Addresses, remainingSize, msg);

    if(msg == nullptr)
    {
        cout << "Failed to create response ip4 peers message." << endl;
        return false;
    }

    remainingSize -= (sizeof(Nrp_Header_Message) + (countIp4addr * sizeof(Nrp_Message_Ip4Peer)));

    msg = GenerateResponsePeersMessage(nrpd_msg_type::ip6peers, countIp6addr, listOfIp6Addresses, remainingSize, msg);

    if(msg == nullptr)
    {
        cout << "Failed to create response ip6 peers message." << endl;
        return false;
    }

    remainingSize -= (sizeof(Nrp_Header_Message) + (countIp6addr * sizeof(Nrp_Message_Ip6Peer)));

    msg = GenerateResponseEntropyMessage(countEntropy, entropy, remainingSize, msg);

    if(msg == nullptr)
    {
        cout << "Failed to create response entropy message." << endl;
        return false;
    }

    remainingSize -= (sizeof(Nrp_Header_Message) + countEntropy);

    // TODO: remove this hard-coded 1
    rej = GenerateRejectHeader(1, msg);

    if(rej == nullptr)
    {
        cout << "Failed to create reject header." << endl;
        return false;
    }

    rej = GenerateRejectMessage(nrpd_reject_reason::unsupported, nrpd_msg_type::pubkey, rej);

    if(rej == nullptr)
    {
        cout << "Failed to create reject message." << endl;
        return false;
    }

    if(!ValidateResponsePacket((pNrp_Header_Response)buffer))
    {
        cout << "Failed to validate response packet." << endl;
        return false;
    }

    // TODO: Add negative test cases for response packets.


    cout << "All response packet creation/validation tests passed!" << endl << endl;
    return true;
}

bool TestServerCalculateMessageSize()
{
    struct TestCaseCalcMsgSize
    {
        int available;
        int size;
        int count;
        int expectedResult;
        int expectedCount;
    };

    int result = 0;
    auto cases = std::list<TestCaseCalcMsgSize>({
                                                {2, 2, 3, 0, 3},
                                                {8, 2, 2, 8, 2},
                                                {5, 2, 1, 0, 1},
                                                {7, 2, 2, 6, 1}});
    auto server = make_unique<NrpdServer>();

    for(auto& test : cases)
    {
        result = server->CalculateMessageSize(test.available, test.size, test.count);
        if(result != test.expectedResult )
        {
            cout << "CalculateMessageSize returned: " << result << ". Expected: " << test.expectedResult << endl;
            return false;
        }

        if(test.count != test.expectedCount)
        {
            cout << "CalculateMessageSize changed msgCount: " << test.count << ". Expected: " << test.expectedCount << endl;
            return false;
        }
    }

    cout << "NrpdServer::CalculateMessageSize passed all tests!" << endl << endl;
    return true;
}

bool TestServerGeneratePeersResponse()
{
    // TODO: positive and negative test cases for NrpdServer::GeneratePeersResponse
    return false;
}

bool TestConfigGetServerList()
{
    // TODO: positive and negative test cases for NrpdConfig::GetServerList
    return false;
}

bool TestConfigActiveServerCount()
{
    // TODO: positive and negative test cases for NrpdConfig::ActiveServerCount
    return false;
}
