

# Docker image platform
BUILD_PLATFORM ?=

ifneq ($(BUILD_PLATFORM),)
DOCKER_PLATFORM=--platform $(BUILD_PLATFORM)
endif

image:
	docker build $(DOCKER_PLATFORM) -t sunodo/rollups-node:0.5.0 -f Dockerfile .
