# Stage 1: Fast Build Stage (Headless Microservices Compilation)
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install essential server build tools only
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-system-dev \
    libboost-thread-dev \
    libsodium-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source repository
COPY . .

# Build all microservices executables in Release mode
RUN rm -rf build && mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_CLIENTS=OFF -DBUILD_TESTS=OFF .. && \
    make gateway_service matchmaker_service gameserver_shard -j$(nproc)

# Stage 2: Runtime stage (Slim container image for all microservices)
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dynamic libraries only
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libsodium23 \
    libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Create persistent storage directory
RUN mkdir -p /app/data

# Copy all built server binaries from builder stage
COPY --from=builder /app/build/gateway_service /app/gateway_service
COPY --from=builder /app/build/matchmaker_service /app/matchmaker_service
COPY --from=builder /app/build/gameserver_shard /app/gameserver_shard

# Default Environment Variables
ENV KUNGFU_REDIS_HOST=redis
ENV KUNGFU_REDIS_PORT=6379
ENV KUNGFU_TCP_PORT=8080
ENV KUNGFU_UDP_PORT=8080

EXPOSE 8080/tcp
EXPOSE 8080/udp

# Default command if executed without args
CMD ["/app/gateway_service"]