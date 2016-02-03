#include "functest.h"
#include <iostream>
#include <list>

#define RUN_TEST(_test_) \
    if(!(result = _test_())) \
    { \
        std::cout << #_test_ << " failed." << std::endl << std::endl; \
    } \


int main(int argc, char* argv[])
{
    bool result = true;

    RUN_TEST(TestStructSizes);
    RUN_TEST(TestProtocolCreateRequest);
    RUN_TEST(TestProtocolCreateResponse);
    RUN_TEST(TestServerCalculateMessageSize);
    RUN_TEST(TestConfigActiveServerCount);
    RUN_TEST(TestConfigGetServerList);
    RUN_TEST(TestServerGeneratePeersResponse);
    RUN_TEST(TestServerGenerateEntropyResponse);
    RUN_TEST(TestMruCacheSockaddrStorage);
    RUN_TEST(TestOperatorEqualsSockaddrStorage);
    RUN_TEST(TestHashSockaddrStorage);
    RUN_TEST(TestHashServerRecord);


    if(!result)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
