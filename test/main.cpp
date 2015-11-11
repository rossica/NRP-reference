#include "functest.h"
#include <list>

int main(int argc, char* argv[])
{
    bool result = true;

    result = TestStructSizes();
    result = TestProtocolCreateRequest();
    result = TestProtocolCreateResponse();

    if(!result)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
