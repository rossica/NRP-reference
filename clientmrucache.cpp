#include "clientmrucache.h"
#include <thread>

namespace nrpd
{
    ClientMRUCache::ClientMRUCache(int lifetimeSeconds)
        : m_lifetimeSeconds(lifetimeSeconds),
          m_cleanIntervalSeconds(lifetimeSeconds),
          m_lastCleanTime(time(nullptr))
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

            for(auto& item : m_recentClients)
            {
                if(time(nullptr) > item.second + m_lifetimeSeconds)
                {
                    m_recentClients.erase(item.first);
                }
            }
        } // End of lock scope

        // TODO: Compute better heuristic for cleaning frequency

        m_lastCleanTime = time(nullptr);

        return true;
    }

    void ClientMRUCache::ScheduleCleaning()
    {
        // Schedule cleaning if it's been long enough since the previous one
        if(time(nullptr) >= m_lastCleanTime + m_cleanIntervalSeconds)
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
            pair<unordered_map<sockaddr_storage, time_t>::iterator, bool> const&
                result = m_recentClients.emplace(addr, time(nullptr));

            // Failed to insert because it is already inserted
            if(!result.second)
            {
                // check whether client is expired or not
                if(time(nullptr) >= result.second + m_lifetimeSeconds)
                {
                    // Expired: return false, update time
                    response = false;
                    result.first->second = time(nullptr);
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
        lock_guard<mutex> lock(m_mutex);
        return (m_recentClients.find(addr) != m_recentClients.end());
    }

    void ClientMRUCache::Add(sockaddr_storage& addr)
    {
        lock_guard<mutex> lock(m_mutex);
        // Don't check whether the insertion succeeded or not because it's not
        // important in this case.
        m_recentClients.emplace(addr, time(nullptr));

    }
}

