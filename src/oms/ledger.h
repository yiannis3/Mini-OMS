#pragma once

#include <fstream>
#include <string>

#include "oms/orders.h"

// Append-only CSV ledger of fills (single-writer)
class Ledger {
public:
    // Opens file and writes header if needed
    bool open(const std::string& path);

    void on_fill(
        long long ts_us,
        int client_id,
        int venue_id,
        const std::string& symbol,
        Side side,
        int qty,
        double price,
        int position_after
    );

private:
    // Simple append-only stream
    std::ofstream out_;
};
