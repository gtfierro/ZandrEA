# .devcontainer/Dockerfile for ZandrEA
FROM ubuntu:24.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install core build dependencies and libraries
RUN apt-get update && apt-get upgrade -y && apt-get install -y \
    libboost-chrono-dev \
    libboost-date-time-dev \
    libboost-dev \
    libboost-filesystem-dev \
    libboost-iostreams-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libcereal-dev \
    libcpprest-dev \
    libstdc++-13-dev \
    libsz2 \
    libaec-dev \
    build-essential \
    clangd-18 \
    gdb \
    git \
    curl \
    wget \
    ca-certificates \
    gpg \
    lsb-release \
    autoconf \
    libtool \
    pkg-config \
    libsystemd-dev \
    nodejs \
    python3 \
    python3-pip \
    python3-requests \
    zsh \
    sudo \
    && ln -sf /usr/bin/clangd-18 /usr/bin/clangd \
    && rm -rf /var/lib/apt/lists/*

# Install latest CMake from Kitware repo
RUN test -f /usr/share/doc/kitware-archive-keyring/copyright || \
      wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | \
      gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null \
    && echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main' | \
      tee /etc/apt/sources.list.d/kitware.list >/dev/null \
    && apt-get update \
    && apt-get install -y kitware-archive-keyring cmake \
    && rm -rf /var/lib/apt/lists/*

# Set up build environment
ARG PKGROOT=/ea
ENV HOME=${PKGROOT}
ENV PATH=$PKGROOT/HDF5/bin:$PATH
WORKDIR $PKGROOT

# Copy necessary files for HDF5 build (assumes context is project root)
COPY hdf5-1.14.4-2.tar.gz h5c++.tmpl Makefile ./

# Build HDF5
RUN mkdir -p /ea/include/ea /ea/lib/ea /ea/bin /data \
    && make -j$(nproc) compiler

# Build gRPC (v1.78.0)
WORKDIR $PKGROOT/grpc/grpc-src/
RUN git clone --recurse-submodules -b v1.78.0 --depth 1 --shallow-submodules \
      https://github.com/grpc/grpc . \
    && mkdir -p $PKGROOT/grpc/build/
WORKDIR $PKGROOT/grpc/build/
RUN cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_CXX_STANDARD=17 \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_INSTALL_PREFIX=$PKGROOT/grpc \
      ../grpc-src \
    && make -j$(nproc) && make install

# Clean up gRPC source to save space
WORKDIR $PKGROOT
RUN rm -rf $PKGROOT/grpc/grpc-src/

# Set up environment variables for gRPC and HDF5
ENV PATH=$PATH:$PKGROOT/grpc/bin:$PKGROOT/protobuf
ENV LD_LIBRARY_PATH=$PKGROOT/grpc/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

# Create the zed user (uid 1000 is default for first user in devcontainers)
ARG USERNAME=zed
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN if getent group $USER_GID; then \
        groupmod -n $USERNAME $(getent group $USER_GID | cut -d: -f1); \
    else \
        groupadd --gid $USER_GID $USERNAME; \
    fi \
    && if getent passwd $USER_UID; then \
        usermod -l $USERNAME -m -d /home/$USERNAME $(getent passwd $USER_UID | cut -d: -f1); \
    else \
        useradd --uid $USER_UID --gid $USER_GID -m $USERNAME; \
    fi \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME \
    && chown -R $USERNAME:$USERNAME $PKGROOT

# Set the default shell to zsh for the zed user
RUN chsh -s /bin/zsh $USERNAME

# Default workspace directory
WORKDIR /workspaces/ZandrEA

USER $USERNAME
