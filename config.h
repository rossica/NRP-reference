#include <string>


using namespace std;

namespace nrpd
{
    class NrpdConfig
    {
    public:
        NrpdConfig();
        NrpdConfig(string*);
    private: 
        string m_configPath;
        short m_port;
        
    };
}
