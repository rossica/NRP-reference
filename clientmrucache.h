#include <memory>
#include <unordered_map>
#include <chrono>
#include <netinet/in.h>
#include <time.h>
#include <string.h>
#include <mutex>
#include <thread>
#include "stdhelpers.h"

#pragma once


using namespace std;

namespace nrpd
{
    template<typename Key>
    class ClientMRUCache : public std::enable_shared_from_this<ClientMRUCache<Key>>
    {
    public:
        ClientMRUCache(int lifetimeSeconds)
            : m_lifetimeSeconds(lifetimeSeconds),
            m_cleanIntervalSeconds(lifetimeSeconds),
            m_lastCleanTime(s_clock.now())
        {
        }

        // Thread procedure to run the Clean member function on an instance
        static void CleanerThread(shared_ptr<ClientMRUCache<Key>> target)
        {
            if(!target->Clean())
            {
                // TODO: log error
            }
        }

        // Clean out expired entries
        // Returns false on error; true on success
        bool Clean()
        {
            {
                // Hold the lock for the duration of the iterator
                lock_guard<mutex> lock(m_mutex);
                int count = 0;

                auto item = m_recentClients.begin();

                while(item != m_recentClients.end())
                {
                    if(s_clock.now() > ((*item).second + m_lifetimeSeconds))
                    {
                        count++;
                        item = m_recentClients.erase(item);
                    }
                    else
                    {
                        item = next(item);
                    }
                }
            } // End of lock scope

            // TODO: Compute better heuristic for cleaning frequency

            m_lastCleanTime = s_clock.now();

            return true;
        }

        // Test whether an entry already exists in the map.
        // If it exists, but has expired, returns false.
        // If it exists, but hasn't expired, returns true.
        // If it doesn't exist, inserts it and returns false.
        bool IsPresentAdd(Key& addr)
        {
            bool response = false;
            {
                // Hold lock for duration of iterator
                lock_guard<mutex> lock(m_mutex);

                // attempt to insert address
                auto const& result = m_recentClients.emplace(addr, s_clock.now());

                // Failed to insert because it is already inserted
                if(!result.second)
                {
                    // check whether client is expired or not
                    if(s_clock.now() >= (result.first->second + m_lifetimeSeconds))
                    {
                        // Expired: return false, update time
                        response = false;
                        result.first->second = s_clock.now();
                    }
                    else
                    {
                        // Client hasn't expired, and must still wait
                        response = true;
                    }
                }
                else
                {
                    // new client was inserted successfully
                    // therefore, they weren't already present
                    response = false;
                }
            } // End of lock scope

            // schedule cleaning if it's been long enough since last cleaning
            ScheduleCleaning();

            return response;
        }

        // Check whether address is present without adding it
        // Returns true if it exists and hasn't expired.
        // Returns false if it doesn't exist, or does exist and has expired
        bool IsPresent(Key& addr)
        {
            bool found = false;
            {
                lock_guard<mutex> lock(m_mutex);

                auto iter = m_recentClients.find(addr);

                if(iter != m_recentClients.end())
                {
                    if(s_clock.now() >= ((*iter).second + m_lifetimeSeconds))
                    {
                        // It's expired
                        found = false;
                    }
                    else
                    {
                        found = true;
                    }
                }
            } // End of lock scope

            // Schedule cleaning if it's been long enough since previous cleaning
            ScheduleCleaning();

            return found;
        }

        // Add address to the container
        void Add(Key& addr)
        {
            lock_guard<mutex> lock(m_mutex);
            // Don't check whether the insertion succeeded or not because it's not
            // important in this case.
            m_recentClients.emplace(addr, s_clock.now());
        }
    private:
        mutex m_mutex;
        static chrono::steady_clock s_clock;
        unordered_map<Key, chrono::time_point<chrono::steady_clock>> m_recentClients;
        chrono::seconds m_lifetimeSeconds; // entry lifetime
        chrono::seconds m_cleanIntervalSeconds; // time between cleanings
        chrono::time_point<chrono::steady_clock> m_lastCleanTime; // time of last cleaning

        // Schedules a cleaning if it's been at least m_cleanIntervalSeconds
        // since m_lastCleanTime.
        void ScheduleCleaning()
        {
            // Schedule cleaning if it's been long enough since the previous one
            if(s_clock.now() >= (m_lastCleanTime + m_cleanIntervalSeconds))
            {
                // Note: we use shared_from_this() in order to keep the object alive
                // until this thread exits.
                thread cleaningThread(ClientMRUCache<Key>::CleanerThread, this->shared_from_this());

                // Let the cleaner do its job and continue executing
                cleaningThread.detach();
            }
        }
    };

}
