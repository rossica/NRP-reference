#include "log.h"
#include <iostream>

using namespace std;

namespace nrpd
{
    void NrpdLog::LogString(string s)
    {
        cout << s << endl;
    }
}
