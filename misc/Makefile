all: dapp.host

TOOLCHAIN:=cartesi/toolchain:0.15.0

create-inputs:
	echo '{"status":"successful","sender":"diego","token":"ctsi","amount":"100000"}' | ./lambadex-memory-range.lua encode erc20-deposit-input > input-0.bin
	echo '{"msg_sender":"erc20-portal", "epoch_index": 0, "input_index": 0, "block_number":0, "time_stamp": 0}' | ./lambadex-memory-range.lua encode input-metadata > input-0-metadata.bin
	echo '{"status":"successful","sender":"perna","token":"usdt","amount":"10000"}' | ./lambadex-memory-range.lua encode erc20-deposit-input > input-1.bin
	echo '{"msg_sender":"erc20-portal", "epoch_index": 0, "input_index": 1, "block_number":1, "time_stamp": 1}' | ./lambadex-memory-range.lua encode input-metadata > input-1-metadata.bin
	echo '{"symbol":"ctsi/usdt","side":"buy","price":15,"quantity":1000}' | ./lambadex-memory-range.lua encode lambadex-new-order-input > input-2.bin
	echo '{"msg_sender":"perna", "epoch_index": 0, "input_index": 2, "block_number":2, "time_stamp": 2}' | ./lambadex-memory-range.lua encode input-metadata > input-2-metadata.bin
	echo '{"symbol":"ctsi/usdt","side":"sell","price":13,"quantity":500}' | ./lambadex-memory-range.lua encode lambadex-new-order-input > input-3.bin
	echo '{"msg_sender":"diego", "epoch_index": 0, "input_index": 3, "block_number":3, "time_stamp": 3}' | ./lambadex-memory-range.lua encode input-metadata > input-3-metadata.bin
	echo '{"symbol":"ctsi/usdt","side":"sell","price":18,"quantity":1500}' | ./lambadex-memory-range.lua encode lambadex-new-order-input > input-4.bin
	echo '{"msg_sender":"diego", "epoch_index": 0, "input_index": 4, "block_number":4, "time_stamp": 4}' | ./lambadex-memory-range.lua encode input-metadata > input-4-metadata.bin
	echo '{"token":"ctsi","quantity":100}' | ./lambadex-memory-range.lua encode lambadex-withdraw-input > input-5.bin
	echo '{"msg_sender":"perna", "epoch_index": 0, "input_index": 5, "block_number":5, "time_stamp": 5}' | ./lambadex-memory-range.lua encode input-metadata > input-5-metadata.bin
	echo '{"id":54321}' | ./lambadex-memory-range.lua encode lambadex-cancel-order-input > input-6.bin
	echo '{"msg_sender":"diego", "epoch_index": 0, "input_index": 6, "block_number":6, "time_stamp": 6}' | ./lambadex-memory-range.lua encode input-metadata > input-6-metadata.bin

create-queries:
	echo '{}' | ./lambadex-memory-range.lua encode lambadex-wallet-query > query-0.bin
	echo '{ "symbol":"ctsi/usdt", "depth":10 }' | ./lambadex-memory-range.lua encode lambadex-book-query > query-1.bin

run-queries: create-queries fs.ext2
	jsonrpc-remote-cartesi-machine \
		--server-address=localhost:8080 2>&1 &
	while ! netstat -ntl 2>&1 | grep 8080; do sleep 1; done
	cartesi-machine \
		--append-dtb-bootargs="single=yes" \
	    --flash-drive="label:fs,filename:fs.ext2" \
		--flash-drive=label:"lambda,filename:lambda.bin,shared" \
		--remote-address="localhost:8080" \
		--remote-protocol="jsonrpc" \
		--rollup \
		--no-remote-destroy \
		-- /mnt/fs/io-test.emulator
	for q in query-0.bin query-1.bin ; do \
		cartesi-machine \
			--remote-address="localhost:8080" \
			--remote-protocol="jsonrpc" \
			--no-remote-create \
			--no-remote-destroy \
			--rollup-inspect-state=query:$$q,report:report-%o-$$q ; \
	done
	cartesi-machine \
		--remote-address="localhost:8080" \
		--remote-protocol="jsonrpc" \
		--no-remote-create \
		--remote-shutdown

run-inputs: create-inputs fs.ext2
	jsonrpc-remote-cartesi-machine \
		--server-address=localhost:8080 2>&1 &
	while ! netstat -ntl 2>&1 | grep 8080; do sleep 1; done
	cartesi-machine \
		--append-dtb-bootargs="single=yes" \
	    --flash-drive="label:fs,filename:fs.ext2" \
		--flash-drive=label:"lambda,filename:lambda.bin,shared" \
		--remote-address="localhost:8080" \
		--remote-protocol="jsonrpc" \
		--remote-shutdown \
		--rollup \
		--rollup-advance-state=input:input-%i.bin,input_metadata:input-%i-metadata.bin,input_index_begin:0,input_index_end:4,epoch_index:0,notice_hashes:/dev/null,voucher_hashes:/dev/null \
		-- /mnt/fs/dapp.emulator

io-test.emulator: io-test.cpp
	docker run \
         -e USER=$$(id -u -n) \
         -e GROUP=$$(id -g -n) \
         -e UID=$$(id -u) \
         -e GID=$$(id -g) \
         -v `pwd`:/home/$$(id -u -n) \
         -w /home/$$(id -u -n) \
         --rm \
		$(TOOLCHAIN) \
		riscv64-cartesi-linux-gnu-g++ -std=c++20 -O4 -o $@ $<

io-test.host: io-test.cpp
	g++ -std=c++20 -DBARE_METAL -O4 -o $@ $<

dapp.host: dapp.cpp
	g++ -std=c++20 -DBARE_METAL -O4 -o $@ $^

mmap-test.host: mmap-test.cpp
	g++ -std=c++20 -O4 -o $@ $<

mmap-test.emulator: mmap-test.cpp
	docker run \
         -e USER=$$(id -u -n) \
         -e GROUP=$$(id -g -n) \
         -e UID=$$(id -u) \
         -e GID=$$(id -g) \
         -v `pwd`:/home/$$(id -u -n) \
         -w /home/$$(id -u -n) \
         --rm \
		$(TOOLCHAIN) \
		riscv64-cartesi-linux-gnu-g++ -std=c++20 -O4 -o $@ $<

fs.ext2: io-test.emulator
	mkdir -p fs
	cp -f io-test.emulator fs/
	genext2fs -f -i 512 -b 8192 -d fs fs.ext2
	truncate -s %4096 fs.ext2
