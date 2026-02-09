#pragma once

#include <string>
#include <unordered_map>

enum class Side { Buy, Sell };

enum class OrderState {
    PendingNew,
    Accepted,
    PendingCancel,
    Cancelled,
    Filled,
    Rejected
};

struct Order {
    int client_id = 0;
    std::string symbol;
    Side side = Side::Buy;
    int qty = 0;
    double price = 0.0;

    int venue_id = -1;
    int filled_qty = 0;
    OrderState state = OrderState::PendingNew;

    std::string reject_reason; // Set when rejected
};

Side parse_side(const std::string& s); // "BUY"/"SELL" -> Side
const char* to_string(Side s);
const char* to_string(OrderState st);

class OrderStore {
public:
    void add_pending_new(int client_id, const std::string& symbol, Side side, int qty, double price);

    void on_ack(int client_id, int venue_id);
    void on_fill(int client_id, int venue_id, int fill_qty, double fill_price);

    bool request_cancel(int client_id);
    void on_cancelled(int client_id, int venue_id);

    void mark_rejected(int client_id, const std::string& reason);

    int open_orders_count() const;       // PendingNew + Accepted + PendingCancel
    const Order* get(int client_id) const;

    void print_one(int client_id) const;

private:
    static bool is_open_state(OrderState st);

    // Keyed by client_id (OMS-assigned
    std::unordered_map<int, Order> orders_;
};
