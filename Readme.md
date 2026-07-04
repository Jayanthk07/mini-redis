# mini-redis

A Redis-compatible TCP server built from scratch in C++. Implements the RESP protocol, core Redis commands, polymorphic data types, master-replica replication, and is containerized with Docker.

---

## Architecture

```
Client
  ↓
RedisServer (TCP, one thread per client)
  ↓
ClientHandler (parses RESP, routes commands)
  ↓              ↓
Store        propagate() → Replica
(mutex-safe)
unordered_map<string, DataType*>
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

- TCP server with one thread per client connection
- RESP protocol parser (length-prefixed, binary-safe — reads declared `$<len>` byte counts rather than scanning for markers)
- Commands: `PING`, `ECHO`, `SET`, `GET`, `DEL`, `EXISTS`, `EXPIRE`
- List commands: `LPUSH`, `RPUSH`, `LRANGE`, `LLEN`, `LREM`
- Polymorphic `DataType` base class with `StringValue` and `ListValue` subtypes
- Thread-safe `Store` using `std::mutex` and `lock_guard`
- Master-replica replication (`--master` / `--replica <port>` flags)
- Dockerized with `Dockerfile`

---

## OOP Design

| Class | Responsibility |
|---|---|
| `RedisServer` | Socket setup, accept loop, replica tracking |
| `ClientHandler` | Per-client thread, command routing |
| `Store` | Thread-safe in-memory key-value store |
| `CommandParser` | Parses raw RESP bytes into token vectors |
| `DataType` | Abstract base class for stored values |
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
redis-cli -p 6379 EXISTS name             # (integer) 0

redis-cli -p 6379 LPUSH mylist a b c      # (integer) 3
redis-cli -p 6379 LRANGE mylist 0 -1      # [c, b, a]
redis-cli -p 6379 LLEN mylist             # (integer) 3
redis-cli -p 6379 LREM mylist 1 a         # OK
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

Throughput scales sharply from 1→10 concurrent clients (single-connection requests are latency-bound), then plateaus — the single `std::mutex` guarding `Store` becomes the limiting factor beyond ~10 concurrent clients rather than raw CPU capacity.

**Payload handling**: verified with a 1024-byte payload (`-d 1024`) after fixing the fixed-size read buffer and moving to length-prefixed RESP parsing — see Known Limitations.

**Bottleneck**: one mutex per `Store` operation. A future optimization would use `std::shared_mutex` for concurrent reads, or a single-threaded event loop (epoll) like real Redis — eliminating locking overhead entirely.

---
## Known Limitations

- **No streaming/partial-read support**: `ClientHandler` assumes one `recv()` call contains exactly one complete RESP command. Large payloads or commands split across multiple TCP reads aren't reassembled — the server responds with a protocol error rather than corrupting state. Proper fix requires a persistent per-connection buffer that accumulates bytes until a full command is parseable.
- **No command pipelining**: follows from the above — multiple commands sent back-to-back without waiting for individual replies aren't currently supported.
- **Type-mismatch errors are shallow**: `WRONGTYPE` is detected via a length-comparison workaround in `ClientHandler` rather than a clean type check surfaced from the `Store` layer.

---
## Design Tradeoffs

**One thread per client vs event loop:**
Chose threads for simplicity. Real Redis uses a single-threaded event loop (`epoll`) which eliminates locking overhead and handles thousands of concurrent connections efficiently. Thread-per-client works fine at small scale but doesn't scale to 10k+ connections.

**`std::mutex` vs `std::shared_mutex`:**
Used `std::mutex` for simplicity — every read and write locks exclusively. A `shared_mutex` would allow multiple concurrent readers (only writers block), improving read-heavy workload performance significantly.

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
| Memory leak on key overwrite | `Store::set` allocated a new `DataType*` without freeing the previous value at that key | `delete` old pointer before overwriting map entry |
---

## What I Learned

- How TCP servers work at the socket level (`socket`, `bind`, `listen`, `accept`, `send`, `recv`)
- RESP wire protocol — how Redis clients and servers actually communicate
- Runtime polymorphism with `virtual` functions and `dynamic_cast`
- Thread safety with `std::mutex` and `lock_guard` (RAII pattern)
- Why `shared_mutex` matters for read-heavy workloads
- How master-replica replication works and the CAP theorem tradeoffs it involves
- Why real Redis uses a single-threaded event loop instead of threads
- Why RESP's explicit length-prefixing exists — a naive parser that scans for `*`/`$` markers breaks the moment binary payload data collides with those bytes; length-prefixing avoids ambiguity entirely