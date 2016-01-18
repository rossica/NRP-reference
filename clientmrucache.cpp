#include "clientmrucache.h"
#include <thread>

namespace nrpd
{
    ClientMRUCache::ClientMRUCache(int lifetimeSeconds)
        : m_lifetimeSeconds(lifetimeSeconds),
          m_cleanIntervalSeconds(lifetimeSeconds),
          m_lastCleanTime(s_clock.now())
    {
    }


    void ClientMRUCache::CleanerThread(shared_ptr<ClientMRUCache> target)
    {
        if(!target->Clean())
        {
            // TODO: log error
        }
    }


    bool ClientMRUCache::Clean()
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

    void ClientMRUCache::ScheduleCleaning()
    {
        // Schedule cleaning if it's been long enough since the previous one
        if(s_clock.now() >= (m_lastCleanTime + m_cleanIntervalSeconds))
        {
            // Note: we use shared_from_this() in order to keep the object alive
            // until this thread exits.
            thread cleaningThread(ClientMRUCache::CleanerThread, shared_from_this());

            // Let the cleaner do its job and continue executing
            cleaningThread.detach();
        }
    }


    bool ClientMRUCache::IsPresentAdd(sockaddr_storage& addr)
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

    bool ClientMRUCache::IsPresent(sockaddr_storage& addr)
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

    void ClientMRUCache::Add(sockaddr_storage& addr)
    {
        lock_guard<mutex> lock(m_mutex);
        // Don't check whether the insertion succeeded or not because it's not
        // important in this case.
        m_recentClients.emplace(addr, s_clock.now());

    }
}

