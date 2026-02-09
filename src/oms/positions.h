#pragma once

#include "oms/orders.h"

// Tracks net position + average cost + realized pnl (one symbol ABC)
class PositionTracker {
public:
    void on_fill(Side side, int qty, double price);

    int position() const { return position_; }
    double avg_cost() const { return avg_cost_; }
    // Realized PnL only (unrealized is intentionally omitted)
    double realized_pnl() const { return realized_pnl_; }

private:
    int position_ = 0;         // >0 long, <0 short
    double avg_cost_ = 0.0;    // Avg entry of current net position
    double realized_pnl_ = 0.0;
};
