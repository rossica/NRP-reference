#include <memory>
#include <unordered_map>
#include <chrono>
#include <netinet/in.h>
#include <time.h>
#include <string.h>
#include <mutex>
#include "stdhelpers.h"

#pragma once


using namespace std;

namespace nrpd
{

    class ClientMRUCache : public std::enable_shared_from_this<ClientMRUCache>
    {
        public:
            ClientMRUCache(int lifetimeSeconds);

            // Thread procedure to run the Clean member function on an instance
            static void CleanerThread(shared_ptr<ClientMRUCache> target);

            // Clean out expired entries
            // Returns false on error; true on success
            bool Clean();

            // Test whether an entry already exists in the map.
            // If it exists, but has expired, returns false.
            // If it exists, but hasn't expired, returns true.
            // If it doesn't exist, inserts it and returns false.
            bool IsPresentAdd(sockaddr_storage& addr);

            // Check whether address is present without adding it
            // Returns true if it exists and hasn't expired.
            // Returns false if it doesn't exist, or does exist and has expired
            bool IsPresent(sockaddr_storage& addr);

            // Add address to the container
            void Add(sockaddr_storage& addr);
        private:
            mutex m_mutex;
            static chrono::steady_clock s_clock;
            unordered_map<sockaddr_storage, chrono::time_point<chrono::steady_clock>> m_recentClients;
            chrono::seconds m_lifetimeSeconds; // entry lifetime
            chrono::seconds m_cleanIntervalSeconds; // time between cleanings
            chrono::time_point<chrono::steady_clock> m_lastCleanTime; // time of last cleaning

            // Schedules a cleaning if it's been at least m_cleanIntervalSeconds
            // since m_lastCleanTime.
            void ScheduleCleaning();
    };

}
