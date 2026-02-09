#include "messages.h"

#include <sstream>

std::string format_new(const NewOrder& o) {
    std::ostringstream oss;
    // Trailing newline = one message per line
    oss << "NEW " << o.client_id << " " << o.symbol << " " << o.side
        << " " << o.qty << " " << o.price << "\n";
    return oss.str();
}

std::string format_cancel(int client_id) {
    std::ostringstream oss;
    oss << "CANCEL " << client_id << "\n";
    return oss.str();
}

Msg parse_msg(const std::string& line) {
    Msg m;

    std::istringstream iss(line);
    std::string kind;
    if (!(iss >> kind)) {
        // If we can't read a kind, treat as Unknown
        return m; // Unknown
    }

    if (kind == "ACK") {
        m.kind = MsgKind::Ack;
        iss >> m.client_id >> m.venue_id;
        return m;
    }

    if (kind == "FILL") {
        m.kind = MsgKind::Fill;
        iss >> m.client_id >> m.venue_id >> m.qty >> m.price >> m.liquidity;
        return m;
    }

    if (kind == "CANCELLED") {
        m.kind = MsgKind::Cancelled;
        iss >> m.client_id >> m.venue_id;
        return m;
    }

    if (kind == "REJECT") {
        m.kind = MsgKind::Reject;
        iss >> m.client_id;
        std::getline(iss, m.reason);
        if (!m.reason.empty() && m.reason[0] == ' ') {
            m.reason.erase(0, 1);
        }
        return m;
    }

    // Unrecognized message kind
    return m;
}
