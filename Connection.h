#pragma once

#include <string>
#include <cstdint>
#include <cstring>
#include <cerrno>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace fx
{

class Connection
{
public:
    Connection() = default;
    ~Connection() { disconnect(); }

    bool connect(const char* host = "127.0.0.1", uint16_t port = 30698)
    {
        m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_fd < 0) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, host, &addr.sin_addr) <= 0)
        {
            ::close(m_fd);
            m_fd = -1;
            return false;
        }

        if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        {
            ::close(m_fd);
            m_fd = -1;
            return false;
        }

        m_connected = true;
        return true;
    }

    void disconnect()
    {
        if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
        m_connected = false;
    }

    bool isConnected() const { return m_connected; }

    bool send(const std::string& json)
    {
        if (!m_connected) return false;
        uint32_t len = htonl(static_cast<uint32_t>(json.size()));
        if (!writeAll(&len, 4)) return setError();
        if (!writeAll(json.data(), json.size())) return setError();
        return true;
    }

    bool receive(std::string& out)
    {
        if (!m_connected) return false;
        uint32_t lenNet = 0;
        if (!readAll(&lenNet, 4)) return setError();
        uint32_t len = ntohl(lenNet);
        if (len == 0 || len > 4 * 1024 * 1024) return setError();
        out.resize(len);
        if (!readAll(out.data(), len)) return setError();
        return true;
    }

private:
    bool setError() { m_connected = false; return false; }

    bool writeAll(const void* data, size_t n)
    {
        const char* p = static_cast<const char*>(data);
        while (n > 0)
        {
            ssize_t w = ::write(m_fd, p, n);
            if (w <= 0) return false;
            p += w; n -= static_cast<size_t>(w);
        }
        return true;
    }

    bool readAll(void* data, size_t n)
    {
        char* p = static_cast<char*>(data);
        while (n > 0)
        {
            ssize_t r = ::read(m_fd, p, n);
            if (r <= 0) return false;
            p += r; n -= static_cast<size_t>(r);
        }
        return true;
    }

    int m_fd = -1;
    bool m_connected = false;
};

}
