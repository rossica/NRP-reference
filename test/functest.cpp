#include <iostream>
#include <memory>
#include <string.h>

#include "../protocol.h"

using namespace std;
using namespace nrpd;

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
        cout << "All structure sizes are as expected!" << endl;
    }

    return result;
}

bool TestCreateRequest()
{
    char buffer[1024];
    pNrp_Header_Message msg;
    unsigned int msgCount = 3;
    unsigned int size = sizeof(Nrp_Header_Packet) + msgCount * sizeof(Nrp_Header_Message);

    // TODO: Pull packet generation into a helper function.

    msg = GeneratePacketHeader(size, nrpd_msg_type::request, msgCount, (pNrp_Header_Packet)&buffer);

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

    // TODO: Validate failure cases

    cout << "Request packet creation/validation passed!" << endl;
    return true;
}

