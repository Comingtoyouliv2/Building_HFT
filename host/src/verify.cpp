// verify.cpp — validate the OrderBook reference against LOBSTER ground truth.
//
// LOBSTER gives two row-aligned CSV files for a ticker/day:
//   * message  file: one event per line  -> Time,Type,OrderID,Size,Price,Direction
//   * orderbook file: book state AFTER that event -> AskP1,AskS1,BidP1,BidS1,...
//
// We replay the message file through OrderBook and, after each message, compare
// the top-N levels against the orderbook file's matching row. The first mismatch
// is printed with full context so you know exactly which event broke the book.
//
// Usage: verify <message.csv> <orderbook.csv> <levels>
#include "OrderBook.hpp"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<int64_t> parseRow(const std::string& line) {
    std::vector<int64_t> out;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, ',')) {
        if (cell.empty()) continue;
        // LOBSTER time has decimals; book columns are integers. Truncate at '.'.
        size_t dot = cell.find('.');
        out.push_back(std::stoll(dot == std::string::npos ? cell : cell.substr(0, dot)));
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s <message.csv> <orderbook.csv> <levels>\n", argv[0]);
        return 2;
    }
    const std::string msgPath = argv[1];
    const std::string obPath  = argv[2];
    const int levels = std::stoi(argv[3]);

    std::ifstream msg(msgPath), ob(obPath);
    if (!msg) { std::fprintf(stderr, "cannot open %s\n", msgPath.c_str()); return 2; }
    if (!ob)  { std::fprintf(stderr, "cannot open %s\n", obPath.c_str());  return 2; }

    OrderBook book;
    std::vector<OrderBook::Price> askP, bidP;
    std::vector<OrderBook::Size>  askS, bidS;

    std::string ml, ol;
    long long row = 0, checked = 0;
    while (std::getline(msg, ml) && std::getline(ob, ol)) {
        ++row;
        if (ml.empty()) continue;
        auto m = parseRow(ml);
        auto truth = parseRow(ol);
        if (m.size() < 6) {
            std::fprintf(stderr, "row %lld: malformed message line\n", row);
            return 2;
        }
        // m = [time, type, oid, size, price, dir]
        if (!book.apply((int)m[1], m[2], m[3], m[4], (int)m[5])) {
            std::fprintf(stderr, "row %lld: inconsistent message (type %lld, oid %lld)\n",
                         row, (long long)m[1], (long long)m[2]);
            return 1;
        }
        book.snapshot(levels, askP, askS, bidP, bidS);

        // Build our row in LOBSTER column order: AskP,AskS,BidP,BidS per level.
        std::vector<int64_t> mine;
        mine.reserve(levels * 4);
        for (int i = 0; i < levels; ++i) {
            mine.push_back(askP[i]); mine.push_back(askS[i]);
            mine.push_back(bidP[i]); mine.push_back(bidS[i]);
        }
        if ((int)truth.size() < levels * 4) {
            std::fprintf(stderr, "row %lld: orderbook line has %zu cols, need %d\n",
                         row, truth.size(), levels * 4);
            return 2;
        }
        for (int c = 0; c < levels * 4; ++c) {
            if (mine[c] != truth[c]) {
                int lvl = c / 4 + 1, field = c % 4;
                const char* names[] = {"AskPrice", "AskSize", "BidPrice", "BidSize"};
                std::fprintf(stderr,
                    "MISMATCH at row %lld, level %d %s: mine=%lld truth=%lld\n",
                    row, lvl, names[field], (long long)mine[c], (long long)truth[c]);
                std::fprintf(stderr, "  message: type=%lld oid=%lld size=%lld price=%lld dir=%lld\n",
                    (long long)m[1], (long long)m[2], (long long)m[3],
                    (long long)m[4], (long long)m[5]);
                return 1;
            }
        }
        ++checked;
    }
    std::printf("OK: %lld rows matched (%d levels each)\n", checked, levels);
    return 0;
}
