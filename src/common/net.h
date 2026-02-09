#pragma once

#include <string>

int tcp_listen_loopback(int port);
int tcp_accept(int listen_fd);
int tcp_connect_ipv4(const char* ip, int port);

// Reads from the socket until '\n', returns false on EOF or error
bool read_line(int fd, std::string& out);
bool write_all(int fd, const std::string& s);
