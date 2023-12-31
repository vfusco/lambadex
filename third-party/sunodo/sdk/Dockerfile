# syntax=docker.io/docker/dockerfile:1.4
ARG BASE_IMAGE
ARG MACHINE_EMULATOR_VERSION
ARG LINUX_VERSION
ARG LINUX_KERNEL_VERSION

FROM ${BASE_IMAGE} as builder

WORKDIR /usr/local/src
RUN <<EOF
apt-get update
apt-get install -y --no-install-recommends autoconf automake build-essential ca-certificates curl libarchive-dev libtool
rm -rf /var/lib/apt/lists/*
EOF

FROM builder as genext2fs

WORKDIR /usr/local/src
ARG GENEXT2FS_VERSION=1.5.2
RUN <<EOF
curl -sL https://github.com/cartesi/genext2fs/archive/refs/tags/v${GENEXT2FS_VERSION}.tar.gz | tar --strip-components=1 -zxvf -
./autogen.sh
./configure --enable-libarchive
make
make install
EOF

FROM builder as su-exec

# v0.2 -> f85e5bde1afef399021fbc2a99c837cf851ceafa
WORKDIR /usr/local/src
RUN <<EOF
curl -sL https://github.com/ncopa/su-exec/archive/f85e5bde1afef399021fbc2a99c837cf851ceafa.tar.gz | tar --strip-components=1 -zxvf -
make
EOF

# sdk image
FROM cartesi/machine-emulator:$MACHINE_EMULATOR_VERSION
ARG MACHINE_EMULATOR_VERSION
ARG LINUX_VERSION
ARG LINUX_KERNEL_VERSION

USER root
RUN <<EOF
apt-get update
apt-get install -y --no-install-recommends curl libarchive-tools locales xz-utils
rm -rf /var/lib/apt/lists/*

sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen
locale-gen
update-locale LANG=en_US.UTF-8
EOF

ENV LC_ALL en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en

COPY entrypoint.sh /usr/local/bin/
COPY retar /usr/local/bin/
COPY --from=genext2fs /usr/local/bin/genext2fs /usr/bin/
COPY --from=su-exec /usr/local/src/su-exec /usr/local/bin/

ADD --chmod=644 \
    https://github.com/cartesi/image-kernel/releases/download/v$LINUX_VERSION/linux-$LINUX_KERNEL_VERSION.bin \
    /usr/share/cartesi-machine/images/linux.bin
RUN curl -sSL https://github.com/cartesi/image-kernel/releases/download/v$LINUX_VERSION/linux-headers-$LINUX_KERNEL_VERSION.tar.xz | \
    tar -xJ -C /
RUN \
    truncate --size 32M /usr/share/cartesi-machine/images/lambda.bin && \
    chmod 666 /usr/share/cartesi-machine/images/lambda.bin

WORKDIR /mnt
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
