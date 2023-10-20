TOOLCHAIN:=cartesi/toolchain:0.15.0
LINUX:=gcc:12.2.0

CXX = g++
CXXFLAGS = -std=c++17
SRCS = exchange-cli.cpp state-mgr.cpp

RUN_TOOLCHAIN = \
	docker run \
         -e USER=$$(id -u -n) \
         -e GROUP=$$(id -g -n) \
         -e UID=$$(id -u) \
         -e GID=$$(id -g) \
         -v `pwd`:/home/$$(id -u -n) \
         -w /home/$$(id -u -n) \
         --rm \
		-$(INTERACTIVE)t $(TOOLCHAIN) 

RUN_LINUX = \
	docker run \
         -e USER=$$(id -u -n) \
         -e GROUP=$$(id -g -n) \
         -e UID=$$(id -u) \
         -e GID=$$(id -g) \
         -v `pwd`:/home/$$(id -u -n) \
         -w /home/$$(id -u -n) \
         --rm \
		-$(INTERACTIVE)t $(LINUX) 

RUN_CARTESI_MACHINE := \
	cartesi-machine.lua \
		--append-dtb-bootargs="single=yes" \
	    --flash-drive="label:fs,filename:exchange-cli-emu.ext2" \
		--flash-drive=label:"state.bin,filename:state.bin,shared" \
		-i 

exchange-cli: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

STATE_SIZE := 8192
STATE_FILE = state.bin
$(STATE_FILE): exchange-cli-linux
	dd if=/dev/zero of=$@  bs=1 count=$(STATE_SIZE)
	$(RUN_LINUX) ./exchange-cli-linux  --init-state --setup-test-fixture  --lambda-state $(STATE_FILE)
	chmod 777 $(STATE_FILE)	

init-state:
	rm -f $(STATE_FILE)
	$(MAKE) $(STATE_FILE)

exchange-cli-emu.ext2: exchange-cli-emu $(STATE_FILE)
	mkdir -p fs
	cp -f exchange-cli-emu $(STATE_FILE) fs/
	$(RUN_TOOLCHAIN)	genext2fs -f -i 512 -b 8192 -d fs $@
	truncate -s %4096 $@

exchange-cli-emu: CXX = $(RUN_TOOLCHAIN) riscv64-cartesi-linux-gnu-g++
exchange-cli-emu: %.o = $(RUN_TOOLCHAIN) g++
exchange-cli-emu: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

exchange-cli-linux: CXX = $(RUN_LINUX) g++
exchange-cli-linux: %.o = $(RUN_LINUX) g++
exchange-cli-linux: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

cartesi-machine: exchange-cli-emu.ext2
	$(RUN_CARTESI_MACHINE) /bin/sh

run-emu: exchange-cli-emu.ext2
	$(RUN_CARTESI_MACHINE) /mnt/fs/exchange-cli-emu --interactive --lambda-state /mnt/fs/state.bin

run-linux: INTERACTIVE =i
run-linux: exchange-cli-emu.ext2
	$(RUN_LINUX) ./exchange-cli-linux --lambda-state state.bin --interactive


%.o: %.cpp $(wildcard *.h)
	$(CXX) $(CXXFLAGS) -c $< -o $@

toolchain-env: INTERACTIVE =i
toolchain-env:
	$(RUN_TOOLCHAIN) /bin/bash		

linux-env: INTERACTIVE =i
linux-env:
	$(RUN_LINUX) /bin/sh

clean:
	rm -f $(OBJS) exchange-cli exchange-cli-emu exchange-cli-linux
