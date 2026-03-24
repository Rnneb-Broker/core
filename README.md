# RabbitBroker - High-Performance Message Broker System

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/Rnneb-Broker/core)
[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)](https://cplusplus.com/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

A **production-grade, high-performance message broker** built in C++17 with MQTT-lite protocol support. Designed for real-time telemetry and event streaming with persistent message storage, multi-language client support, and sub-millisecond latency.

## Key Features

### Core Capabilities

- High-Throughput Async I/O: Event-driven architecture with Boost.Asio, configurable thread pools
- Persistent Storage: Double-buffered segment logs with CRC validation and automatic recovery
- MQTT-Compatible Protocol: Connect, Publish, Subscribe with QoS levels and wildcard support
- Multi-Language Support: C++, JavaScript/Node.js, and Python clients
- Offset-Based Message Access: Query and replay messages by offset or fetch from stored logs

### Performance Characteristics

- Lock-Free Append Path: Single-writer principle with atomic buffer swaps
- Sparse Indexing: O(log N) message lookup for efficient replay
- Sub-Millisecond Latency: Optimized serialization and zero-copy where possible
- Configurable Durability: FAST (100ms flush), BALANCED (on full), SAFE (per-message)

### Production Ready

- Graceful Shutdown: Proper cleanup of connections and resources
- Error Recovery: Segment validation and automatic recovery on startup
- Thread-Safe: Comprehensive synchronization with proper memory ordering
- Scalable: Support for 10,000+ concurrent connections per broker instance

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│           CLIENT LIBRARIES                           │
│  (C++ / JavaScript / Python)                         │
└────────────┬────────────────┬──────────────┬─────────┘
             │                │              │
      ┌──────▼─────────────────▼──────────┬──▼────────┐
      │    MQTT-Lite Binary Protocol      │           │
      └──────┬──────────────────────────────────────┬─┘
             │                                        │
      ┌──────▼────────────────────────────────────┬──▼────┐
      │        RABBIT BROKER                      │        │
      │  ┌──────────────┐  ┌──────────────┐      │        │
      │  │   Session    │  │  Topic/Subs  │      │        │
      │  │  Manager     │  │  Manager     │      │        │
      │  └──────────────┘  └──────────────┘      │        │
      │                                            │        │
      │  ┌──────────────────────────────────┐    │        │
      │  │   Storage Manager                │    │        │
      │  │  - Double Buffering              │    │        │
      │  │  - Segment Logs (512MB each)     │    │        │
      │  │  - Sparse Index                  │    │        │
      │  │  - Flush Worker                  │    │        │
      │  └──────────────────────────────────┘    │        │
      └────────────────────────────────────────────────────┘
             │              │
      ┌──────▼──────┬──────▼────────┐
      │   Disk I/O  │  Retention    │
      │ (Segments)  │  Cleanup      │
      └─────────────┴───────────────┘
```

## Components

### Broker Core

- **Broker**: Central coordinator managing connections, message routing, and subscriptions
- **Session**: Per-client connection handler with protocol parsing and state machine
- **TopicManager**: Topic registry with MQTT wildcard pattern matching
- **SubscriptionManager**: Subscription tracking and message routing

### Storage System

- **StorageManager**: Central registry for topic logs and retention management
- **SegmentLog**: Per-topic append-only log with double-buffering and atomic buffer swaps
- **Buffer**: Fixed-size circular write buffer optimized for O(1) append
- **FlushWorker**: Background thread for durable disk writes with configurable modes
- **SparseIndex**: One-per-1024 messages index enabling O(log N) message lookup

### Protocol

- **PacketSerializer**: Binary protocol encoding/decoding
- **BinaryReader/Writer**: Network byte order (big-endian) serialization utilities

### Clients

- **C++ Client**: Async Boost.Asio-based client for publishers and subscribers
- **JavaScript Client**: Node.js and browser-based WebSocket/TCP client
- **Python Client**: Native Python client with sync/async support

## Message Format

All messages are stored in a binary format optimized for sequential I/O:

```
┌──────────┬───────┬─────┬─────────────┐
│ Offset   │ Size  │ CRC │  Payload    │
│ 8 bytes  │4 bytes│4 byt│ size bytes  │
└──────────┴───────┴─────┴─────────────┘

Total header: 16 bytes (fixed)
CRC32 computed over payload only
Enables fast validation and recovery
```

## System Requirements

### Build Requirements

- C++17 Compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15 or later
- Conan package manager
- Boost 1.84+ (Asio component)
- GTest 1.14+ (for unit tests)

### Runtime Requirements

- Linux/macOS/Windows: POSIX-compatible OS
- Memory: Minimum 256MB (scales with buffer sizes and connections)
- Disk: Configurable; segments rotate at 512MB by default

## Getting Started

### Installation and Build

```bash
# Clone the repository
git clone https://github.com/Rnneb-Broker/core.git
cd rabbit-broker

# Create build directory
mkdir -p build && cd build

# Install dependencies via Conan
conan install .. --build=missing

# Configure and build with CMake
cmake ..
make -j$(nproc)

# Run tests to verify installation
ctest
```

### Launch Broker

```bash
# Terminal 1: Start the broker
./broker

# Output:
# [BROKER] Starting on port 1883
# [BROKER] IO threads: 4
# [BROKER] Ready to accept connections
```

### Publish and Subscribe

```bash
# Terminal 2: Start sensor simulator (C++)
./sensor_simulator 127.0.0.1 1883 10

# Terminal 3: Start traffic monitor (C++)
./traffic_monitor 127.0.0.1 1883

# Or use JavaScript examples:
cd ../client
node examples/producer.js       # Publish sensor data
node examples/consumer.js       # Subscribe to topics
node examples/monitor.js        # Monitor with statistics
```

## Usage Examples

### C++ Publisher (Sensor)

```cpp
#include "client/client.hpp"
using namespace rabbit;

int main() {
    // Create and configure client
    Client::Config config;
    config.host = "127.0.0.1";
    config.port = 1883;
    config.client_id = "sensor_1";

    auto client = std::make_shared<Client>(config);

    // Connect to broker
    client->connect([](bool success) {
        if (success) std::cout << "Connected!\n";
    });

    // Publish sensor reading
    std::vector<uint8_t> payload = {0x10, 0x20, 0x30};
    client->publish("rabbit/sensor_1/telemetry", payload, QoS::AtLeastOnce);

    // Keep running
    client->run();
    return 0;
}
```

### C++ Subscriber (Consumer)

```cpp
#include "client/client.hpp"
using namespace rabbit;

int main() {
    Client::Config config;
    config.host = "127.0.0.1";
    config.port = 1883;
    config.client_id = "monitor_1";

    auto client = std::make_shared<Client>(config);

    // Set message handler
    client->set_message_handler([](const std::string& topic,
                                   const std::vector<uint8_t>& payload) {
        std::cout << "Topic: " << topic << " Payload size: " << payload.size() << "\n";
    });

    // Connect and subscribe with wildcard pattern
    client->connect();
    client->subscribe("rabbit/+/telemetry", QoS::AtMostOnce);

    client->run();
    return 0;
}
```

### JavaScript Producer

```javascript
const { Client } = require("./rabbit-client");

const client = new Client({
    host: "127.0.0.1",
    port: 1883,
    clientId: "sensor_js_1",
});

client.on("connect", () => {
    console.log("Connected to broker");

    // Publish every second
    setInterval(() => {
        const data = JSON.stringify({
            sensorId: "A1",
            speed: Math.random() * 120,
            timestamp: Date.now(),
        });

        client.publish("rabbit/A1/telemetry", data, { qos: 1 });
    }, 1000);
});

client.connect();
```

### JavaScript Consumer

```javascript
const { Client } = require("./rabbit-client");

const client = new Client({
    host: "127.0.0.1",
    port: 1883,
    clientId: "consumer_js_1",
});

client.on("connect", () => {
    console.log("Connected");
    client.subscribe("rabbit/+/telemetry", { qos: 0 });
});

client.on("message", (topic, payload) => {
    console.log(`[${topic}] ${payload.toString()}`);
});

client.connect();
```

### Python Producer

```python
from rabbit_client import Client

client = Client(
    host='127.0.0.1',
    port=1883,
    client_id='sensor_py_1'
)

def on_connect():
    print("Connected to broker")
    for i in range(100):
        data = json.dumps({
            'sensorId': 'A1',
            'speed': random.random() * 120,
            'timestamp': int(time.time() * 1000)
        })
        client.publish('rabbit/A1/telemetry', data, qos=1)

client.on_connect = on_connect
client.connect()
client.loop_forever()
```

## Wildcard Patterns

Rabbit uses MQTT-style wildcard patterns for flexible subscriptions:

| Pattern  | Description           | Example Matches                                                           |
| -------- | --------------------- | ------------------------------------------------------------------------- |
| `+`      | Single-level wildcard | `rabbit/+/telemetry` matches `rabbit/A1/telemetry`, `rabbit/A2/telemetry` |
| `#`      | Multi-level wildcard  | `rabbit/#` matches any topic under `rabbit/`                              |
| Combined | Both wildcards        | `rabbit/+/*/telemetry` matches `rabbit/A1/north/telemetry`                |

## Configuration

### Broker Configuration

```cpp
Broker::Config config;
config.port = 1883;              // Listen port (default: 1883)
config.max_connections = 10000;  // Max concurrent clients
config.io_threads = 4;           // Thread pool size
config.buffer_size = 65536;      // TCP buffer size
```

### Storage Configuration

```cpp
StorageManager::Config storage_config;
storage_config.data_dir = "./data";           // Where to store segments
storage_config.max_segment_size = 512 * 1024 * 1024;  // 512MB per segment
storage_config.flush_interval_ms = 100;       // Flush frequency (FAST mode)
storage_config.retention_hours = 24;          // Keep messages for 24 hours
storage_config.durability_mode = DurabilityMode::FAST;
```

## Subscription Modes

### PUSH_LIVE (Default)

Receives new messages in real-time as they arrive:

```cpp
client->subscribe("rabbit/+/telemetry", QoS::AtMostOnce);
```

### CATCHUP_THEN_PUSH (Replay)

Receive historical messages starting from offset, then switch to live:

```cpp
client->subscribe_from_offset("rabbit/+/telemetry", start_offset, QoS::AtMostOnce);
```

## Quality of Service (QoS)

| Level | Name        | Guarantee           | Use Case                                    |
| ----- | ----------- | ------------------- | ------------------------------------------- |
| 0     | AtMostOnce  | Fire and forget     | Non-critical data, high volume              |
| 1     | AtLeastOnce | Guaranteed delivery | Important events (may receive duplicates)   |
| 2     | ExactlyOnce | Single delivery     | Critical transactions (not yet implemented) |

## Performance Characteristics

- Throughput: 100,000+ messages/sec on modern hardware
- Latency: Sub-millisecond p99 for publish-to-receive
- Memory: ~100 bytes per active subscription
- Disk I/O: Sequential writes with configurable flush intervals
- Connections: Tested with 10,000+ concurrent clients

## Testing

### Run All Tests

```bash
cd build
ctest
```

### Run Specific Test Suite

```bash
./storage_manager_test
```

### Manual Testing with Examples

```bash
# Terminal 1
./broker

# Terminal 2
./sensor_simulator 127.0.0.1 1883 50

# Terminal 3
./traffic_monitor 127.0.0.1 1883
```

## Documentation

For detailed information, see:

- docs/INSTALLATION.md - Detailed build and installation instructions
- docs/QUICKSTART.md - Get running in 5 minutes
- docs/USAGE_EXAMPLES.md - Comprehensive code examples
- docs/ARCHITECTURE.md - System design and internals
- docs/INTEGRATION.md - Full system integration guide
- client/API.md - JavaScript client reference
- client/python/API.md - Python client reference

## Contributing

Please see CONTRIBUTING.md for guidelines on:

- Code style and standards
- Submitting pull requests
- Running tests and validation
- Development workflow

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support and Community

- Issues: Report bugs on GitHub Issues
- Discussions: Join community discussions on GitHub Discussions
- Documentation: Full docs available in /docs directory

## Roadmap

- QoS Level 2 (ExactlyOnce) implementation
- Cluster mode with broker replication
- WebSocket support for browser clients
- Authentication and authorization (ACL)
- Built-in metrics and monitoring dashboard
- Time-based message expiration (TTL per message)

## Acknowledgments

Built with:

- Boost.Asio - Async I/O
- GTest - Testing framework
- CMake - Build system
- Conan - Package management

---

For more information and examples, please refer to the complete documentation in the docs directory.
