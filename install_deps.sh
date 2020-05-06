#!/bin/bash

# This script installs and builds all the tools needed for building and running Concord-BFT
# It is used for building a docker image that is used by CI/CD and may be used in the
# development environment.
# If you prefer working w/o docker at your dev station just run the script with sudo.
# If you need to add any tool or dependency this is the right place to do it.
# Please bear in mind that we want to keep our list of dependencies as small as possible.
set -e

APT_GET_FLAGS="-y --no-install-recommends"
WGET_FLAGS="--no-check-certificate -q"

# Install tools
apt-get update && apt-get ${APT_GET_FLAGS} install \
    autoconf \
    automake \
    build-essential \
    ccache \
    clang \
    clang-format-9 \
    cmake \
    git \
    iptables \
    openssl \
    parallel \
    pkg-config \
    python3-pip \
    python3-setuptools \
    sudo \
    wget

ln -fs /usr/bin/clang-format-9 /usr/bin/clang-format
ln -fs /usr/bin/clang-format-diff-9 /usr/bin/clang-format-diff

# Install 3rd parties
apt-get ${APT_GET_FLAGS} install \
    libboost-filesystem1.65-dev \
    libboost-system1.65-dev \
    libboost1.65-dev \
    libbz2-dev \
    libgmp3-dev \
    liblz4-dev \
    libs3-dev \
    libsnappy-dev \
    libssl-dev \
    libtool \
    libz-dev \
    libzstd-dev

pip3 install --upgrade wheel && pip3 install --upgrade trio

# Build 3rd parties
cd ${HOME}
wget ${WGET_FLAGS} \
    https://github.com/log4cplus/log4cplus/releases/download/REL_1_2_1/log4cplus-1.2.1.tar.gz && \
    tar -xzf log4cplus-1.2.1.tar.gz && \
    rm log4cplus-1.2.1.tar.gz && \
    cd log4cplus-1.2.1 && \
    autoreconf -f -i && \
    ./configure CXXFLAGS="--std=c++11 -march=x86-64 -mtune=generic" \
                --enable-static && \
    make -j8 && \
    make install && \
    cd ${HOME} && \
    rm -r log4cplus-1.2.1

cd ${HOME}
git clone https://github.com/google/googletest.git && \
    cd googletest && \
    git checkout e93da23920e5b6887d6a6a291c3a59f83f5b579e && \
    mkdir _build && cd _build && \
    cmake -DCMAKE_CXX_FLAGS="-std=c++11 -march=x86-64 -mtune=generic" .. && \
    make -j8 && \
    make install && \
    cd ${HOME} && \
    rm -r googletest

cd ${HOME}
wget ${WGET_FLAGS} \
    https://github.com/weidai11/cryptopp/archive/CRYPTOPP_8_2_0.tar.gz && \
    tar -xzf CRYPTOPP_8_2_0.tar.gz && \
    rm CRYPTOPP_8_2_0.tar.gz && \
    cd cryptopp-CRYPTOPP_8_2_0 && \
    CXX_FLAGS="-march=x86-64 -mtune=generic" make -j8 && \
    make install && \
    cd ${HOME} && \
    rm -r cryptopp-CRYPTOPP_8_2_0

cd ${HOME}
git clone https://github.com/relic-toolkit/relic && \
    cd relic && \
    git checkout 0998bfcb6b00aec85cf8d755d2a70d19ea3051fd && \
    mkdir build && \
    cd build && \
    cmake   -DALLOC=AUTO -DWSIZE=64 \
            -DWORD=64 -DRAND=UDEV \
            -DSHLIB=ON -DSTLIB=ON \
            -DSTBIN=OFF -DTIMER=HREAL \
            -DCHECK=on -DVERBS=on \
            -DARITH=x64-asm-254 -DFP_PRIME=254 \
            -DFP_METHD="INTEG;INTEG;INTEG;MONTY;LOWER;SLIDE" \
            -DCOMP="-O3 -funroll-loops -fomit-frame-pointer -finline-small-functions -march=x86-64 -mtune=generic" \
            -DFP_PMERS=off -DFP_QNRES=on \
            -DFPX_METHD="INTEG;INTEG;LAZYR" -DPP_METHD="LAZYR;OATEP" .. && \
    make -j8 && \
    make install && \
    cd ${HOME} && \
    rm -r relic

cd ${HOME}
wget ${WGET_FLAGS} \
    https://github.com/facebook/rocksdb/archive/v5.7.3.tar.gz && \
    tar -xzf v5.7.3.tar.gz && \
    rm v5.7.3.tar.gz && \
    cd rocksdb-5.7.3 && \
    PORTABLE=1 make -j8 shared_lib && \
    PORTABLE=1 make install-shared && \
    cd ${HOME} && \
    rm -r rocksdb-5.7.3

cd ${HOME}
git clone https://github.com/emil-e/rapidcheck.git && \
    cd rapidcheck && \
    git checkout 258d907da00a0855f92c963d8f76eef115531716 && \
    mkdir build && cd build && \
    cmake -DRC_ENABLE_GTEST=ON -DRC_ENABLE_GMOCK=ON .. && \
    make -j8 && \
    make install && \
    cd ${HOME} && \
    rm -r rapidcheck

# In order to be compatible with the native build
cd ${HOME}
wget ${WGET_FLAGS} \
    https://dl.min.io/server/minio/release/linux-amd64/minio && \
    chmod 755 ${HOME}/minio
