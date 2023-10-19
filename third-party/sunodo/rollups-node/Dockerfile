# syntax=docker.io/docker/dockerfile:1.4
ARG REGISTRY=docker.io
ARG ORG=sunodo
ARG ROLLUPS_VERSION=0.5.0

FROM docker.io/cartesi/machine-emulator:lambadex.deb as machine-emulator-package
FROM docker.io/cartesi/server-manager:lambadex as server-manager-img

FROM ${REGISTRY}/${ORG}/rollups-node:${ROLLUPS_VERSION}

USER root

COPY --from=server-manager-img /usr/bin/server-manager /usr/bin/server-manager
COPY --from=machine-emulator-package /usr/src/emulator/cartesi-machine-v0.15.2_amd64.deb /tmp/cartesi-machine.deb


RUN apt-get update && \
    apt-get remove -y cartesi-machine && \
    DEBIAN_FRONTEND="noninteractive" apt-get install -y /tmp/cartesi-machine.deb libboost-log1.81.0 && \
    rm -rf /tmp/cartesi-machine.deb /var/lib/apt/lists/*
