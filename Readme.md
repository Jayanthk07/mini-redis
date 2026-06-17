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
- RESP protocol parser (strips `*` and `$` framing lines)
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

Tested with `redis-benchmark -p 6379 -t set,get -n 10000`:

```
Throughput: 3,745 requests/sec
Avg latency: 4.992ms
p99 latency: 0.151ms
```

Bottleneck: one mutex per Store operation. A future optimization would be `std::shared_mutex` to allow concurrent reads, or a single-threaded event loop (epoll) like real Redis uses — eliminating locking overhead entirely.

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

---

## What I Learned

- How TCP servers work at the socket level (`socket`, `bind`, `listen`, `accept`, `send`, `recv`)
- RESP wire protocol — how Redis clients and servers actually communicate
- Runtime polymorphism with `virtual` functions and `dynamic_cast`
- Thread safety with `std::mutex` and `lock_guard` (RAII pattern)
- Why `shared_mutex` matters for read-heavy workloads
- How master-replica replication works and the CAP theorem tradeoffs it involves
- Why real Redis uses a single-threaded event loop instead of threads