# mini-oms (C++17)

A simple demo of a participant-side order management system (OMS) talking to a simulated venue over TCP.

It implements:
- Interactive OMS CLI (BUY/SELL/CANCEL/STATUS)
- Text protocol (`NEW`, `ACK`, `FILL`, `CANCEL`, `CANCELLED`, `REJECT`)
- Order state tracking (Accepted/Filled/Cancelled/Rejected)
- Participant-side risk checks before sending orders
- Position tracking + average cost + realized PnL
- CSV ledger of fills (`fills.csv`)

---

## Requirements

- Linux (tested on Ubuntu 24.04)
- g++ (C++17)
- CMake
- Ninja (optional but recommended)

Install:

```bash
sudo apt install -y build-essential cmake ninja-build
````

---

## Build

From the repo root:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

This produces:

* `./build/venue_sim`
* `./build/oms`

---

## Run

Open two terminals in the repo root.

### Terminal A (venue)

```bash
./build/venue_sim
```

Example output:

```text
venue_sim: listening on 127.0.0.1:9001
venue_sim: client connected
```

### Terminal B (OMS)

```bash
./build/oms
```

Example output:

```text
oms: connected to 127.0.0.1:9001
oms: ledger=fills.csv
```

---

## OMS Commands

Inside `oms`, you can type:

### Place orders

```text
BUY <qty> <price>
SELL <qty> <price>
```

Example:

```text
BUY 10 100
SELL 4 105
```

### Cancel orders

```text
CANCEL <client_id>
```

Example:

```text
CANCEL 1001
```

### Status

```text
STATUS
```

Shows:

* current position
* avg_cost
* realized_pnl
* open_orders
* risk limits

### Exit

```text
exit
```

---

## Example Session (PnL)

```text
STATUS
BUY 10 100
STATUS
SELL 4 105
STATUS
SELL 10 110
STATUS
```

Expected behavior:

* After `BUY 10 100`: position = 10, avg_cost = 100, realized_pnl = 0
* After `SELL 4 105`: position = 6, avg_cost = 100, realized_pnl = (105-100)*4 = 20
* After `SELL 10 110`: closes remaining 6 long (adds 60) and opens 4 short at 110:

  * position = -4
  * avg_cost = 110
  * realized_pnl = 80

---

## Risk Checks (participant-side)

Before sending `NEW` to the venue, OMS checks:

* `max_order_qty` (default: 100)
* `max_notional` = qty * price (default: 50,000)
* `max_open_orders` (default: 50)
* `max_abs_position` (default: 200)

If a check fails:

* OMS marks the order `Rejected` with reason `RISK_<REASON>`
* OMS does NOT send the order to the venue

Example (qty too large):

```text
BUY 100000 101.25
```

Expected:

```text
oms: RISK_REJECT client_id=... reason=RISK_MAX_ORDER_QTY
```

---

## Ledger Output

OMS appends one row per fill to `fills.csv` (in the repo root).

Format:

```text
ts_us,client_id,venue_id,symbol,side,qty,price,position_after
```

View it from your shell (not inside OMS):

```bash
cat fills.csv
```

---

## Text Protocol (line-based)

OMS → Venue:

* `NEW <client_id> <symbol> <BUY|SELL> <qty> <price>`
* `CANCEL <client_id>`

Venue → OMS:

* `ACK <client_id> <venue_id>`
* `FILL <client_id> <venue_id> <qty> <price> <A|P>`
* `CANCELLED <client_id> <venue_id>`
* `REJECT <client_id> <reason>`

Note:
IDs are demo values: `client_id` starts at 1001 (OMS) and `venue_id` starts at 90001 (venue), then increment per order.
