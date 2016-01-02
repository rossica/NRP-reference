#include <iostream>
#include <memory>
#include <list>
#include <string.h>
#include <arpa/inet.h>
#include <random>
#include <time.h>
#include <math.h>

#include "../protocol.h"

// This is a hack-y way to accomplish this, but I don't philosophically agree
// with the alternatives.
#define private public
#include "../server.h"
#include "../config.h"
#undef private


using namespace std;
using namespace nrpd;

struct TestCaseCalcMsgSize
{
    int available;
    int size;
    int count;
    int expectedResult;
    int expectedCount;
};

struct TestCaseActiveServerCount
{
    shared_ptr<NrpdConfig> config;
    nrpd_msg_type type;
    int expectedCount;
};

struct TestCaseGetServerList
{
    shared_ptr<NrpdConfig> config;
    nrpd_msg_type type;
    int count;
    int outSize;
    bool expectedNullptr;
    int expectedSize;
};

struct TestCaseGenPeersResponse
{
    shared_ptr<NrpdServer> server;
    nrpd_msg_type type;
    int msgCount;
    int availableBytes;
    int responseSize;
    bool expectedNullptr;
    int expectedResponseSize;
};

struct TestCaseGenEntropyResponse
{
    shared_ptr<NrpdServer> server;
    int size;
    int bytesRemaining;
    int responseSize;
    bool expectedNullptr;
    int expectedResponseSize;
};

void GenerateConfigFakeActiveServers(shared_ptr<NrpdConfig>& config, int ip4Count, int ip6Count)
{
    std::mt19937 mt(time(nullptr));
    std::uniform_int_distribution<unsigned char> dis(0, 255); // IP byte
    std::uniform_int_distribution<unsigned short> dis2(0,65535); // port number

    int end = max(ip4Count, ip6Count);

    for(int i = 0; i < end; i++)
    {
        if(i < ip4Count)
        {
            config->m_activeServers.push_back({{{dis(mt), dis(mt), dis(mt), dis(mt)}}, false, dis2(mt), 0, 0L});
        }

        if(i < ip6Count)
        {
            config->m_activeServers.push_back({{{dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt), dis(mt)}}, true, dis2(mt), 0, 0L});
        }
    }
}

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

// The purpose of this is to generate random inputs that should be handled by
// the function appropriately.
void GenerateCalcMsgSizeTestCase(std::list<TestCaseCalcMsgSize>& cases)
{
    int avail;
    int size;
    int count;
    int expectedResult;
    int expectedCount;

    int temp;

    /// Generate an avail too small for a header
    avail = rand() % sizeof(Nrp_Header_Message);
    size = rand() % MAX_RESPONSE_MESSAGE_SIZE;
    count = rand() % MAX_BYTE;
    expectedResult = 0;
    expectedCount = count;
    cases.push_back({avail, size, count, expectedResult, expectedCount});

    /// Generate an avail larger than a header, but smaller than header and 1 msg
    count = 2 + (rand() % (MAX_BYTE-2));
    size = 1 + (rand() % ((MAX_RESPONSE_MESSAGE_SIZE / count) - 1));
    avail = sizeof(Nrp_Header_Message) + 1 + (rand() % (size - 1));
    expectedResult = 0;
    expectedCount = count;
    cases.push_back({avail, size, count, expectedResult, expectedCount});

    /// Generate an avail large enough for data + header
    temp = MAX_RESPONSE_MESSAGE_SIZE - sizeof(Nrp_Header_Message);
    count = 1 + (rand() % (MAX_BYTE-1));
    size = 1 + (rand() % ((temp / count) - 1));
    temp = sizeof(Nrp_Header_Message) + (size * count);
    avail = temp + (rand() % (MAX_RESPONSE_MESSAGE_SIZE - temp));
    expectedCount = count;
    expectedResult = sizeof(Nrp_Header_Message) + (size * count);
    cases.push_back({avail, size, count, expectedResult, expectedCount});

    /// Generate an avail large enough for header and some data, but not all data
    count = 2 + (rand() % (MAX_BYTE - 2));
    temp = MAX_RESPONSE_MESSAGE_SIZE - sizeof(Nrp_Header_Message);
    size = 1 + (rand() % ((temp / count) - 1));
    temp = 1 + (rand() % (count - 1)); // generate a count less than above
    avail = sizeof(Nrp_Header_Message) + ((size * temp)) + (rand() % size);
    expectedCount = temp;
    expectedResult = sizeof(Nrp_Header_Message) + (temp * size);

    cases.push_back({avail, size, count, expectedResult, expectedCount});
}

bool TestServerCalculateMessageSize()
{
    int result = 0;
    auto cases = std::list<TestCaseCalcMsgSize>({
                    {2, 2, 3, 0, 3}, // available space less than header
                    {8, 2, 2, 8, 2}, // available space enough for header and messages
                    {5, 2, 1, 0, 1}, // available space more than header, less than header + 1 message
                    {7, 2, 2, 6, 1}}); // available space more than header + 1 message, less than header + requested messages
    auto server = make_unique<NrpdServer>();

    srand(time(nullptr)); // 1447588177; // useful constant for testing

    GenerateCalcMsgSizeTestCase(cases);
    GenerateCalcMsgSizeTestCase(cases);

    for(auto& test : cases)
    {
        result = server->CalculateMessageSize(test.available, test.size, test.count);
        if(result != test.expectedResult)
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
    shared_ptr<NrpdConfig> tempConfig;
    shared_ptr<NrpdServer> tempServer;
    unique_ptr<unsigned char[]> result;
    std::list<TestCaseGenPeersResponse> cases;
    nrpd_msg_type type;
    int msgCount;
    int availableBytes;
    int expectedResponseSize;
    int temp;


    srand(time(nullptr));

    tempConfig = make_shared<NrpdConfig>();
    GenerateConfigFakeActiveServers(tempConfig, 256, 256);

    tempServer = make_shared<NrpdServer>(tempConfig);

    /// Test cases:
    /// enough space for peers response as requested
    type = ip4peers;
    msgCount = 2;
    availableBytes = sizeof(Nrp_Header_Message) + (msgCount * sizeof(Nrp_Message_Ip4Peer));
    cases.push_back({tempServer, type, msgCount, availableBytes, 0, false, availableBytes});

    type = ip6peers;
    msgCount = 3;
    availableBytes = sizeof(Nrp_Header_Message) + (msgCount * sizeof(Nrp_Message_Ip6Peer));
    cases.push_back({tempServer, type, msgCount, availableBytes, 0, false, availableBytes});

    msgCount = 1 + (rand() % (MAX_BYTE - 1)); // guarantee at least 1 message
    type = (((rand() % 2) == 0) ? ip4peers : ip6peers); // randomly select type
    temp = ((type == ip4peers) ? sizeof(Nrp_Message_Ip4Peer) : sizeof(Nrp_Message_Ip6Peer));
    temp = sizeof(Nrp_Header_Message) + (msgCount * temp);
    availableBytes = temp + (rand() % (MAX_RESPONSE_MESSAGE_SIZE - temp));
    cases.push_back({tempServer, type, msgCount, availableBytes, 0, false, temp});


    /// not enough space for peers response as requested, but with fewer msgs
    msgCount = 2;
    type = ip4peers;
    expectedResponseSize = sizeof(Nrp_Header_Message) + sizeof(Nrp_Message_Ip4Peer);
    availableBytes = expectedResponseSize;
    cases.push_back({tempServer, type, msgCount, availableBytes, 0, false, expectedResponseSize});

    msgCount = 2;
    type = ip6peers;
    expectedResponseSize = sizeof(Nrp_Header_Message) + sizeof(Nrp_Message_Ip6Peer);
    availableBytes = expectedResponseSize;
    cases.push_back({tempServer, type, msgCount, availableBytes, 0, false, expectedResponseSize});

    msgCount = 2 + (rand() % (MAX_BYTE - 2)); // guarantee at least 2 messages
    type = (((rand() % 2) == 0) ? ip4peers : ip6peers); // randomly select type
    temp = ((type == ip4peers) ? sizeof(Nrp_Message_Ip4Peer) : sizeof(Nrp_Message_Ip6Peer));
    expectedResponseSize = sizeof(Nrp_Header_Message) + ((1 + (rand() % msgCount - 1)) * temp);
    availableBytes = expectedResponseSize + (rand() % temp);
    cases.push_back({tempServer, type, msgCount, availableBytes, 0, false, expectedResponseSize});


    /// not enough space for peers response at all
    msgCount = 1 + (rand() % (MAX_BYTE - 1));
    type = (((rand() % 2) == 0) ? ip4peers : ip6peers); // randomly select type
    temp = ((type == ip4peers) ? sizeof(Nrp_Message_Ip4Peer) : sizeof(Nrp_Message_Ip6Peer));
    availableBytes = rand() % (sizeof(Nrp_Header_Message) + temp);
    expectedResponseSize = 0;
    cases.push_back({tempServer, type, msgCount, availableBytes, 0, true, expectedResponseSize});


    /// No servers to select
    msgCount = 1 + (rand() % (MAX_BYTE - 1));
    type = (((rand() % 2) == 0) ? ip4peers : ip6peers); // randomly select type
    temp = ((type == ip4peers) ? sizeof(Nrp_Message_Ip4Peer) : sizeof(Nrp_Message_Ip6Peer));
    availableBytes = sizeof(Nrp_Header_Message) + (msgCount * temp);
    expectedResponseSize = 0;
    tempConfig = make_shared<NrpdConfig>();
    tempServer = make_shared<NrpdServer>(tempConfig);
    cases.push_back({tempServer, type, msgCount, availableBytes, 0, true, expectedResponseSize});

    for(auto& test : cases)
    {
        result = test.server->GeneratePeersResponse(test.type, test.msgCount, test.availableBytes, test.responseSize);

        if(test.expectedNullptr)
        {
            if(result != nullptr)
            {
                cout << "GeneratePeersResponse returned memory. Expected nullptr." << endl;
                return false;
            }
        }
        else
        {
            if(result == nullptr)
            {
                cout << "GeneratePeersResponse returned nullptr. Expected memory." << endl;
                return false;
            }

            if(test.responseSize != test.expectedResponseSize)
            {
                cout << "GeneratePeersResponse returned size: " << test.responseSize << ". Expected: " << test.expectedResponseSize << endl;
                return false;
            }

            if(!ValidateMessageHeader((pNrp_Header_Message) result.get(), false))
            {
                cout << "GeneratePeersResponse returned invalid message. Expected valid message." << endl;
                return false;
            }

        }
    }

    cout << "NrpdServer::GeneratePeersResponse passed all tests!" << endl << endl;
    return true;
}

bool TestServerGenerateEntropyResponse()
{
    int err;
    std::list<TestCaseGenEntropyResponse> cases;
    shared_ptr<NrpdServer> tempServer;
    shared_ptr<NrpdConfig> tempConfig;
    unique_ptr<unsigned char[]> result;


    tempConfig = make_shared<NrpdConfig>();

    tempServer = make_shared<NrpdServer>(tempConfig);

    if((err = tempServer->InitializeServer()) != EXIT_SUCCESS)
    {
        cout << "Failed to initialize server. Error: " << err << endl;
        return false;
    }

    /// Success case
    cases.push_back({tempServer, 8, 12, 0, false, 12});
    /// Default entropy size
    cases.push_back({tempServer, 0, 16, 0, false, 12});
    /// Not enough size for message header
    cases.push_back({tempServer, 12, 3, 0, true, 0});
    /// Not enough size for requested message
    cases.push_back({tempServer, 8, 6, 0, false, 6});
    /// 1 byte of entropy
    cases.push_back({tempServer, 1, 6, 0, false, 5});
    // TODO: Add test cases for error paths reading from random device
    // for example: open a crafted file to read from as the random device

    for(TestCaseGenEntropyResponse& test : cases)
    {
        result = test.server->GenerateEntropyResponse(test.size, test.bytesRemaining, test.responseSize);

        if(test.expectedNullptr)
        {
            if(result != nullptr)
            {
                cout << "GenerateEntropyResponse returned memory. Expected nullptr" << endl;
                return false;
            }
        }
        else
        {
            if(result == nullptr)
            {
                cout << "GenerateEntropyResponse returned nullptr. Expected memory" << endl;
                return false;
            }

            if(test.responseSize != test.expectedResponseSize)
            {
                cout << "GenerateEntropyResponse returned response: " << test.responseSize << ". Expected: " << test.expectedResponseSize << endl;
                return false;
            }

            if(!ValidateMessageHeader((pNrp_Header_Message) result.get(), false))
            {
                cout << "GenerateEntropyResponse returned invalid entropy message. Expected valid message" << endl;
                return false;
            }
        }
    }

    cout << "NrpdServer::GenerateEntropyResponse passed all tests!" << endl << endl;
    return true;
}

bool TestConfigGetServerList()
{
    unique_ptr<unsigned char[]> result;
    shared_ptr<NrpdConfig> tempConfig;
    std::list<TestCaseGetServerList> cases;
    pNrp_Message_Ip4Peer ip4Msg;
    pNrp_Message_Ip6Peer ip6Msg;
    int count;

    /// Negative test cases
    tempConfig = make_shared<NrpdConfig>();
    cases.push_back({tempConfig, ip4peers, 1, 0, true, 0}); // no active ip4 servers
    cases.push_back({tempConfig, ip6peers, 1, 0, true, 0}); // no active ip6 servers
    cases.push_back({tempConfig, entropy, 1, 0, true, 0}); // bad type
    cases.push_back({tempConfig, ip4peers, -1, 0, true, 0}); // bad count value

    /// ip4 test cases
    tempConfig = make_shared<NrpdConfig>();
    tempConfig->m_activeServers.push_back({{1,2,3,4}, false, 1234, 0, 0});
    tempConfig->m_activeServers.push_back({{5,6,7,8}, false, 5678, 0, 0});

    cases.push_back({tempConfig, ip4peers, 1, 0 ,false, sizeof(Nrp_Message_Ip4Peer)});
    cases.push_back({tempConfig, ip4peers, 2, 0, false, 2 * sizeof(Nrp_Message_Ip4Peer)});
    cases.push_back({tempConfig, ip6peers, 1, 0, true, 0}); // no ip6 servers

    /// ip6 test cases
    tempConfig = make_shared<NrpdConfig>();
    tempConfig->m_activeServers.push_back({{0,1,2,3,4,5,6,7,8,9,0xa,0xb,0xc,0xd,0xe,0xf}, true, 4321, 0, 0});
    tempConfig->m_activeServers.push_back({{0xf,0xe,0xd,0xc,0xb,0xa,9,8,7,6,5,4,3,2,1,0}, true, 1234, 0, 0});

    cases.push_back({tempConfig, ip6peers, 1, 0, false, sizeof(Nrp_Message_Ip6Peer)});
    cases.push_back({tempConfig, ip6peers, 2, 0, false, 2 * sizeof(Nrp_Message_Ip6Peer)});
    cases.push_back({tempConfig, ip4peers, 1, 0, true, 0}); // no ip4 servers

    /// Mixed test cases
    tempConfig = make_shared<NrpdConfig>();
    tempConfig->m_activeServers.push_back({{1,2,3,4}, false, 1234, 0, 0});
    tempConfig->m_activeServers.push_back({{0,1,2,3,4,5,6,7,8,9,0xa,0xb,0xc,0xd,0xe,0xf}, true, 4321, 0, 0});
    tempConfig->m_activeServers.push_back({{5,6,7,8}, false, 5678, 0, 0});
    tempConfig->m_activeServers.push_back({{0xf,0xe,0xd,0xc,0xb,0xa,9,8,7,6,5,4,3,2,1,0}, true, 1234, 0, 0});

    cases.push_back({tempConfig, ip4peers, 1, 0 ,false, sizeof(Nrp_Message_Ip4Peer)});
    cases.push_back({tempConfig, ip4peers, 2, 0, false, 2 * sizeof(Nrp_Message_Ip4Peer)});
    cases.push_back({tempConfig, ip6peers, 1, 0, false, sizeof(Nrp_Message_Ip6Peer)});
    cases.push_back({tempConfig, ip6peers, 2, 0, false, 2 * sizeof(Nrp_Message_Ip6Peer)});



    for(auto& test : cases)
    {
        count = 0;
        result = test.config->GetServerList(test.type, test.count, test.outSize);

        if(test.expectedNullptr)
        {
            if(result.get() != nullptr)
            {
                cout << "GetServerList returned memory. Expected nullptr" << endl;
                //return false;
            }
        }
        else
        {
            if(result.get() == nullptr)
            {
                cout << "GetServerList returned nullptr. Expected memory" << endl;
                //return false;
            }

            if(test.outSize != test.expectedSize)
            {
                cout << "GetServerList output size: " << test.outSize << ". Expected: " << test.expectedSize << endl;
                return false;
            }

            /// Validate the data copied correctly
            if(test.type == ip4peers)
            {
                ip4Msg = (pNrp_Message_Ip4Peer) result.get();

                for(auto& rec : test.config->m_activeServers)
                {
                    if((test.type == ip6peers) == rec.ipv6)
                    {
                        if(ip4Msg->port != rec.port)
                        {
                            cout << "Copied port is: " << ip4Msg->port << ". Expected: " << rec.port << endl;
                            return false;
                        }

                        if(memcmp(ip4Msg->ip, rec.host4, sizeof(ip4Msg->ip)))
                        {
                            cout << "Copied IPv4 address doesn't match! Fail." << endl;
                            return false;
                        }

                        // increment pointer and counter
                        ip4Msg++;
                        count++;
                    }

                    // Exit the loop when all messages have been compared
                    if(count >= test.count)
                    {
                        break;
                    }
                }
            }
            else
            {
                ip6Msg = (pNrp_Message_Ip6Peer) result.get();

                for(auto& rec : test.config->m_activeServers)
                {
                    if((test.type == ip6peers) == rec.ipv6)
                    {
                        if(ip6Msg->port != rec.port)
                        {
                            cout << "Copied port is: " << ip6Msg->port << ". Expected: " << rec.port << endl;
                            return false;
                        }

                        if(memcmp(ip6Msg->ip, rec.host6, sizeof(ip6Msg->ip)))
                        {
                            cout << "Copied IPv6 address doesn't match! Fail." << endl;
                            return false;
                        }

                        // increment pointer and counter
                        ip6Msg++;
                        count++;
                    }

                    // Exit the loop when all messages have been compared
                    if(count >= test.count)
                    {
                        break;
                    }
                }
            }
        }
    }

    cout << "NrpdConfig::GetServerList passed all tests!" << endl << endl;
    return true;
}

bool TestConfigActiveServerCount()
{
    shared_ptr<NrpdConfig> tempConfig;
    std::list<TestCaseActiveServerCount> cases;
    int count;

    /// Test with empty active server list
    tempConfig = make_shared<NrpdConfig>();
    cases.push_back({tempConfig, ip4peers, 0});
    cases.push_back({tempConfig, ip6peers, 0});
    cases.push_back({tempConfig, entropy, 0});

    /// Test with some fake servers
    tempConfig = make_shared<NrpdConfig>();
    tempConfig->m_activeServers.push_back({{1,2,3,4}, false, 1234, 0, 0});
    tempConfig->m_activeServers.push_back({{0,1,2,3,4,5,6,7,8,9,0xa,0xb,0xc,0xd,0xe,0xf}, true, 4321, 0, 0});
    tempConfig->m_activeServers.push_back({{5,6,7,8}, false, 1234, 0, 0});

    cases.push_back({tempConfig, ip4peers, 2});
    cases.push_back({tempConfig, ip6peers, 1});


    for(auto& test : cases)
    {
        count = test.config->ActiveServerCount(test.type);
        if(count != test.expectedCount)
        {
            cout << "ActiveServerCount returned: " << count << ". Expected: " << test.expectedCount << endl;
            return false;
        }
    }

    cout << "NrpdConfig::ActiveServerCount passed all tests!" << endl << endl;
    return true;
}
