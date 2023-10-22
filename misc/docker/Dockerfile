FROM ubuntu:jammy

RUN apt-get update && \
    DEBIAN_FRONTEND="noninteractive" apt-get install --no-install-recommends -y \
        build-essential vim wget ca-certificates g++-12-riscv64-linux-gnu git xz-utils tar && \
    rm -rf /var/lib/apt/lists/*

COPY entrypoint.sh /usr/local/bin/entrypoint.sh

RUN chmod +x /usr/local/bin/entrypoint.sh

ENV BUILD_BASE "/tmp/build"

RUN mkdir $BUILD_BASE

RUN \
    cd $BUILD_BASE && \
    git clone --branch v0.2 --depth 1 https://github.com/ncopa/su-exec.git && \
    cd su-exec && \
    if [ `git rev-parse --verify HEAD` != 'f85e5bde1afef399021fbc2a99c837cf851ceafa' ]; then exit 1; fi && \
    make && \
    cp su-exec /usr/local/bin/ && \
    rm -rf $BUILD_BASE/su-exec

RUN \
    rm -rf /usr/riscv64-linux-gnu/include/linux && \
    cd $BUILD_BASE && \
    wget https://github.com/cartesi/image-kernel/releases/download/v0.17.0/linux-headers-5.15.63-ctsi-2-v0.17.0.tar.xz && \
    tar -Jxvf linux-headers-5.15.63-ctsi-2-v0.17.0.tar.xz && \
    mv opt/riscv/kernel/work/linux-headers/include/linux /usr/riscv64-linux-gnu/include && \
    rm -rf $BUILD_BASE/opt

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

CMD ["/bin/bash", "-l"]
