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

    if(!result)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
