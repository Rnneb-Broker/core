# Relay Network Architecture Documentation

## Table of Contents

1. [Project Overview](#project-overview)
2. [Design Approach](#design-approach)
3. [Supported Features](#supported-features)
4. [Architecture & Design Patterns](#architecture--design-patterns)
5. [Class Diagram](#class-diagram)
6. [Component Details](#component-details)
7. [Message Flow](#message-flow)
8. [Performance Benchmarking](#performance-benchmarking)
9. [Usage Examples](#usage-examples)

---

## Project Overview

**Relay** is a high-performance peer-to-peer (P2P) message relay network implemented in C++17. The application provides a distributed messaging system where nodes can communicate with each other through intermediate relay nodes, enabling message propagation across a network topology.

### Key Characteristics

-   **Language**: C++17
-   **Async Framework**: Boost.Asio for non-blocking I/O
-   **Build System**: CMake + Conan
-   **Architecture**: Event-driven, asynchronous messaging
-   **Network Protocol**: TCP-based custom binary protocol

---

## Design Approach

### 1. **Event-Driven Asynchronous Architecture**

The application leverages **Boost.Asio** to implement a fully asynchronous, non-blocking event-driven system. Each node operates its own I/O context that handles:

-   Accepting incoming connections
-   Establishing outgoing connections
-   Reading/writing messages asynchronously
-   Managing connection lifecycle

### 2. **Separation of Concerns**

The codebase follows clean architecture principles with clear separation between:

-   **Core Logic** (`core/`): Node management, routing, peer connections
-   **User Interface** (`ui/`): CLI interface for interaction
-   **Benchmarking** (`benchmark/`): Performance testing framework

### 3. **Composition Over Inheritance**

Classes are designed with composition in mind:

-   `Node` contains `PeerManager` and `Router`
-   `Router` uses references to `PeerManager` and `Node`
-   Minimal inheritance, mostly using composition and callbacks

### 4. **Resource Management**

-   **RAII** (Resource Acquisition Is Initialization) for automatic cleanup
-   Smart pointers (`std::unique_ptr`, `std::shared_ptr`) for memory safety
-   Move semantics to avoid unnecessary copies
-   Deleted copy constructors where appropriate

---

## Supported Features

### Core Functionality

[+] **Multi-Node Network**

-   Create and manage multiple network nodes simultaneously
-   Each node operates independently with its own I/O context

[+] **Peer-to-Peer Connections**

-   Nodes can connect to other nodes via TCP
-   Bidirectional communication channels
-   Automatic peer lifecycle management

[+] **Message Routing**

-   Flood routing algorithm with TTL (Time-To-Live) mechanism
-   Messages forwarded to all peers except the sender
-   Automatic message delivery to destination nodes

[+] **Interactive CLI**

-   Command-line interface for network management
-   Runtime node creation and connection
-   Message sending between nodes
-   Network topology visualization

[+] **Performance Benchmarking**

-   Automated benchmark suite
-   Latency measurement (p50, p95, p99 percentiles)
-   Throughput calculation (messages per second)
-   Various topology support (server-client, custom)

[+] **Configurable Network Topologies**

-   Support for different network structures
-   Loading topology from configuration files
-   Dynamic topology modification at runtime

---

## Architecture & Design Patterns

### Design Patterns Used

#### 1. **Observer Pattern**

-   Implemented through callback handlers
-   `Node::set_receive_handler()` allows observers to subscribe to message events
-   Used in both CLI and Benchmark modes

```cpp
node->set_receive_handler([](uint64_t node_id, uint64_t from_id, const std::string &msg) {
    // Custom message handling logic
});
```

#### 2. **Factory Pattern**

-   `Benchmark::generate_nodes()` acts as a factory method
-   Generates multiple nodes with sequential IDs and ports

#### 3. **Manager Pattern**

-   `PeerManager` manages collection of peer connections
-   `CliManager` manages collection of client nodes
-   Centralized resource management and iteration

#### 4. **Singleton-like Thread Pool**

-   Each `Node` has its own `io_context` running in a dedicated thread
-   Work guard pattern to keep `io_context` alive

#### 5. **Command Pattern**

-   CLI commands parsed and executed through `CliManager::process_command()`
-   Each command encapsulates an action

#### 6. **Producer-Consumer Pattern**

-   Message write queue in `PeerConnection`
-   Asynchronous writing without blocking

---

## Class Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                          CliManager                              │
├─────────────────────────────────────────────────────────────────┤
│ - clients_: map<uint64_t, ClientInfo>                           │
│ - clients_mutex_: mutex                                          │
│ - running_: bool                                                 │
├─────────────────────────────────────────────────────────────────┤
│ + run()                                                          │
│ + add_client(id, port)                                           │
│ + connect_client(client_id, host, port)                          │
│ + send_message(from_id, to_id, message)                          │
│ + list_clients()                                                 │
│ + stop_client(id)                                                │
└──────────────────────────┬──────────────────────────────────────┘
                           │ manages
                           ├─────────────────┐
                           ▼                 ▼
┌────────────────────────────────┐   ┌──────────────────────┐
│           Node                 │   │    Benchmark         │
├────────────────────────────────┤   ├──────────────────────┤
│ - id_: uint64_t                │   │ - nodes_: vector<>   │
│ - io_: io_context              │   │ - sent_: atomic      │
│ - acceptor_: tcp::acceptor     │   │ - received_: atomic  │
│ - peers_: PeerManager          │   │ - dropped_: atomic   │
│ - router_: Router              │   │ - latencies_ns_: []  │
│ - thread_: thread              │   ├──────────────────────┤
│ - work_: work_guard            │   │ + start()            │
│ - receive_handler_: callback   │   │ + wait_and_collect() │
├────────────────────────────────┤   │ + generate_nodes()   │
│ + run()                        │   └──────────────────────┘
│ + connect(endpoint)            │
│ + send(dst, data)              │
│ + set_receive_handler()        │
└────────┬───────────┬───────────┘
         │ has       │ has
         ▼           ▼
┌─────────────────┐ ┌──────────────────────────────┐
│  PeerManager    │ │         Router               │
├─────────────────┤ ├──────────────────────────────┤
│ - peers_: set   │ │ - self_id_: uint64_t         │
├─────────────────┤ │ - peers_: PeerManager&       │
│ + add(peer)     │ │ - node_: Node&               │
│ + remove(peer)  │ ├──────────────────────────────┤
│ + for_each(fn)  │ │ + on_message(msg, from)      │
└────────┬────────┘ │ - forward(msg, from)         │
         │          └──────────────────────────────┘
         │ contains
         ▼
┌───────────────────────────────────────┐
│        PeerConnection                 │
├───────────────────────────────────────┤
│ - socket_: tcp::socket                │
│ - router_: Router&                    │
│ - header_: MessageHeader              │
│ - body_: vector<uint8_t>              │
│ - write_queue_: deque<vector<>>       │
├───────────────────────────────────────┤
│ + start()                             │
│ + async_send(msg)                     │
│ - read_header_()                      │
│ - read_body_()                        │
│ - write_next_()                       │
└───────────────────────────────────────┘

┌──────────────────────────────────────┐
│         Message                      │
├──────────────────────────────────────┤
│ + header: MessageHeader              │
│   - version: uint8_t                 │
│   - type: uint16_t                   │
│   - src_node_id: uint64_t            │
│   - dst_node_id: uint64_t            │
│   - size: uint32_t                   │
│   - ttl: uint16_t                    │
│ + payload: vector<uint8_t>           │
└──────────────────────────────────────┘
```

---

## Component Details

### 1. Node

**Purpose**: Core network node that can send/receive messages

**Responsibilities**:

-   Manage TCP acceptor for incoming connections
-   Establish outgoing connections to peers
-   Run dedicated I/O context in separate thread
-   Coordinate between PeerManager and Router
-   Provide callback mechanism for message reception

**Key Methods**:

-   `run()`: Starts the accept loop and spawns I/O thread
-   `connect(endpoint)`: Connects to another node
-   `send(dst, data)`: Sends message to destination via all peers
-   `set_receive_handler()`: Sets custom message handler

### 2. PeerConnection

**Purpose**: Represents a single TCP connection to/from a peer

**Responsibilities**:

-   Handle asynchronous reading of message header and body
-   Maintain write queue for outgoing messages
-   Parse binary protocol messages
-   Forward received messages to Router

**Key Features**:

-   Uses `enable_shared_from_this` for safe async operations
-   Binary protocol with fixed-size header
-   Automatic message serialization/deserialization
-   Queue-based writing to prevent concurrent write issues

### 3. PeerManager

**Purpose**: Container and manager for all peer connections

**Responsibilities**:

-   Store active peer connections
-   Add/remove peers safely
-   Provide iteration over peers

**Key Feature**:

-   Template-based `for_each()` for functional operations on all peers

### 4. Router

**Purpose**: Message routing and forwarding logic

**Responsibilities**:

-   Determine if message is for this node or needs forwarding
-   Implement flood routing with TTL
-   Prevent routing loops (don't send back to sender)
-   Invoke receive handler for local messages

**Routing Algorithm**:

```
1. Check if message destination matches self_id
   → YES: Invoke receive handler, DONE
   → NO: Continue to step 2

2. Check TTL (Time-To-Live)
   → TTL == 0: Drop message, DONE
   → TTL > 0: Decrement TTL, continue to step 3

3. Forward message to all peers EXCEPT the sender
```

### 5. CliManager

**Purpose**: Interactive command-line interface for network management

**Responsibilities**:

-   Parse user commands
-   Create and manage multiple nodes at runtime
-   Establish connections between nodes
-   Send test messages
-   Display network status

**Supported Commands**:

-   `add <id> <port>` - Create new node
-   `connect <client_id> <host> <port>` - Connect node to peer
-   `send <from_id> <to_id> <message>` - Send message
-   `list` - Show all active nodes
-   `stop <id>` - Stop specific node
-   `stopall` - Stop all nodes
-   `help` - Show help
-   `quit/exit` - Exit application

### 6. Benchmark

**Purpose**: Performance testing and measurement

**Responsibilities**:

-   Generate network topology with N nodes
-   Measure message latency (p50, p95, p99)
-   Calculate throughput (messages/second)
-   Track sent/received/dropped message counts
-   Provide statistical analysis

**Metrics**:

-   **Latency**: End-to-end message delivery time in nanoseconds
-   **Throughput**: Messages per second
-   **Reliability**: Sent vs received vs dropped counts
-   **Percentiles**: p50, p95, p99 for latency distribution

### 7. Message

**Purpose**: Binary protocol message structure

**Format**:

```
┌─────────────────────────────────────────┐
│          MessageHeader (29 bytes)        │
├──────────────┬──────────────────────────┤
│ version (1)  │ Always 1                 │
│ type (2)     │ MessageType::Data = 1    │
│ src_id (8)   │ Source node ID           │
│ dst_id (8)   │ Destination node ID      │
│ size (4)     │ Payload size in bytes    │
│ ttl (2)      │ Time-to-live counter     │
├──────────────┴──────────────────────────┤
│          Payload (variable)              │
│          (size bytes)                    │
└──────────────────────────────────────────┘
```

---

## Message Flow

### Scenario: Node A sends message to Node C through Node B

```
┌──────┐               ┌──────┐               ┌──────┐
│  A   │               │  B   │               │  C   │
│ (1)  │               │ (2)  │               │ (3)  │
└───┬──┘               └───┬──┘               └───┬──┘
    │                      │                      │
    │ 1. send(3, "hello")  │                      │
    │────────────────────► │                      │
    │   [src=1, dst=3,     │                      │
    │    ttl=2, "hello"]   │                      │
    │                      │                      │
    │                      │ 2. Router checks:    │
    │                      │    dst_id != self_id │
    │                      │    ttl > 0           │
    │                      │                      │
    │                      │ 3. Forward(ttl=1)    │
    │                      │─────────────────────►│
    │                      │   [src=1, dst=3,     │
    │                      │    ttl=1, "hello"]   │
    │                      │                      │
    │                      │                      │ 4. Router checks:
    │                      │                      │    dst_id == self_id
    │                      │                      │
    │                      │                      │ 5. Deliver to app
    │                      │                      │    receive_handler(
    │                      │                      │      3, 1, "hello")
```

### Flow Explanation:

1. **Node A** calls `send(3, "hello")`

    - Creates Message with src=1, dst=3, ttl=2
    - Sends to all peers (Node B)

2. **Node B's PeerConnection** receives message

    - Reads header + body asynchronously
    - Passes to Router

3. **Node B's Router** processes message

    - Checks: dst_id (3) ≠ self_id (2) → not for me
    - Checks: ttl (2) > 0 → can forward
    - Decrements TTL to 1
    - Forwards to all peers except sender

4. **Node C's PeerConnection** receives message

    - Reads header + body asynchronously
    - Passes to Router

5. **Node C's Router** processes message
    - Checks: dst_id (3) == self_id (3) → for me!
    - Calls `receive_handler(3, 1, "hello")`
    - Message delivered to application

---

## Performance Benchmarking

### Benchmark Methodology

The benchmark system creates a network topology and measures:

1. **Message Generation**

    - Messages sent at controlled rate (~5000 msg/s)
    - Each message contains timestamp and sequence number
    - Messages sent for specified duration (e.g., 60 seconds)

2. **Latency Measurement**

    - Timestamp embedded in message payload
    - Latency = receive_time - send_time
    - Stored in vector for percentile calculation

3. **Topology**
    - Server-client: One central node, all others connect to it
    - Can be extended for other topologies (ring, mesh, etc.)

### Results (from `results.md`)

**Typical Performance** (Server-Client topology):

-   **Throughput**: ~5000 messages/second
-   **p50 Latency**: 11-24 microseconds
-   **p95 Latency**: 12-67 microseconds
-   **p99 Latency**: 13-145 microseconds
-   **Reliability**: 0 dropped messages (100% delivery)

**Scalability**:

-   Works well from 2 to 4181 nodes
-   Latency remains relatively stable
-   Some variance under system load

---

## Usage Examples

### Example 1: Simple Two-Node Communication

```bash
# Terminal 1 - Start the application
./build/relay

# In the CLI
add 1 10000        # Create node 1 on port 10000
add 2 10001        # Create node 2 on port 10001
connect 2 127.0.0.1 10000  # Connect node 2 to node 1
send 1 2 Hello World!      # Send message from 1 to 2
```

**Output**:

```
[CLIENT 2] <<< Message from 1: Hello World!
```

### Example 2: Multi-Node Relay Network

```bash
# Create 3 nodes
add 1 10000
add 2 10001
add 3 10002

# Create topology: 1 ←→ 2 ←→ 3
connect 2 127.0.0.1 10000
connect 2 127.0.0.1 10002

# Send from node 1 to node 3 (goes through node 2)
send 1 3 Message relayed through node 2
```

### Example 3: Loading Topology from File

The application supports loading predefined topologies:

```bash
# assets/map.txt contains:
add 1 8080
add 2 8081
connect 1 127.0.0.1 8081
...
```

### Example 4: Benchmark Mode

```cpp
// In main.cpp (uncomment benchmark code)
auto nodes = Benchmark::generate_nodes(100, PORT_BASE);
Benchmark bench(nodes, MINUTES(1));
bench.start();
auto result = bench.wait_and_collect();
```

---

## Technical Design Decisions

### Why Boost.Asio?

-   Industry-standard async I/O library
-   Cross-platform (Linux, Windows, macOS)
-   Zero-copy operations where possible
-   Efficient event loop implementation
-   Well-tested and mature

### Why Flood Routing?

-   Simple implementation
-   Guarantees message delivery (if path exists)
-   No routing table maintenance
-   Good for small to medium networks
-   TTL prevents infinite loops

**Trade-offs**:

-   Higher network overhead (messages duplicated)
-   Not suitable for very large networks
-   Future improvement: Implement DHT or distance-vector routing

### Why Binary Protocol?

-   Compact representation (29-byte header)
-   Fast serialization/deserialization
-   No parsing overhead (like JSON/XML)
-   Fixed-size header enables efficient reading
-   Low memory footprint

### Why Thread-Per-Node?

-   Isolation: Each node operates independently
-   Simplicity: No complex thread pooling
-   Real-world simulation: Each node is autonomous
-   Trade-off: Limited by thread count (use connection pooling for production)

---

## Future Enhancements

### Potential Improvements:

1. **Routing Algorithms**

    - Implement Dijkstra's shortest path
    - Distance-vector routing
    - DHT (Distributed Hash Table)

2. **Network Features**

    - Node discovery protocol
    - NAT traversal
    - Encryption (TLS/SSL)
    - Authentication

3. **Reliability**

    - Message acknowledgment
    - Retry mechanism
    - Persistent storage

4. **Scalability**

    - Connection pooling
    - Message batching
    - Compression

5. **Monitoring**
    - Real-time dashboard (web UI)
    - Grafana integration
    - Distributed tracing

---

## Conclusion

The Relay network is a well-architected, high-performance P2P messaging system demonstrating:

-   Clean separation of concerns
-   Event-driven asynchronous design
-   Robust resource management
-   Comprehensive benchmarking
-   Extensible architecture

The codebase follows modern C++ best practices and provides a solid foundation for building distributed messaging applications or learning about network programming and P2P systems.

---

**Project Statistics**:

-   **Lines of Code**: ~1000+ (source files)
-   **Classes**: 7 main classes
-   **Design Patterns**: 6+ patterns
-   **Performance**: Sub-100μs latency, 5000+ msg/s throughput
-   **Build System**: CMake + Conan
-   **Dependencies**: Boost, SDL2

---
