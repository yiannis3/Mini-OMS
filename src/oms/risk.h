#pragma once

#include <string>

#include "oms/orders.h"
#include "oms/positions.h"

struct RiskConfig {
    int max_order_qty = 100;
    double max_notional = 50'000.0;
    int max_open_orders = 50;
    int max_abs_position = 200;
};

// Return "" if OK, otherwise a reason like "MAX_NOTIONAL"
// Simple point-in-time checks only (no market data or fee modeling)
std::string check_new_order(
    const RiskConfig& cfg,
    const OrderStore& store,
    const PositionTracker& pos,
    Side side,
    int qty,
    double price
);
