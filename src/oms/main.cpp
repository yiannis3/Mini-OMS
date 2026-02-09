#include "common/net.h"
#include "common/messages.h"
#include "oms/orders.h"
#include "oms/positions.h"
#include "oms/risk.h"
#include "oms/ledger.h"

#include <poll.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

static void trim_crlf(std::string& s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
}

static long long now_us() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return (long long)tv.tv_sec * 1000000LL + (long long)tv.tv_usec;
}

static void print_status(const OrderStore& store, const PositionTracker& pos, const RiskConfig& cfg) {
    std::cout << "oms: STATUS\n";
    std::cout << "  position(ABC)=" << pos.position() << "\n";
    std::cout << "  avg_cost(ABC)=" << pos.avg_cost() << "\n";
    std::cout << "  realized_pnl=" << pos.realized_pnl() << "\n";
    std::cout << "  open_orders=" << store.open_orders_count() << "\n";
    std::cout << "  limits: max_order_qty=" << cfg.max_order_qty
              << " max_notional=" << cfg.max_notional
              << " max_open_orders=" << cfg.max_open_orders
              << " max_abs_position=" << cfg.max_abs_position
              << "\n";
}

int main() {
    const char* ip = "127.0.0.1";
    const int port = 9001;

    int fd = tcp_connect_ipv4(ip, port);
    if (fd < 0) return 1;

    std::cout << "oms: connected to " << ip << ":" << port << "\n";
    std::cout << "oms: commands:\n";
    std::cout << "  BUY <qty> <price>\n";
    std::cout << "  SELL <qty> <price>\n";
    std::cout << "  CANCEL <client_id>\n";
    std::cout << "  STATUS\n";
    std::cout << "  exit\n";

    RiskConfig risk_cfg;
    int next_id = 1001;

    OrderStore store;
    PositionTracker pos;

    Ledger ledger;
    if (!ledger.open("fills.csv")) {
        std::cerr << "oms: cannot continue without ledger\n";
        ::close(fd);
        return 1;
    }
    std::cout << "oms: ledger=fills.csv\n";

    pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = fd;
    fds[1].events = POLLIN;

    while (true) {
        // Single-threaded: poll stdin + venue socket
        int rc = ::poll(fds, 2, -1);
        if (rc < 0) {
            if (errno == EINTR) continue;
            std::cerr << "poll() failed: " << std::strerror(errno) << "\n";
            break;
        }

        // ---- stdin ----
        if (fds[0].revents & POLLIN) {
            std::string cmd;
            if (!std::getline(std::cin, cmd)) {
                std::cout << "oms: stdin closed, exiting\n";
                break;
            }

            trim_crlf(cmd);
            if (cmd.empty()) continue;

            if (cmd == "exit" || cmd == "quit") {
                std::cout << "oms: exiting\n";
                break;
            }

            std::istringstream iss(cmd);
            std::string kind;
            iss >> kind;

            if (kind == "STATUS") {
                std::string extra;
                if (iss >> extra) {
                    std::cout << "oms: invalid. STATUS takes no args\n";
                } else {
                    print_status(store, pos, risk_cfg);
                }
                continue;
            }

            if (kind == "BUY" || kind == "SELL") {
                int qty = 0;
                double price = 0.0;
                if (!(iss >> qty >> price) || qty <= 0 || price <= 0.0) {
                    std::cout << "oms: invalid. expected: BUY 10 101.25\n";
                    continue;
                }

                std::string extra;
                if (iss >> extra) {
                    std::cout << "oms: invalid. unexpected extra token: " << extra << "\n";
                    continue;
                }

                int client_id = next_id++;
                Side side_enum = parse_side(kind);

                // Store first so we can print/reject consistently
                store.add_pending_new(client_id, "ABC", side_enum, qty, price);

                // Participant-side risk gate before sending to the venue
                std::string reason = check_new_order(risk_cfg, store, pos, side_enum, qty, price);
                if (!reason.empty()) {
                    store.mark_rejected(client_id, "RISK_" + reason);
                    std::cout << "oms: RISK_REJECT client_id=" << client_id
                              << " reason=" << ("RISK_" + reason) << "\n";
                    store.print_one(client_id);
                    continue;
                }

                // Send NEW
                NewOrder o;
                o.client_id = client_id;
                o.symbol = "ABC";
                o.side = kind;
                o.qty = qty;
                o.price = price;

                std::string wire = format_new(o);
                if (!write_all(fd, wire)) {
                    std::cerr << "oms: failed to send NEW\n";
                    break;
                }
                std::cout << "oms: sent: " << wire;
                continue;
            }

            if (kind == "CANCEL") {
                int client_id = 0;
                if (!(iss >> client_id) || client_id <= 0) {
                    std::cout << "oms: invalid. expected: CANCEL 1001\n";
                    continue;
                }

                std::string extra;
                if (iss >> extra) {
                    std::cout << "oms: invalid. unexpected extra token: " << extra << "\n";
                    continue;
                }

                if (!store.request_cancel(client_id)) {
                    store.print_one(client_id);
                    continue;
                }

                std::string wire = format_cancel(client_id);
                if (!write_all(fd, wire)) {
                    std::cerr << "oms: failed to send CANCEL\n";
                    break;
                }
                std::cout << "oms: sent: " << wire;
                store.print_one(client_id);
                continue;
            }

            std::cout << "oms: unknown command\n";
        }

        // Venue socket
        if (fds[1].revents & POLLIN) {
            std::string line;
            if (!read_line(fd, line)) {
                std::cerr << "oms: venue disconnected\n";
                break;
            }

            Msg m = parse_msg(line);

            switch (m.kind) {
                case MsgKind::Ack: {
                    std::cout << "oms: ACK client_id=" << m.client_id
                              << " venue_id=" << m.venue_id << "\n";
                    store.on_ack(m.client_id, m.venue_id);
                    store.print_one(m.client_id);
                    break;
                }
                case MsgKind::Fill: {
                    std::cout << "oms: FILL client_id=" << m.client_id
                              << " venue_id=" << m.venue_id
                              << " qty=" << m.qty
                              << " price=" << m.price
                              << " liq=" << m.liquidity << "\n";

                    const Order* o = store.get(m.client_id);
                    if (!o) {
                        std::cout << "oms: WARN fill for unknown order, cannot update pnl/ledger\n";
                    } else {
                        pos.on_fill(o->side, m.qty, m.price);

                        ledger.on_fill(
                            now_us(),
                            m.client_id,
                            m.venue_id,
                            o->symbol,
                            o->side,
                            m.qty,
                            m.price,
                            pos.position()
                        );

                        std::cout << "oms: position=" << pos.position()
                                  << " avg_cost=" << pos.avg_cost()
                                  << " realized_pnl=" << pos.realized_pnl()
                                  << "\n";
                    }

                    store.on_fill(m.client_id, m.venue_id, m.qty, m.price);
                    store.print_one(m.client_id);
                    break;
                }
                case MsgKind::Cancelled: {
                    std::cout << "oms: CANCELLED client_id=" << m.client_id
                              << " venue_id=" << m.venue_id << "\n";
                    store.on_cancelled(m.client_id, m.venue_id);
                    store.print_one(m.client_id);
                    break;
                }
                case MsgKind::Reject: {
                    std::cout << "oms: REJECT client_id=" << m.client_id
                              << " reason=" << m.reason << "\n";
                    if (m.client_id > 0) {
                        store.mark_rejected(m.client_id, "VENUE_" + m.reason);
                        store.print_one(m.client_id);
                    }
                    break;
                }
                default: {
                    std::cout << "oms: recv(unparsed): " << line << "\n";
                    break;
                }
            }
        }
    }

    ::close(fd);
    return 0;
}
