all: jsonrpc-dapp.host dapp.host dapp.emulator fs.ext2

build:
	docker build docker -t builder

run:
	docker run \
         -e USER=$$(id -u -n) \
         -e GROUP=$$(id -g -n) \
         -e UID=$$(id -u) \
         -e GID=$$(id -g) \
         -v `pwd`:/home/$$(id -u -n) \
         -w /home/$$(id -u -n) \
         --rm -it builder

run-as-root:
	docker run \
         -v `pwd`:/home/$$(id -u -n) \
         -w /home/$$(id -u -n) \
         --rm -it builder

#TOOLCHAIN:=cartesi/toolchain:0.15.0
#CROSS:=riscv64-cartesi-linux-gnu-
TOOLCHAIN:=builder:latest
CROSS:=riscv64-linux-gnu-
CXX:=g++-12
CC:=gcc-12

create-inputs:
	echo '{"status":"successful","sender":"diego","token":"CTSI","amount":"100000"}' | ./lambadex-memory-range.lua encode erc20-deposit-input > input-0.bin
	echo '{"msg_sender":"erc20-portal", "epoch_index": 0, "input_index": 0, "block_number":0, "time_stamp": 0}' | ./lambadex-memory-range.lua encode input-metadata > input-0-metadata.bin
	echo '{"status":"successful","sender":"perna","token":"USDT","amount":"10000"}' | ./lambadex-memory-range.lua encode erc20-deposit-input > input-1.bin
	echo '{"msg_sender":"erc20-portal", "epoch_index": 0, "input_index": 1, "block_number":1, "time_stamp": 1}' | ./lambadex-memory-range.lua encode input-metadata > input-1-metadata.bin
	echo '{"symbol":"CTSI/USDT","side":"buy","price":15,"quantity":1000}' | ./lambadex-memory-range.lua encode lambadex-new-order-input > input-2.bin
	echo '{"msg_sender":"perna", "epoch_index": 0, "input_index": 2, "block_number":2, "time_stamp": 2}' | ./lambadex-memory-range.lua encode input-metadata > input-2-metadata.bin
	echo '{"symbol":"CTSI/USDT","side":"sell","price":13,"quantity":500}' | ./lambadex-memory-range.lua encode lambadex-new-order-input > input-3.bin
	echo '{"msg_sender":"diego", "epoch_index": 0, "input_index": 3, "block_number":3, "time_stamp": 3}' | ./lambadex-memory-range.lua encode input-metadata > input-3-metadata.bin
	echo '{"symbol":"CTSI/USDT","side":"sell","price":18,"quantity":1500}' | ./lambadex-memory-range.lua encode lambadex-new-order-input > input-4.bin
	echo '{"msg_sender":"diego", "epoch_index": 0, "input_index": 4, "block_number":4, "time_stamp": 4}' | ./lambadex-memory-range.lua encode input-metadata > input-4-metadata.bin
	echo '{"token":"CTSI","quantity":100}' | ./lambadex-memory-range.lua encode lambadex-withdraw-input > input-5.bin
	echo '{"msg_sender":"perna", "epoch_index": 0, "input_index": 5, "block_number":5, "time_stamp": 5}' | ./lambadex-memory-range.lua encode input-metadata > input-5-metadata.bin

show-input-output:
#	./lambadex-memory-range.lua decode input-metadata < input-0-metadata.bin
#	./lambadex-memory-range.lua decode erc20-deposit-input < input-0.bin
#	./lambadex-memory-range.lua decode lambadex-wallet-notice < input-0-notice-0.bin
#	./lambadex-memory-range.lua decode input-metadata < input-1-metadata.bin
#	./lambadex-memory-range.lua decode erc20-deposit-input < input-1.bin
#	./lambadex-memory-range.lua decode lambadex-wallet-notice < input-1-notice-0.bin
#	./lambadex-memory-range.lua decode input-metadata < input-2-metadata.bin
#	./lambadex-memory-range.lua decode lambadex-new-order-input < input-2.bin
#	for t in input-2-notice*.bin; do ./lambadex-memory-range.lua decode lambadex-execution-notice < $$t ; done
	./lambadex-memory-range.lua decode input-metadata < input-3-metadata.bin
	./lambadex-memory-range.lua decode lambadex-new-order-input < input-3.bin
	for t in input-3-notice*.bin; do ./lambadex-memory-range.lua decode lambadex-execution-notice < $$t ; done
#	./lambadex-memory-range.lua decode input-metadata < input-4-metadata.bin
#	./lambadex-memory-range.lua decode lambadex-new-order-input < input-4.bin
#	for t in input-4-notice*.bin; do ./lambadex-memory-range.lua decode lambadex-execution-notice < $$t ; done
#	./lambadex-memory-range.lua decode input-metadata < input-5-metadata.bin
#	./lambadex-memory-range.lua decode lambadex-withdraw-input < input-5.bin
#	./lambadex-memory-range.lua decode lambadex-wallet-notice < input-5-notice-0.bin

create-queries:
	echo '{ "trader": "diego" }' | ./lambadex-memory-range.lua encode lambadex-wallet-query > query-0.bin
	echo '{ "symbol":"CTSI/USDT", "depth":10 }' | ./lambadex-memory-range.lua encode lambadex-book-query > query-1.bin

create-inputs-queries: create-inputs create-queries

show-query-output:
#	./lambadex-memory-range.lua decode lambadex-wallet-query < query-0.bin
#	./lambadex-memory-range.lua decode lambadex-wallet-report < query-0-report-0.bin
	./lambadex-memory-range.lua decode lambadex-book-query < query-1.bin
	./lambadex-memory-range.lua decode lambadex-book-report < query-1-report-0.bin


run-queries: fs.ext2
	jsonrpc-remote-cartesi-machine \
		--server-address=localhost:8080 > /dev/null 2>&1 &
	@while ! netstat -ntl 2>&1 | grep -q 8080; do sleep 0.1; done
	cartesi-machine \
		--append-dtb-bootargs="single=yes" \
		--flash-drive="label:fs,filename:fs.ext2" \
		--flash-drive=label:"lambda,filename:lambda.bin,shared" \
		--remote-address="localhost:8080" \
		--remote-protocol="jsonrpc" \
		--rollup \
		--no-remote-destroy \
		-- /mnt/fs/dapp.emulator
	@for q in 0 1 ; do \
		echo cartesi-machine \
			--remote-address="localhost:8080" \
			--remote-protocol="jsonrpc" \
			--no-remote-create \
			--no-remote-destroy \
			--rollup-inspect-state=query:query-$${q}.bin,report:query-$${q}-report-%o.bin ; \
		cartesi-machine \
			--remote-address="localhost:8080" \
			--remote-protocol="jsonrpc" \
			--no-remote-create \
			--no-remote-destroy \
			--rollup-inspect-state=query:query-$${q}.bin,report:query-$${q}-report-%o.bin ; \
	done
	@cartesi-machine \
		--remote-address="localhost:8080" \
		--remote-protocol="jsonrpc" \
		--no-remote-create \
		--remote-shutdown

run-inputs: fs.ext2
	@truncate --size 2M lambda.bin
	jsonrpc-remote-cartesi-machine \
		--server-address=localhost:8080 > /dev/null 2>&1 &
	@while ! netstat -ntl 2>&1 | grep -q 8080; do sleep 0.1; done
	cartesi-machine \
		--append-dtb-bootargs="single=yes" \
		--flash-drive="label:fs,filename:fs.ext2" \
		--flash-drive=label:"lambda,filename:lambda.bin,shared" \
		--remote-address="localhost:8080" \
		--remote-protocol="jsonrpc" \
		--remote-shutdown \
		--rollup \
		--rollup-advance-state=input:input-%i.bin,input_metadata:input-%i-metadata.bin,input_index_begin:0,input_index_end:6,epoch_index:0,notice_hashes:/dev/null,voucher_hashes:/dev/null,voucher:input-%i-voucher-%o.bin,notice:input-%i-notice-%o.bin,report:input-%i-report-%o.bin \
		-- /mnt/fs/dapp.emulator --initialize-lambda

run-inputs-host: dapp.host
	@truncate --size 2M lambda.host.bin
	./dapp.host --image-filename=lambda.host.bin --initialize-lambda --rollup-input-begin=0 --rollup-input-end=6

run-queries-host: dapp.host
	./dapp.host --image-filename=lambda.host.bin --rollup-query-begin=0 --rollup-query-end=2

dapp.emulator: dapp.cpp io-types.h rollup-emulator.hpp
	docker run \
         -e USER=$$(id -u -n) \
         -e GROUP=$$(id -g -n) \
         -e UID=$$(id -u) \
         -e GID=$$(id -g) \
         -v `pwd`:/home/$$(id -u -n) \
         -w /home/$$(id -u -n) \
         --rm \
		$(TOOLCHAIN) \
		$(CROSS)$(CXX) -DEMULATOR -std=c++20 -O4 -o $@ $<

run-queries-jsonrpc: jsonrpc-dapp.host
	./jsonrpc-dapp.host --server-address=localhost:8080 --image-filename=lambda.bin 2>&1 &
	@while ! netstat -ntl 2>&1 | grep -q 8080; do sleep 0.1; done
	@curl -s -X POST -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":"id","method":"inspect","params":{"query":{"what":"wallet","wallet":{"trader":"0x0000000000000000000000000000000000000002"}}}}' http://localhost:8080 > /dev/null
	@curl -s -X POST -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":"id","method":"inspect","params":{"query":{"what":"book","book":{"symbol":"CTSI/USDT","depth":10}}}}' http://localhost:8080 > /dev/null
	@curl -s -X POST -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":"id","method":"shutdown"}' http://localhost:8080 > /dev/null

dapp.host: dapp.cpp io-types.h rollup-bare-metal.hpp
	$(CXX) -std=c++20 -DBARE_METAL -O4 -o $@ $<

jsonrpc-dapp.host: jsonrpc-dapp.host.o json-util.o mongoose.o
	$(CXX) -std=c++20 -DJSONRPC_SERVER -O4 -o $@ $^

jsonrpc-dapp.host.o: dapp.cpp rollup-jsonrpc-server.hpp io-types.h
	$(CXX) -std=c++20 -DJSONRPC_SERVER -O4 -c -o $@ $<

json-util.o: json-util.cpp json-util.h io-types.h
	$(CXX) -std=c++20 -DJSONRPC_SERVER -O4 -c -o $@ $<

mongoose.o: mongoose.c
	$(CC) -c -O4 -o $@ $^

fs.ext2: dapp.emulator
	mkdir -p fs
	cp -f $< fs/
	genext2fs -f -i 512 -b 8192 -d fs fs.ext2
	truncate -s %4096 fs.ext2

clean-outputs:
	\rm -f input-*-notice-*.bin
	\rm -f input-*-voucher-*.bin
	\rm -f input-*-report-*.bin
	\rm -f query-*-notice-*.bin
	\rm -f query-*-voucher-*.bin
	\rm -f query-*-report-*.bin

clean-inputs-queries:
	\rm -f input-[0-9].bin
	\rm -f input-[0-9]-metadata.bin
	\rm -f query-[0-9].bin

clean: clean-inputs-queries clean-outputs
	\rm -f fs.ext2
	\rm -f dapp.emulator
	\rm -f lambda.bin
	\rm -f lambda.host.bin
	\rm -f dapp.host
	\rm -f jsonrpc-dapp.host
