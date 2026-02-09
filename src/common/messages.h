#pragma once

#include <string>

// Outgoing order from OMS -> venue (line-based text protocol)
struct NewOrder {
    int client_id;
    std::string symbol;
    std::string side; // "BUY" or "SELL"
    int qty;
    double price;
};

std::string format_new(const NewOrder& o);
std::string format_cancel(int client_id);

// Incoming message from venue -> OMS
enum class MsgKind {
    Ack,
    Fill,
    Cancelled,
    Reject,
    Unknown
};

// Parsed message (which fields are meaningful depends on MsgKind)
struct Msg {
    MsgKind kind = MsgKind::Unknown;

    int client_id = 0;
    int venue_id = 0;

    // Fill fields
    int qty = 0;
    double price = 0.0;
    char liquidity = '?';

    // Reject fields
    std::string reason;
};

// Parse a single line (without '\n') into a Msg
Msg parse_msg(const std::string& line);
