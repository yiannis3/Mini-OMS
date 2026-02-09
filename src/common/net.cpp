#include "net.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

int tcp_listen_loopback(int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
        return -1;
    }

    int yes = 1;
    // Allow quick restart after close() without waiting on TIME_WAIT
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << std::strerror(errno) << "\n";
        ::close(fd);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind() failed: " << std::strerror(errno) << "\n";
        ::close(fd);
        return -1;
    }

    if (::listen(fd, 1) < 0) {
        std::cerr << "listen() failed: " << std::strerror(errno) << "\n";
        ::close(fd);
        return -1;
    }

    return fd;
}

int tcp_accept(int listen_fd) {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    int cfd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (cfd < 0) {
        std::cerr << "accept() failed: " << std::strerror(errno) << "\n";
        return -1;
    }
    return cfd;
}

int tcp_connect_ipv4(const char* ip, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        std::cerr << "inet_pton failed for " << ip << "\n";
        ::close(fd);
        return -1;
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "connect() failed: " << std::strerror(errno) << "\n";
        ::close(fd);
        return -1;
    }

    return fd;
}

bool read_line(int fd, std::string& out) {
    out.clear();
    char ch;
    while (true) {
        // Read until newline to get one complete message
        ssize_t n = ::recv(fd, &ch, 1, 0);
        if (n == 0) return false;
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "recv() failed: " << std::strerror(errno) << "\n";
            return false;
        }
        if (ch == '\n') break;
        out.push_back(ch);
    }
    return true;
}

bool write_all(int fd, const std::string& s) {
    const char* p = s.data();
    size_t left = s.size();
    while (left > 0) {
        ssize_t n = ::send(fd, p, left, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "send() failed: " << std::strerror(errno) << "\n";
            return false;
        }
        p += n;
        left -= static_cast<size_t>(n);
    }
    return true;
}
