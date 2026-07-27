# Stage 1: Build stage
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install compiler, CMake, and build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    libsodium-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source repository
COPY . .

# Build the dedicated server executable in Release mode
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make kungfu_server -j$(nproc)

# Stage 2: Runtime stage
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime shared libraries only
RUN apt-get update && apt-get install -y \
    libboost-system-dev \
    libsodium23 \
    libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Create persistent storage directory for DB
RUN mkdir -p /app/data

# Copy built server binary from builder stage
COPY --from=builder /app/build/kungfu_server /app/kungfu_server

# Default environment variables for container runtime
ENV KUNGFU_TCP_PORT=8080
ENV KUNGFU_UDP_PORT=8080
ENV KUNGFU_DB_PATH=/app/data/kungfu_chess.db

# Expose dual transport ports (TCP control + UDP realtime)
EXPOSE 8080/tcp
EXPOSE 8080/udp

# Launch server
CMD ["/app/kungfu_server"]