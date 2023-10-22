force ?= false

all: dapp node

submodules:
	git submodule update --init --recursive

third-party/cartesi/machine-emulator/Makefile:
	$(info submodules are not initialized! Please run 'make submodules' first)
	@exit 1

emulator.deb: third-party/cartesi/machine-emulator/Makefile
	@if ! (docker image inspect "cartesi/machine-emulator:lambadex.deb" >/dev/null 2>&1) || [[ "$(force)" == "true" ]]; then \
		echo "Building emulator..."; \
		$(MAKE) -C third-party/cartesi/machine-emulator build-debian-package; \
	fi

emulator: emulator.deb
	@if ! (docker image inspect "cartesi/machine-emulator:lambadex" >/dev/null 2>&1) || [[ "$(force)" == "true" ]]; then \
		echo "Building emulator..."; \
		$(MAKE) -C third-party/cartesi/machine-emulator build-debian-image; \
	fi

manager: emulator
	@if ! (docker image inspect "cartesi/server-manager:lambadex" >/dev/null 2>&1) || [[ "$(force)" == "true" ]]; then \
		echo "Building server-manager..."; \
		$(MAKE) -C third-party/cartesi/server-manager image; \
	fi

sdk: emulator
	@if ! (docker image inspect "sunodo/sdk:0.2.0-lambadex" >/dev/null 2>&1) || [[ "$(force)" == "true" ]]; then \
		echo "Building sunodo sdk..."; \
		$(MAKE) -C third-party/sunodo/sdk; \
	fi

node: manager
	@if ! (docker image inspect "sunodo/rollups-node:0.5.0" >/dev/null 2>&1) || [[ "$(force)" == "true" ]]; then \
		echo "Building sunodo rollups-node..."; \
		$(MAKE) -C third-party/sunodo/rollups-node; \
	fi

dapp/.sunodo/image/hash:
	@cd dapp && sunodo build

dapp: sdk dapp/.sunodo/image/hash

run: dapp node
	@cd dapp && sunodo run

clean-dapp:
	@cd dapp && sunodo clean

clean: clean-dapp

help:
	@echo "Usage:"
	@echo "  make all                 - Build all targets (DEFAULT)"
	@echo "  make emulator            - Build the 'emulator' image."
	@echo "  make manager             - Build the 'manager' image."
	@echo "  make sdk                 - Build the 'sdk' image."
	@echo "  make node                - Build the 'node' image."
	@echo "  make dapp                - Build the cartesi-machine dapp."
	@echo "  make run                 - Build the run the dapp."
	@echo "  make help                - Display this usage information."

.PHONY: all emulator manager sdk node help clean-dapp clean
