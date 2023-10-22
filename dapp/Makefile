CXX  := g++

.PHONY: clean

dapp: dapp.cpp io-types.h rollup-emulator.hpp
	$(CXX) -DEMULATOR -std=c++20 -O4 -I /opt/riscv/kernel/work/linux-headers/include -o $@ $<

lambda.bin:
	truncate --size 2M lambda.bin

clean:
	@rm -rf dapp
