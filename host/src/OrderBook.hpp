// OrderBook.hpp — software reference limit order book for Nasdaq ITCH / LOBSTER.
//
// This is the "answer key" the FPGA must match. It is intentionally simple and
// readable rather than fast: correctness first. Prices and sizes are integers
// (LOBSTER prices are dollars * 10000).
//
// Side convention follows LOBSTER: +1 = buy (bid), -1 = sell (ask).
#pragma once
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

class OrderBook {
public:
    using Price = int64_t;
    using Size  = int64_t;
    using Oid   = int64_t;

    // LOBSTER sentinels for empty levels (so output matches the orderbook file).
    static constexpr Price EMPTY_ASK_PRICE =  9999999999LL;
    static constexpr Price EMPTY_BID_PRICE = -9999999999LL;

    // Apply one LOBSTER message. type/oid/size/price/dir are taken straight
    // from the message-file columns. Returns false on an inconsistency that
    // should never happen on clean data (e.g. cancel of an unknown order).
    bool apply(int type, Oid oid, Size size, Price price, int dir) {
        switch (type) {
            case 1: return addOrder(oid, size, price, dir);      // new limit order
            case 2: return reduceOrder(oid, size);               // partial cancel
            case 3: return deleteOrder(oid);                     // full delete
            case 4: return reduceOrder(oid, size);               // visible execution
            case 5: return true;                                 // hidden exec: no book change
            case 6: return true;                                 // cross trade: no visible change
            case 7: return true;                                 // halt: no book change
            default: return true;                                // unknown: ignore
        }
    }

    // Top-of-book helpers.
    bool hasBid() const { return !bids_.empty(); }
    bool hasAsk() const { return !asks_.empty(); }
    Price bestBid() const { return bids_.empty() ? EMPTY_BID_PRICE : bids_.rbegin()->first; }
    Price bestAsk() const { return asks_.empty() ? EMPTY_ASK_PRICE : asks_.begin()->first; }

    // Fill ask/bid price+size for the top `levels` levels in LOBSTER column order.
    // Empty levels get the sentinel price and size 0.
    void snapshot(int levels, std::vector<Price>& askP, std::vector<Size>& askS,
                  std::vector<Price>& bidP, std::vector<Size>& bidS) const {
        askP.assign(levels, EMPTY_ASK_PRICE); askS.assign(levels, 0);
        bidP.assign(levels, EMPTY_BID_PRICE); bidS.assign(levels, 0);
        int i = 0;                                   // asks: ascending price
        for (auto it = asks_.begin(); it != asks_.end() && i < levels; ++it, ++i) {
            askP[i] = it->first; askS[i] = it->second;
        }
        i = 0;                                       // bids: descending price
        for (auto it = bids_.rbegin(); it != bids_.rend() && i < levels; ++it, ++i) {
            bidP[i] = it->first; bidS[i] = it->second;
        }
    }

private:
    struct Resting { Price price; Size size; int dir; };

    bool addOrder(Oid oid, Size size, Price price, int dir) {
        levelFor(dir)[price] += size;
        orders_[oid] = Resting{price, size, dir};
        return true;
    }

    bool reduceOrder(Oid oid, Size size) {
        auto it = orders_.find(oid);
        if (it == orders_.end()) return false;       // unknown order
        Resting& r = it->second;
        r.size -= size;
        auto& lvl = levelFor(r.dir);
        auto lit = lvl.find(r.price);
        if (lit != lvl.end()) {
            lit->second -= size;
            if (lit->second <= 0) lvl.erase(lit);
        }
        if (r.size <= 0) orders_.erase(it);
        return true;
    }

    bool deleteOrder(Oid oid) {
        auto it = orders_.find(oid);
        if (it == orders_.end()) return false;
        Resting& r = it->second;
        auto& lvl = levelFor(r.dir);
        auto lit = lvl.find(r.price);
        if (lit != lvl.end()) {
            lit->second -= r.size;
            if (lit->second <= 0) lvl.erase(lit);
        }
        orders_.erase(it);
        return true;
    }

    std::map<Price, Size>& levelFor(int dir) { return dir > 0 ? bids_ : asks_; }

    std::map<Price, Size> bids_;                     // ordered: rbegin = best bid
    std::map<Price, Size> asks_;                     // ordered: begin  = best ask
    std::unordered_map<Oid, Resting> orders_;
};
