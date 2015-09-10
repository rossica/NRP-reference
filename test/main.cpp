#include "functest.h"
#include <list>

int main(int argc, char* argv[])
{
    bool result = true;

    result = TestStructSizes();
    result = TestCreateRequest();
    result = TestCreateResponse();

    if(!result)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
