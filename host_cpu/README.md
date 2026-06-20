# host — software reference order book

The C++ "answer key" for the project. It reconstructs the Nasdaq limit order
book in software and validates itself against **LOBSTER** ground-truth data,
event by event. Once this reference is trusted, it becomes what the FPGA book
must match (over UART).

## Files

- `src/OrderBook.hpp` — the order book reference. Integer prices/sizes, ordered
  `std::map` per side, `std::unordered_map` from order-id to resting order.
  Handles LOBSTER message types: 1 new, 2 partial cancel, 3 delete,
  4 visible execution; 5/6/7 leave the visible book unchanged.
- `src/verify.cpp` — replays a LOBSTER message file through `OrderBook` and, after
  every message, compares the top-N levels against the matching row of the
  orderbook file. Stops at the first mismatch and prints the offending event.
- `test/` — a tiny hand-computed scenario proving the harness works without any
  downloaded data.

## Build & self-test

```sh
cd host
make test      # builds, runs the synthetic check -> "OK: 7 rows matched"
```

## Validate against LOBSTER

Download a free sample from lobsterdata.com (AAPL, AMZN, GOOG, INTC, MSFT). You
get two row-aligned CSVs per ticker/day — a `_message_` file and an `_orderbook_`
file at some level depth (1/5/10/50).

```sh
make verify
./verify  AAPL_..._message_5.csv  AAPL_..._orderbook_5.csv  5
```

`OK: N rows matched` means the reference book is bit-for-bit identical to LOBSTER
at every event. A mismatch prints the row, level, field, and the message that
broke it — so you know exactly which handler to fix.

## Notes

- LOBSTER prices are dollars × 10000 (integers); empty levels use the sentinels
  `9999999999` (ask) and `-9999999999` (bid), which this code reproduces so the
  comparison is exact.
- LOBSTER messages are decoded CSV, not raw binary ITCH. This validates the book
  *logic*. The raw ITCH 5.0 binary parser (for the FPGA path) must be tested
  separately against a real `.gz` feed from Nasdaq's emi FTP.
