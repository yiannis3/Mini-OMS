#include "oms/positions.h"

#include <algorithm>

void PositionTracker::on_fill(Side side, int qty, double price) {
    if (qty <= 0) return;

    // No fees/slippage modeled, PnL uses raw fill price
    int pos = position_;

    // If position is flat, this fill simply opens a new position
    if (pos == 0) {
        if (side == Side::Buy) position_ = qty;
        else position_ = -qty;
        avg_cost_ = price;
        return;
    }

    // LONG case
    if (pos > 0) {
        if (side == Side::Buy) {
            // Increase long: weighted average
            int new_pos = pos + qty;
            avg_cost_ = (avg_cost_ * pos + price * qty) / (double)new_pos;
            position_ = new_pos;
            return;
        } else {
            // Sell reduces or flips
            int close_qty = std::min(qty, pos);
            realized_pnl_ += (price - avg_cost_) * (double)close_qty;

            int remaining_long = pos - close_qty;
            int leftover_sell = qty - close_qty;

            if (remaining_long > 0) {
                position_ = remaining_long; // Still long
                return;
            }

            // Now flat
            position_ = 0;
            avg_cost_ = 0.0;

            if (leftover_sell > 0) {
                // Flip to short with leftover
                position_ = -leftover_sell;
                avg_cost_ = price;
            }
            return;
        }
    }

    // SHORT case (pos < 0)
    int abs_pos = -pos;

    if (side == Side::Sell) {
        // Increase short: weighted average entry
        int new_abs = abs_pos + qty;
        avg_cost_ = (avg_cost_ * abs_pos + price * qty) / (double)new_abs;
        position_ = -new_abs;
        return;
    } else {
        // Buy reduces or flips
        int close_qty = std::min(qty, abs_pos);
        realized_pnl_ += (avg_cost_ - price) * (double)close_qty;

        int remaining_short = abs_pos - close_qty;
        int leftover_buy = qty - close_qty;

        if (remaining_short > 0) {
            position_ = -remaining_short; // Still short
            return;
        }

        // Now flat
        position_ = 0;
        avg_cost_ = 0.0;

        if (leftover_buy > 0) {
            // Flip to long with leftover
            position_ = leftover_buy;
            avg_cost_ = price;
        }
        return;
    }
}
