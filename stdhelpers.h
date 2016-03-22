#include <netinet/in.h>
#include <string.h>

#pragma once

#define COUNT_OF(x) (sizeof((x))/sizeof((x)[0]))

namespace std
{
    // Note: this only compares AF_FAMILY and address fields.
    inline constexpr bool operator==(const sockaddr_storage& lhs, const sockaddr_storage& rhs)
    {
        if(&lhs == nullptr || &rhs == nullptr)
        {
            // Don't crash if someone tricks the compiler.
            return false;
        }

        // Compare address families
        if(lhs.ss_family != rhs.ss_family)
        {
            return false;
        }

        // Compare addresses
        if(lhs.ss_family == AF_INET)
        {
            return (0 == memcmp(&(((const sockaddr_in &)lhs).sin_addr),
                                &(((const sockaddr_in &)rhs).sin_addr),
                                sizeof(((const sockaddr_in &)lhs).sin_addr)));
        }
        else if (lhs.ss_family == AF_INET6)
        {
            return (0 == memcmp(&(((sockaddr_in6 const&)lhs).sin6_addr),
                                &(((sockaddr_in6 const&)rhs).sin6_addr),
                                sizeof(((sockaddr_in6 const&)lhs).sin6_addr)));
        }

        // Why are you comparing sockaddrs that are not ip4 or ip6?
        return false;
    }

    inline constexpr bool operator!=(const sockaddr_storage& lhs, const sockaddr_storage& rhs)
    {
        return !(operator==(lhs, rhs));
    }



    // Create a specialization of hash that supports sockaddr_storage
    // Note: this only hashes the address field, not the port or other fields.
    template<>
    struct hash<sockaddr_storage>
    {
        typedef sockaddr_storage argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& s) const
        {
            result_type result = 0L;
            // Only supports ip4 and ip6 addresses
            if(s.ss_family == AF_INET)
            {
                result = std::hash<unsigned int>()(((sockaddr_in const&) s).sin_addr.s_addr);
            }
            else if(s.ss_family == AF_INET6)
            {
                for(unsigned int i = 0; i < COUNT_OF(((sockaddr_in6 const&) s).sin6_addr.s6_addr32); i++)
                {
                    result = result ^ ((std::hash<unsigned int>()(((sockaddr_in6 const&) s).sin6_addr.s6_addr32[i])) << i);
                }
            }

            // Yep, doesn't hash non-ip4/ip6 addresses
            return result;
        }
    };

    // Test if the IP address in a sockaddr is IP4
    // default to false
    inline bool IsAddressIp4(const sockaddr_storage& ss)
    {
        if(ss.ss_family == AF_INET)
        {
            return true;
        }
        else if(ss.ss_family == AF_INET6)
        {
            unsigned char nullPrefix[12] = {0};
            // Check for IPv6-Mapped-IPv4 address
            if(0 == memcmp(&(((const sockaddr_in6&)ss).sin6_addr.s6_addr),
                           nullPrefix,
                           sizeof(nullPrefix)))
            {
                return true;
            }

            return false;
        }
        else
        {
            // This shouldn't happen.
            return false;
        }
    }

}
