#include <string>
#include <memory>

#pragma once

namespace nrpd
{
    class NrpdLog
    {
        public:
            NrpdLog() = default;
            static void LogString(std::string s);
        private:
            bool m_initialized;
    };
}

