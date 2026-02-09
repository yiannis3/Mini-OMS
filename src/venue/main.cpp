#include "common/net.h"
#include "common/messages.h"

#include <poll.h>
#include <sys/time.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

static long long now_us() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return (long long)tv.tv_sec * 1000000LL + (long long)tv.tv_usec;
}

struct LiveOrder {
    int client_id = 0;
    int venue_id = 0;
    int qty = 0;
    double price = 0.0;

    bool cancelled = false;
    bool filled = false;
};

struct ScheduledFill {
    long long due_us = 0;
    int client_id = 0;
};

static void send_reject(int fd, int client_id, const std::string& reason) {
    std::ostringstream oss;
    oss << "REJECT " << client_id << " " << reason << "\n";
    write_all(fd, oss.str());
    std::cout << "venue_sim: sent: " << oss.str();
}

int main() {
    const int port = 9001;
    // Fixed delay to keep fills predictable for the demo
    const long long FILL_DELAY_US = 500000; // 0.5s

    int lfd = tcp_listen_loopback(port);
    if (lfd < 0) return 1;

    std::cout << "venue_sim: listening on 127.0.0.1:" << port << "\n";

    int cfd = tcp_accept(lfd);
    if (cfd < 0) {
        ::close(lfd);
        return 1;
    }

    std::cout << "venue_sim: client connected\n";

    int next_venue_id = 90001;

    std::unordered_map<int, LiveOrder> orders; // client_id -> order
    std::vector<ScheduledFill> schedule; // Due times for scheduled full fills

    pollfd pfd;
    pfd.fd = cfd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    while (true) {
        // Compute poll timeout based on next scheduled fill (wall clock)
        int timeout_ms = -1;
        if (!schedule.empty()) {
            long long tnow = now_us();
            long long next_due = schedule[0].due_us;
            for (size_t i = 1; i < schedule.size(); i++) {
                if (schedule[i].due_us < next_due) next_due = schedule[i].due_us;
            }

            long long delta_us = next_due - tnow;
            if (delta_us < 0) delta_us = 0;
            timeout_ms = (int)(delta_us / 1000);
        }

        int rc = ::poll(&pfd, 1, timeout_ms);
        if (rc < 0) {
            if (errno == EINTR) continue;
            std::cerr << "venue_sim: poll failed\n";
            break;
        }

        // Socket readable: handle one inbound line
        if (rc > 0 && (pfd.revents & POLLIN)) {
            std::string line;
            if (!read_line(cfd, line)) {
                std::cout << "venue_sim: client disconnected\n";
                break;
            }

            std::cout << "venue_sim: recv: " << line << "\n";

            std::istringstream iss(line);
            std::string kind;
            iss >> kind;

            if (kind == "NEW") {
                int client_id = 0;
                std::string symbol, side;
                int qty = 0;
                double price = 0.0;

                if (!(iss >> client_id >> symbol >> side >> qty >> price)) {
                    send_reject(cfd, 0, "BAD_FORMAT");
                    continue;
                }

                int venue_id = next_venue_id++;

                LiveOrder o;
                o.client_id = client_id;
                o.venue_id = venue_id;
                o.qty = qty;
                o.price = price;
                orders[client_id] = o;

                // ACK immediately
                {
                    std::ostringstream ack;
                    ack << "ACK " << client_id << " " << venue_id << "\n";
                    write_all(cfd, ack.str());
                    std::cout << "venue_sim: sent: " << ack.str();
                }

                // Schedule a single full fill after a short delay
                ScheduledFill sf;
                sf.due_us = now_us() + FILL_DELAY_US;
                sf.client_id = client_id;
                schedule.push_back(sf);
            }
            else if (kind == "CANCEL") {
                int client_id = 0;
                if (!(iss >> client_id)) {
                    send_reject(cfd, 0, "BAD_FORMAT");
                    continue;
                }

                auto it = orders.find(client_id);
                if (it == orders.end()) {
                    send_reject(cfd, client_id, "UNKNOWN_ORDER");
                    continue;
                }

                LiveOrder& o = it->second;

                if (o.filled) {
                    send_reject(cfd, client_id, "ALREADY_FILLED");
                    continue;
                }
                if (o.cancelled) {
                    send_reject(cfd, client_id, "ALREADY_CANCELLED");
                    continue;
                }

                o.cancelled = true;

                std::ostringstream msg;
                msg << "CANCELLED " << o.client_id << " " << o.venue_id << "\n";
                write_all(cfd, msg.str());
                std::cout << "venue_sim: sent: " << msg.str();
            }
            else {
                send_reject(cfd, 0, "UNKNOWN_MSG");
            }
        }

        // After poll process due fills (no partials)
        long long tnow = now_us();

        std::vector<ScheduledFill> remaining;
        remaining.reserve(schedule.size());

        for (size_t i = 0; i < schedule.size(); i++) {
            const ScheduledFill& s = schedule[i];
            if (s.due_us > tnow) {
                remaining.push_back(s);
                continue;
            }

            auto it = orders.find(s.client_id);
            if (it == orders.end()) {
                continue;
            }

            LiveOrder& o = it->second;
            if (o.cancelled || o.filled) {
                continue;
            }

            std::ostringstream fill;
            fill << "FILL " << o.client_id << " " << o.venue_id
                 << " " << o.qty << " " << o.price << " A\n";
            write_all(cfd, fill.str());
            std::cout << "venue_sim: sent: " << fill.str();

            o.filled = true;
        }

        schedule.swap(remaining);
        pfd.revents = 0;
    }

    ::close(cfd);
    ::close(lfd);
    return 0;
}
