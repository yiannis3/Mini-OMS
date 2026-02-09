#include "oms/orders.h"

#include <iostream>
#include <stdexcept>

Side parse_side(const std::string& s) {
    if (s == "BUY") return Side::Buy;
    if (s == "SELL") return Side::Sell;
    throw std::runtime_error("invalid side: " + s);
}

const char* to_string(Side s) {
    return (s == Side::Buy) ? "BUY" : "SELL";
}

const char* to_string(OrderState st) {
    switch (st) {
        case OrderState::PendingNew:    return "PendingNew";
        case OrderState::Accepted:      return "Accepted";
        case OrderState::PendingCancel: return "PendingCancel";
        case OrderState::Cancelled:     return "Cancelled";
        case OrderState::Filled:        return "Filled";
        case OrderState::Rejected:      return "Rejected";
    }
    return "Unknown";
}

bool OrderStore::is_open_state(OrderState st) {
    return st == OrderState::PendingNew
        || st == OrderState::Accepted
        || st == OrderState::PendingCancel;
}

void OrderStore::add_pending_new(int client_id, const std::string& symbol, Side side, int qty, double price) {
    Order o;
    o.client_id = client_id;
    o.symbol = symbol;
    o.side = side;
    o.qty = qty;
    o.price = price;
    o.state = OrderState::PendingNew;

    orders_[client_id] = std::move(o);
}

void OrderStore::on_ack(int client_id, int venue_id) {
    auto it = orders_.find(client_id);
    if (it == orders_.end()) {
        std::cout << "oms: WARN ack for unknown client_id=" << client_id << "\n";
        return;
    }

    Order& o = it->second;
    if (o.state == OrderState::Rejected) return;

    o.venue_id = venue_id;

    // If we already requested cancel before ACK arrived, keep PendingCancel
    if (o.state == OrderState::PendingCancel) return;

    o.state = OrderState::Accepted;
}

void OrderStore::on_fill(int client_id, int venue_id, int fill_qty, double /*fill_price*/) {
    auto it = orders_.find(client_id);
    if (it == orders_.end()) {
        std::cout << "oms: WARN fill for unknown client_id=" << client_id << "\n";
        return;
    }

    Order& o = it->second;

    if (o.state == OrderState::Rejected) {
        std::cout << "oms: WARN fill for rejected client_id=" << client_id << "\n";
        return;
    }
    if (o.state == OrderState::Cancelled) {
        std::cout << "oms: WARN fill for cancelled client_id=" << client_id << "\n";
        return;
    }

    if (o.venue_id != -1 && o.venue_id != venue_id) {
        std::cout << "oms: WARN fill venue_id mismatch client_id=" << client_id
                  << " expected=" << o.venue_id << " got=" << venue_id << "\n";
    }

    o.venue_id = venue_id;
    o.filled_qty += fill_qty;

    if (o.filled_qty >= o.qty) {
        o.filled_qty = o.qty;
        o.state = OrderState::Filled;
    } else {
        o.state = OrderState::Accepted;
    }
}

bool OrderStore::request_cancel(int client_id) {
    auto it = orders_.find(client_id);
    if (it == orders_.end()) {
        std::cout << "oms: WARN cancel unknown client_id=" << client_id << "\n";
        return false;
    }

    Order& o = it->second;

    if (o.state == OrderState::Filled || o.state == OrderState::Cancelled || o.state == OrderState::Rejected) {
        std::cout << "oms: WARN cancel not allowed in state=" << to_string(o.state) << "\n";
        return false;
    }
    if (o.state == OrderState::PendingCancel) {
        std::cout << "oms: WARN cancel already pending client_id=" << client_id << "\n";
        return false;
    }

    // Allow cancel even before ACK
    o.state = OrderState::PendingCancel;
    return true;
}

void OrderStore::on_cancelled(int client_id, int venue_id) {
    auto it = orders_.find(client_id);
    if (it == orders_.end()) {
        std::cout << "oms: WARN cancelled unknown client_id=" << client_id << "\n";
        return;
    }

    Order& o = it->second;
    if (o.state == OrderState::Rejected) return;

    if (o.venue_id != -1 && o.venue_id != venue_id) {
        std::cout << "oms: WARN cancelled venue_id mismatch client_id=" << client_id << "\n";
    }

    o.venue_id = venue_id;
    o.state = OrderState::Cancelled;
}

void OrderStore::mark_rejected(int client_id, const std::string& reason) {
    auto it = orders_.find(client_id);
    if (it == orders_.end()) {
        std::cout << "oms: WARN reject unknown client_id=" << client_id << "\n";
        return;
    }

    Order& o = it->second;
    o.state = OrderState::Rejected;
    o.reject_reason = reason;
}

int OrderStore::open_orders_count() const {
    int n = 0;
    for (const auto& kv : orders_) {
        if (is_open_state(kv.second.state)) n++;
    }
    return n;
}

const Order* OrderStore::get(int client_id) const {
    auto it = orders_.find(client_id);
    if (it == orders_.end()) return nullptr;
    return &it->second;
}

void OrderStore::print_one(int client_id) const {
    auto it = orders_.find(client_id);
    if (it == orders_.end()) {
        std::cout << "oms: (no such order) client_id=" << client_id << "\n";
        return;
    }

    const Order& o = it->second;
    std::cout << "oms: order " << o.client_id
              << " " << o.symbol
              << " " << to_string(o.side)
              << " qty=" << o.qty
              << " px=" << o.price
              << " venue_id=" << o.venue_id
              << " filled=" << o.filled_qty
              << " state=" << to_string(o.state);

    if (o.state == OrderState::Rejected) {
        std::cout << " reason=" << o.reject_reason;
    }

    std::cout << "\n";
}
