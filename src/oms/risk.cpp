#include "oms/risk.h"

#include <cmath>

std::string check_new_order(
    const RiskConfig& cfg,
    const OrderStore& store,
    const PositionTracker& pos,
    Side side,
    int qty,
    double price
) {
    if (qty <= 0 || price <= 0.0) return "BAD_INPUT";

    if (qty > cfg.max_order_qty) return "MAX_ORDER_QTY";

    double notional = (double)qty * price;
    if (notional > cfg.max_notional) return "MAX_NOTIONAL";

    // In our OMS flow we already inserted the order as PendingNew
    int open_now = store.open_orders_count();
    if (open_now > cfg.max_open_orders) return "MAX_OPEN_ORDERS";

    int current_pos = pos.position();
    // Simple check: only current position, not outstanding open orders
    int new_pos = current_pos + ((side == Side::Buy) ? qty : -qty);
    if (std::abs(new_pos) > cfg.max_abs_position) return "MAX_POSITION";

    return "";
}
