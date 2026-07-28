# Kung-Fu Chess — System Architecture & Technical Specification

## 1. Executive Summary & Core Mechanics

Kung-Fu Chess is a real-time, simultaneous chess engine and distributed multiplayer gaming platform written in modern C++ (C++17/20).

Unlike traditional turn-based chess:

- **Turnless Gameplay**: Both players can manipulate any of their available pieces simultaneously in real-time.
- **Piece Cooldowns**: After completing a move, a piece enters a strict cooldown window (e.g., 2000 ms) during which it cannot be issued new move commands.
- **Real-Time Physics & Interpolation**: Pieces slide smoothly across board cells at a fixed velocity (`msPerCellSpeed`).
- **Mid-Route Collisions**: When opposing pieces cross paths along intersecting trajectories, the engine calculates sub-millisecond arrival vectors to resolve mid-route captures.
- **Premove Queue System**: Players can queue actions for pieces currently in motion or under cooldown, which execute automatically upon availability.

The platform supports local gameplay (vs. AI or local PvP), real-time online multiplayer over a custom hybrid protocol, live spectating, and horizontal cloud scalability.

## 2. Distributed Microservices Architecture

The system is organized into decoupled microservices. User accounts are persisted in **PostgreSQL**, while realtime coordination (matchmaking queues, session/shard registry, and inter-service Pub/Sub) continues to run through a central **Redis 7** layer:

```mermaid
flowchart TD
    CLIENT["Client App (SFML 3 / OpenCV)<br/>Human / AI Player Engine"]

    subgraph INGRESS["Ingress Layer"]
        GW["1. API & Realtime Gateway<br/>(gateway_service)<br/>• TCP Port 8080 (Auth, Lobby)<br/>• UDP Port 8080 (Moves, Heartbeats)"]
    end

    subgraph POSTGRES_TIER["User Persistence Layer (PostgreSQL 15)"]
        USER_DB["PostgreSQL users table<br/>• username (PK)<br/>• password_hash<br/>• rating<br/>• idx_users_rating (leaderboards)"]
    end

    subgraph REDIS["Shared Realtime Data & Messaging Layer (Redis 7)"]
        PUB_SUB["Redis Pub/Sub Channels<br/>• shard:&lt;id&gt;:commands<br/>• shard:&lt;id&gt;:moves<br/>• gateway:broadcast"]
        QUEUE["Matchmaking Queues<br/>• mm:public<br/>• mm:room:&lt;code&gt;"]
        REGISTRY["Session & Match Registry<br/>• match:&lt;id&gt;:server<br/>• user:&lt;user&gt;:match"]
    end

    subgraph CONTROL["Control Services"]
        MM["2. MatchmakerDaemon<br/>(matchmaker_service)"]
        ALLOC["3. Game Allocator<br/>(GameAllocator)"]
    end

    subgraph SHARDS["Execution Layer (Horizontal Scale)"]
        SHARD1["4. Game Server Shard #1<br/>(gameserver_shard)<br/>Authoritative GameEngine / LiveMatch"]
        SHARD2["5. Game Server Shard #2<br/>(gameserver_shard)<br/>Authoritative GameEngine / LiveMatch"]
    end

    CLIENT -- "TCP / UDP" --> GW
    GW -- "Auth / Lookup" --> USER_DB
    GW -- "Session Lookup" --> REGISTRY
    GW -- "Queue Entry" --> QUEUE
    GW -- "Dispatch Moves" --> PUB_SUB

    MM -- "Poll Pairs" --> QUEUE
    MM -- "Rating Lookup" --> USER_DB
    MM -- "Request Provisioning" --> ALLOC
    ALLOC -- "Register Mapping" --> REGISTRY
    ALLOC -- "Publish Command" --> PUB_SUB

    PUB_SUB -- "Receive Commands / Moves" --> SHARD1
    PUB_SUB -- "Receive Commands / Moves" --> SHARD2

    SHARD1 -- "Broadcast Updates" --> PUB_SUB
    SHARD2 -- "Broadcast Updates" --> PUB_SUB
    PUB_SUB -- "Forward to Clients" --> GW
```

## 3. Microservice Components Breakdown

### 3.1 API & Realtime Gateway Service (`gateway_service`)

**Executable**: `server/cmd/gateway_main.cpp`
**Role**: Client entry point for TCP control and UDP gameplay.

**Responsibilities**:
- Handles client authentication (`LOGIN_REQUEST`, `REGISTER_REQUEST`) against `PostgresUserRepository` (production) or `SqliteUserRepository` (local dev), via the shared `IUserRepository` interface.
- Manages session lifecycle (`PlayerSession`, `SessionManager`) and `SESSION_BIND` handshakes, backed by the Redis session registry.
- Proxies high-frequency move commands (`GAME_MOVE`) from clients to the designated Game Server Shard via Redis Pub/Sub.
- Receives match update events from shards and broadcasts them back to connected clients.
- Depends on both `redis` (realtime bus/registry) and `postgres` (user accounts) at startup, configured via `KUNGFU_POSTGRES_CONN`.

### 3.2 Matchmaker & Game Allocator Service (`matchmaker_service`)

**Executable**: `server/cmd/matchmaker_main.cpp`
**Role**: Background pairing daemon and room allocation manager.

**Responsibilities**:
- Periodically polls global Redis matchmaking sets (`mm:public`, `mm:room:<code_id>`).
- Evaluates rating differentials (ELO) — sourced from PostgreSQL via `IUserRepository` — and pairs compatible waiting players.
- Invokes `GameAllocator` to select an available Game Server Shard (e.g., Round-Robin strategy).
- Registers the match mapping (`match:<id>:server`) in `RedisSessionRegistry` and dispatches `PROVISION` commands via Redis Pub/Sub.

### 3.3 Authoritative Game Server Shards (`gameserver_shard`)

**Executable**: `server/cmd/gameserver_main.cpp`
**Role**: Authoritative game simulation worker nodes.

**Responsibilities**:
- Operates with zero direct client network sockets for maximum isolation and security.
- Subscribes to shard-specific Redis Pub/Sub command channels (`shard:<shard_id>:commands`).
- Instantiates and executes active `LiveMatch` instances containing the pure C++ `GameEngine` and `RealTimeArbiter`.
- Runs high-frequency tick loops (50 ms) using Boost.Asio steady timers and strands.
- Publishes authoritative state updates and move validation results back to the Gateway.
- Has no direct dependency on PostgreSQL; all persistence needed mid-match (session/shard registry, snapshots) flows through Redis.

### 3.4 Inter-Service Messaging Infrastructure (`RedisPubSubClient`)

**Class**: `server/network/RedisPubSubClient`
**Role**: High-performance asynchronous Redis Pub/Sub messenger built on Boost.Asio.

**Features**:
- Native RESP protocol formatting (`PUBLISH` / `SUBSCRIBE`).
- Dual-socket design separating command publishing from continuous push subscription streams.

### 3.5 User Persistence Layer (`PostgresUserRepository` / `SqliteUserRepository`)

**Classes**: `server/persistence/PostgresUserRepository.hpp` / `.cpp`, `server/persistence/SqliteUserRepository`
**Role**: Durable storage for user accounts, credentials, and ratings.

**Responsibilities**:
- `PostgresUserRepository` is the production implementation of `IUserRepository`, built on **libpqxx**, managing:
  - `users` table: `username VARCHAR(64) PRIMARY KEY`, `password_hash VARCHAR(255) NOT NULL`, `rating INTEGER DEFAULT 1200`.
  - `idx_users_rating` index on `rating DESC` for fast leaderboard and rating-band queries.
- `SqliteUserRepository` remains as the embedded, file-based fallback for single-process local/offline development.
- `IUserRepository` provides the abstraction that lets the rest of the codebase (Gateway, Matchmaker) switch between the two implementations at runtime with no code changes.
- `RedisUserRepository` has been **removed entirely** — Redis no longer stores any user account or credential data.

## 4. Networking & Dual-Transport Protocol

```mermaid
sequenceDiagram
    autonumber
    participant Client
    participant Gateway
    participant Postgres as PostgreSQL (User Store)
    participant Redis as Redis (Pub/Sub)
    participant Shard as Game Server Shard

    Client->>Gateway: 1. TCP LOGIN_REQUEST
    Gateway->>Postgres: Verify credentials / fetch rating
    Postgres-->>Gateway: User record (password_hash, rating)
    Gateway-->>Client: 2. TCP LOGIN_RESPONSE (sessionToken)
    Client->>Gateway: 3. UDP SESSION_BIND
    Gateway-->>Client: 4. UDP SESSION_BIND_ACK

    Client->>Gateway: 5. TCP JOIN_MATCH_REQUEST
    Gateway->>Redis: Push to mm:public Queue
    Redis-->>Redis: Matchmaker Allocates Match
    Redis->>Shard: PROVISION Command
    Shard->>Shard: Instantiates LiveMatch
    Redis-->>Gateway: Broadcast MATCH_FOUND
    Gateway-->>Client: 6. TCP MATCH_FOUND

    Client->>Gateway: 7. UDP GAME_MOVE
    Gateway->>Redis: Publish shard:gameserver-1:moves
    Redis->>Shard: Process Move Packet
    Shard->>Shard: GameEngine Validation
    Shard->>Redis: Publish gateway:broadcast
    Redis-->>Gateway: Receive Broadcast
    Gateway-->>Client: 8. UDP MOVE_RESULT / MOVE
```

### 4.1 Transport Responsibilities

- **TCP Control Channel (Port 8080)**: Reliable, ordered transport for Login, Registration, Room Listings, Spectating, and Disconnection notifications.
- **UDP Realtime Channel (Port 8080)**: Low-latency datagram transport for `GAME_MOVE`, `MOVE_RESULT`, and `HEARTBEAT` packets.
- **UDP Reliability**: Client maintains an in-flight `pendingMoves` map with automatic retry timeouts up to 5 attempts.

### 4.2 Strand Concurrency

- Each `LiveMatch` instance executes inside an isolated `boost::asio::strand`.
- Guarantees sequential execution of network events, engine tick logic, and timers without global mutex locks.

## 5. Technology Stack & Directory Structure

| Component | Library / Framework | Description |
|---|---|---|
| Core Engine | C++17 / C++20 | Pure domain logic (`GameEngine`, `RealTimeArbiter`, `RuleEngine`). |
| I/O Framework | Boost.Asio | Asynchronous network I/O, timers, strands, and sockets. |
| Realtime Data & Pub/Sub | Redis 7 (RESP Protocol) | Shared queues, session registry, and Pub/Sub bus. |
| User Persistence | PostgreSQL 15 (libpqxx) | Durable `users` table (credentials, ratings) with leaderboard indexing. |
| Cryptography | Libsodium | Argon2id password hashing (`crypto_pwhash`). |
| Database Option | SQLite3 | Embedded database alternative for single-process development. |
| Containerization | Docker & Docker Compose | Multi-stage slim container deployment. |

### Directory Layout

```text
├── app/
│   ├── main_sfml.cpp              # SFML graphical client
│   └── main_opencv.cpp            # OpenCV client
├── engine/                        # Core authoritative game engine
│   ├── actions/                   # ActionRequest / ActionResult
│   ├── analysis/                  # Move generator, evaluator, ELO
│   ├── board/                     # Board & Piece entities
│   ├── core/                      # GameEngine & MatchController
│   ├── realtime/                  # RealTimeArbiter & CollisionDetector
│   └── snapshot/                  # MatchStateSnapshot DTOs
├── server/                        # Server infrastructure
│   ├── cmd/                       # Microservice entry points (Binaries)
│   │   ├── gateway_main.cpp       # API & Realtime Gateway
│   │   ├── matchmaker_main.cpp    # Matchmaker & Game Allocator daemon
│   │   └── gameserver_main.cpp    # Authoritative Game Server Shard
│   ├── match/                     # LiveMatch, DistributedMatchmaker, GameAllocator
│   ├── network/                   # TcpServer, UdpServer, RedisPubSubClient
│   └── persistence/                # PostgresUserRepository, SqliteUserRepository, PasswordHasher
├── Dockerfile                     # Multi-stage microservices build (includes libpqxx)
└── docker-compose.yml             # Distributed stack orchestration (redis + postgres)
```

## 6. Execution & Container Orchestration

To build and launch the complete distributed microservice cluster:

```bash
docker-compose up --build -d
```

### Orchestrated Containers

- `kungfu_chess_redis`: Central Redis 7 instance for realtime Pub/Sub, matchmaking queues, and session registry (6379).
- `kungfu_chess_postgres`: PostgreSQL 15 instance for the user accounts store, port 5432, backed by the `postgres_data` volume.
- `kungfu_gateway`: Ingress API & Realtime Gateway (8080/tcp, 8080/udp) — depends on both `redis` and `postgres`, configured via `KUNGFU_POSTGRES_CONN`.
- `kungfu_matchmaker`: Matchmaker & Allocator daemon.
- `kungfu_gameserver_1`: Authoritative Game Server Shard #1 (`gameserver-1`).
- `kungfu_gameserver_2`: Authoritative Game Server Shard #2 (`gameserver-2`).

### docker-compose.yml (excerpt)

```yaml
services:
  # 1a. Central Redis Store & PubSub Messenger
  redis:
    image: redis:7-alpine
    container_name: kungfu_chess_redis
    restart: unless-stopped
    ports:
      - "6379:6379"
    volumes:
      - redis_data:/data

  # 1b. Central PostgreSQL Database for User Persistence
  postgres:
    image: postgres:15-alpine
    container_name: kungfu_chess_postgres
    restart: unless-stopped
    environment:
      - POSTGRES_DB=kungfu
      - POSTGRES_USER=postgres
      - POSTGRES_PASSWORD=postgres
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data

  # 2. API & Realtime Gateway Service (Entry point for clients)
  gateway:
    build:
      context: .
      dockerfile: Dockerfile
    container_name: kungfu_gateway
    command: ["/app/gateway_service"]
    restart: unless-stopped
    depends_on:
      - redis
      - postgres
    ports:
      - "8080:8080/tcp"
      - "8080:8080/udp"
    environment:
      - KUNGFU_TCP_PORT=8080
      - KUNGFU_UDP_PORT=8080
      - KUNGFU_REDIS_HOST=redis
      - KUNGFU_REDIS_PORT=6379
      - KUNGFU_POSTGRES_CONN=host=postgres port=5432 dbname=kungfu user=postgres password=postgres

  # 3. Matchmaker & Game Allocator Service
  matchmaker:
    build:
      context: .
      dockerfile: Dockerfile
    container_name: kungfu_matchmaker
    command: ["/app/matchmaker_service"]
    restart: unless-stopped
    depends_on:
      - redis
    environment:
      - KUNGFU_REDIS_HOST=redis
      - KUNGFU_REDIS_PORT=6379

  # 4. Game Server Shard 1 (Authoritative Game Engine)
  gameserver-1:
    build:
      context: .
      dockerfile: Dockerfile
    container_name: kungfu_gameserver_1
    command: ["/app/gameserver_shard"]
    restart: unless-stopped
    depends_on:
      - redis
    environment:
      - KUNGFU_SHARD_ID=gameserver-1
      - KUNGFU_REDIS_HOST=redis
      - KUNGFU_REDIS_PORT=6379

  # 5. Game Server Shard 2 (Horizontal Scaling - Authoritative Game Engine)
  gameserver-2:
    build:
      context: .
      dockerfile: Dockerfile
    container_name: kungfu_gameserver_2
    command: ["/app/gameserver_shard"]
    restart: unless-stopped
    depends_on:
      - redis
    environment:
      - KUNGFU_SHARD_ID=gameserver-2
      - KUNGFU_REDIS_HOST=redis
      - KUNGFU_REDIS_PORT=6379

volumes:
  redis_data:
  postgres_data:
```
