CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

verify: src/verify.cpp src/OrderBook.hpp
	$(CXX) $(CXXFLAGS) src/verify.cpp -o verify

.PHONY: test
test: verify
	./verify test/sample_message.csv test/sample_orderbook.csv 2

.PHONY: clean
clean:
	rm -f verify
