#include "functest.h"
#include <list>

int main(int argc, char* argv[])
{
    bool result = true;

    result = TestStructSizes();
    result = TestProtocolCreateRequest();
    result = TestProtocolCreateResponse();
    result = TestServerCalculateMessageSize();
    result = TestConfigActiveServerCount();
    result = TestConfigGetServerList();
    result = TestServerGeneratePeersResponse();
    result = TestServerGenerateEntropyResponse();
    result = TestMruCacheSockaddrStorage();
    result = TestOperatorEqualsSockaddrStorage();
    result = TestHashSockaddrStorage();
    result = TestHashServerRecord();


    if(!result)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
