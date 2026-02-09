#include "oms/ledger.h"

#include <fstream>
#include <iostream>

static bool file_empty_or_missing(const std::string& path) {
    std::ifstream in(path);
    if (!in.good()) return true;
    int c = in.peek();
    return c == std::ifstream::traits_type::eof();
}

bool Ledger::open(const std::string& path) {
    bool need_header = file_empty_or_missing(path);

    out_.open(path, std::ios::out | std::ios::app);
    if (!out_) {
        std::cerr << "oms: failed to open ledger file: " << path << "\n";
        return false;
    }

    if (need_header) {
        out_ << "ts_us,client_id,venue_id,symbol,side,qty,price,position_after\n";
        out_.flush();
    }

    return true;
}

void Ledger::on_fill(
    long long ts_us,
    int client_id,
    int venue_id,
    const std::string& symbol,
    Side side,
    int qty,
    double price,
    int position_after
) {
    if (!out_) return;

    out_ << ts_us << ","
         << client_id << ","
         << venue_id << ","
         << symbol << ","
         << to_string(side) << ","
         << qty << ","
         << price << ","
         << position_after
         << "\n";

    // Flush to make the ledger usable during a live run
    out_.flush();
}
