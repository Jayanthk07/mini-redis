# mini-redis

A Redis-compatible TCP server built from scratch in C++. Implements the RESP protocol, core Redis commands, polymorphic data types, master-replica replication, and is containerized with Docker.

---

## Architecture

```
Client
  ↓
RedisServer (TCP, epoll event loop, non-blocking I/O)
  ↓
ClientHandler (parses RESP, routes commands)
  ↓              ↓
Store        propagate() → Replica
(shared_mutex)
unordered_map<string, unique_ptr<DataType>>
  ↓              ↓
StringValue   ListValue
(string)      (deque<string>)
```

**Master-Replica flow:**
```
redis-cli SET name Jayanth → Master (:6379)
                                  ↓ propagates raw RESP
                             Replica (:6380)
                             applies to own Store

redis-cli GET name → Replica (:6380) → "Jayanth"
```

---

## Features

- TCP server with epoll-based event loop and non-blocking I/O
- RESP protocol parser (length-prefixed, binary-safe — reads declared `$<len>` byte counts rather than scanning for markers)
- Commands: `PING`, `ECHO`, `SET`, `GET`, `DEL`, `EXISTS`, `EXPIRE`
- List commands: `LPUSH`, `RPUSH`, `LRANGE`, `LLEN`, `LREM`
- Polymorphic `DataType` base class with `StringValue` and `ListValue` subtypes
- RAII memory management using `std::unique_ptr<DataType>` — no manual `new`/`delete`
- Thread-safe `Store` using `std::shared_mutex` (concurrent readers, exclusive writers) and RAII lock guards
- Input validation on all numeric arguments — returns `-ERR value is not an integer or out of range` instead of crashing
- `DEL` returns correct `0`/`1` based on whether the key existed
- Master-replica replication (`--master` / `--replica <port>` flags)
- Dockerized with `Dockerfile` and `docker-compose.yml`

---

## OOP Design

| Class | Responsibility |
|---|---|
| `RedisServer` | Socket setup, epoll event loop, accept connections, replica tracking |
| `ClientHandler` | Command routing, RESP response formatting, input validation |
| `Store` | Thread-safe in-memory key-value store with `shared_mutex` and active expiry eviction |
| `CommandParser` | Parses raw RESP bytes into token vectors (length-prefixed, binary-safe) |
| `DataType` | Abstract base class for stored values (`virtual` destructor, `type()` method) |
| `StringValue` | Stores a single string (inherits DataType) |
| `ListValue` | Stores a deque of strings (inherits DataType) |
| `Replicator` | Connects to master, receives and applies propagated commands |

---

## Requirements

- g++ (C++17)
- make
- redis-tools (for testing with `redis-cli` and `redis-benchmark`)
- Docker (optional)

```bash
sudo apt install g++ make redis-tools
```

---

## Build & Run

**Local:**
```bash
make
./redis-server --port 6379 --master
```

**Replica:**
```bash
./redis-server --port 6380 --replica 6379
```

**Docker:**
```bash
docker build -t redis-cpp .
docker run -p 6379:6379 redis-cpp
```

---

## Usage

```bash
redis-cli -p 6379 SET name Jayanth        # OK
redis-cli -p 6379 GET name                # "Jayanth"
redis-cli -p 6379 SET temp hello EX 5     # OK (expires in 5s)
redis-cli -p 6379 DEL name                # (integer) 1
redis-cli -p 6379 DEL name                # (integer) 0 — key already deleted
redis-cli -p 6379 EXISTS name             # (integer) 0

redis-cli -p 6379 LPUSH mylist a b c      # (integer) 3
redis-cli -p 6379 LRANGE mylist 0 -1      # [c, b, a]
redis-cli -p 6379 LLEN mylist             # (integer) 3
redis-cli -p 6379 LREM mylist 1 a         # OK
```

**Error handling:**
```bash
redis-cli -p 6379 SET key val EX notanum  # (error) ERR value is not an integer or out of range
redis-cli -p 6379 LRANGE mylist abc def   # (error) ERR value is not an integer or out of range
redis-cli -p 6379 SET strkey hello
redis-cli -p 6379 LPUSH strkey oops       # (error) WRONGTYPE Operation against a key holding the wrong kind of value
```

**Replication:**
```bash
# Terminal 1
./redis-server --port 6379 --master

# Terminal 2
./redis-server --port 6380 --replica 6379

# Terminal 3
redis-cli -p 6379 SET name Jayanth
redis-cli -p 6380 GET name    # returns "Jayanth"
```

---

## Benchmark

Tested with `redis-benchmark` on native Linux (Dell G15, Ubuntu 22.04) — not Docker/WSL.

**Core commands** (`-n 100000 -c 50`):

| Command | Throughput (req/s) | p50 latency |
|---|---|---|
| PING | ~206,000 | 0.023ms |
| SET | ~208,000 | 0.023ms |
| GET | ~200,000 | 0.023ms |
| LPUSH | ~205,000 | 0.023ms |
| RPUSH | ~205,000 | 0.023ms |

**Concurrency scaling** (`SET`/`GET`, `-n 100000`):

| Concurrent clients | Throughput (req/s) |
|---|---|
| 1 | ~71,000 |
| 10 | ~220,000 |
| 50 | ~209,000 |
| 200 | ~215,000 |

Throughput scales sharply from 1→10 concurrent clients (single-connection requests are latency-bound), then plateaus — the `std::shared_mutex` guarding `Store` allows concurrent readers but exclusive writers, so write-heavy workloads still serialize.

**Payload handling**: verified with a 1024-byte payload (`-d 1024`) after fixing the fixed-size read buffer and moving to length-prefixed RESP parsing — see Known Limitations.

**Bottleneck**: While `std::shared_mutex` allows concurrent reads, write operations (`SET`, `DEL`) still require exclusive write locks. Under heavy concurrent write loads, mutex contention on the single `Store` instance becomes the primary bottleneck rather than CPU or network capacity.

---
## Known Limitations

- **Type-mismatch errors are shallow**: `WRONGTYPE` is detected via a length-comparison workaround in `ClientHandler` rather than a clean type check surfaced directly from the `Store` layer.
- **No data persistence**: In-memory only; restarts reset the store (no RDB snapshotting or AOF log).
- **Single-master without automated failover**: Replicas are read-only. If the master node goes down, manual reconfiguration is required (would require a consensus protocol like Raft or Redis Sentinel for automated leader election).

---
## Design Tradeoffs

**Migrating from Thread-per-client to epoll Event Loop:**
Started with a thread-per-client model for simplicity. However, thread-per-client incurs ~8MB stack memory overhead per connection and severe context-switching costs under high concurrency. Migrated to a non-blocking Linux `epoll` event loop in edge-triggered mode (`EPOLLET`), allowing a single thread to multiplex thousands of concurrent connections with negligible memory footprint.

**`std::mutex` vs `std::shared_mutex`:**
Used `std::shared_mutex` for read/write lock separation — `GET`, `EXISTS`, `LRANGE`, `LLEN` take shared locks (multiple concurrent readers), while `SET`, `DEL`, `LPUSH`, `RPUSH`, `LREM` take exclusive locks. This improves read-heavy workload performance significantly compared to a plain `std::mutex`.

**`std::unique_ptr` for memory safety:**
All `DataType` objects are owned by `std::unique_ptr` inside the `Store` map. This eliminates manual `new`/`delete`, prevents memory leaks on key overwrites or deletions, and simplifies the destructor — the map's own cleanup frees everything automatically.

**Input validation with `safeStoi`:**
All user-provided numeric arguments (`EX`/`PX` values, `LRANGE` indices, `LREM` count) are parsed through a safe wrapper that catches `std::invalid_argument` and `std::out_of_range` exceptions, returning a Redis-compatible error instead of crashing.

**Single-master replication:**
All writes go to master only. Replicas are read-only. This guarantees consistency — one source of truth. The tradeoff is write availability: if master goes down, no writes can be accepted until a new master is elected (not implemented — would require a consensus protocol like Raft).

**In-memory only:**
No persistence. All data is lost on restart. A production system would use RDB snapshots (periodic binary dump) or AOF (append-only log of every write command) for durability.

---

## Bugs Fixed During Development

| Bug | Root Cause | Fix |
|---|---|---|
| Replica `GET` returned nil after `SET` | Stale zombie processes from crashed test runs squatting on ports | `pkill -9 redis-server` before each test run |
| `connect()` failed silently on replica startup | No return value check on `connect()` | Added retry loop: 10 attempts, 500ms backoff, `exit(1)` on all failures |
| `bind()` crash on server restart | Port still in `TIME_WAIT` state | Added `SO_REUSEADDR` socket option |
| `accept()` crash on bad connection | No error handling | Changed to `continue` on `accept()` failure instead of crashing |
| Commands not matching (`ping` vs `PING`) | `redis-cli` sends lowercase, code checked uppercase | Added `transform` to uppercase `cmd[0]` after parsing |
| Replica receiving SET twice | `propagate()` called before checking `isMaster` | Added `getIsMaster()` check before every `propagate()` call |
| Server crash on binary payloads (e.g. `redis-benchmark -d 1024`) | Parser scanned for `*`/`$` bytes anywhere in the line instead of trusting RESP's declared length prefix — binary payload bytes could collide with those markers, causing token count mismatch and out-of-bounds vector access | Rewrote parser to read declared `$<len>` byte counts directly (length-prefixed, binary-safe) instead of scanning content |
| Crash on malformed/incomplete commands | No bounds checking before indexing `cmd[1]`, `cmd[2]`, etc. in `ClientHandler` | Added argument-count guards before every command handler; returns `-ERR wrong number of arguments` instead of crashing |
| `LPUSH`/`RPUSH` on a non-list key crashed | `dynamic_cast<ListValue*>` result never null-checked before dereferencing | Added null check; returns `WRONGTYPE` error instead of crashing |
| Memory leak on key overwrite | `Store::set` allocated a new `DataType*` without freeing the previous value at that key | Replaced raw `new`/`delete` with `std::unique_ptr<DataType>` — assignment operator auto-frees old value |
| `DEL` always returned `1` even if key didn't exist | `Store::del` returned `void`, `ClientHandler` hardcoded `:1` | Changed `del()` to return `bool`; response now reflects actual deletion |
| Server crash on non-numeric arguments (e.g. `SET key val EX abc`) | `stoi()` throws `std::invalid_argument` on non-numeric input, uncaught | Added `safeStoi()` wrapper; returns `-ERR value is not an integer or out of range` instead of crashing |
| Protocol errors on fragmented TCP packets | Assumed a single `recv()` call contained exactly one complete RESP command | Added persistent per-connection buffers (`clientBuffers[fd]`) to accumulate incoming stream chunks until a full command is parsed |
---

## What I Learned

- How TCP servers work at the socket level (`socket`, `bind`, `listen`, `accept`, `send`, `recv`)
- RESP wire protocol — how Redis clients and servers actually communicate
- Runtime polymorphism with `virtual` functions and `dynamic_cast`
- Thread safety with `std::shared_mutex`, `lock_guard`, and `unique_lock` (RAII pattern)
- Why `shared_mutex` matters for read-heavy workloads
- RAII ownership with `std::unique_ptr` — why smart pointers eliminate entire classes of memory bugs
- How master-replica replication works and the CAP theorem tradeoffs it involves
- How to implement an event-driven server using Linux `epoll` in edge-triggered mode (`EPOLLET`)
- Why RESP's explicit length-prefixing exists — a naive parser that scans for `*`/`$` markers breaks the moment binary payload data collides with those bytes; length-prefixing avoids ambiguity entirely
- Defensive input validation — never trust user-provided data to be the right type