# Complete Documentation Overview

## 📚 Documentation Structure

This document shows you all available documentation and where to find it.

---

## 🚀 Quick Start (Start Here!)

1. **For Broker Setup**: [README.md](../README.md) → INSTALL section
2. **For JavaScript Client**: [client/README.md](../client/README.md)
3. **For Running Examples**: [client/QUICKSTART.sh](../client/QUICKSTART.sh)

---

## 🏗️ Architecture & Design

### [ARCHITECTURE.md](ARCHITECTURE.md)
High-level system design, component interactions, and data flow.

**Topics:**
- Core components overview
- Message flow diagrams
- Data structures (SegmentLog, Buffer pool, Sparse index)
- Thread safety model
- Connection lifecycle

**Read this if:** You want to understand system design or contribute to core components.

---

## 📖 Main Documentation

### [README.md](../README.md)
Primary documentation for the complete Highway Broker system.

**Sections:**
- What is Highway Broker?
- Features overview
- Installation instructions
- Building from source
- Running the broker
- API overview

**Read this if:** This is your entry point. Start here!

---

## 🔗 Integration Guide

### [INTEGRATION.md](INTEGRATION.md)
Complete guide for integrating C++ broker with JavaScript clients.

**Sections:**
- Architecture diagram (full system)
- Building complete system step-by-step
- Protocol details and packet structures
- Broker configuration tuning
- Client configuration examples
- Scalability considerations (throughput, storage, bandwidth)
- Monitoring and debugging
- Hands-on telemetry example
- Deployment checklist
- Troubleshooting matrix

**Read this if:** You're deploying the system or need protocol details.

---

## 💻 JavaScript Client Documentation

### [client/README.md](./../client/README.md)
Overview and quick start for Node.js client.

**Sections:**
- Installation
- Quick start example
- Feature overview
- Basic usage

**Read this if:** You're new to the JavaScript client.

### [client/API.md](./../client/API.md)
Complete API reference for HighwayClient class.

**Sections:**
- Constructor with all options
- Core methods (connect, subscribe, publish, disconnect)
- All events (connect, message, error, close, suback, puback)
- QoS explanation
- Complete examples
- Data types and structures
- Error handling patterns
- Performance tips
- Limits and specifications

**Read this if:** You're writing code with the client.

### [client/TESTING.md](./../client/TESTING.md)
Guide for running all example code and testing the system.

**Sections:**
- Prerequisites setup
- Test 1: Basic Pub/Sub
- Test 2: Multiple Subscribers
- Test 3: Real-Time Monitor
- Test 4: Message Persistence
- Test 5: High-Volume Publisher
- Test 6: Error Handling
- Performance benchmarks
- Debug mode setup
- Troubleshooting

**Read this if:** You want to run and test the system.

### [client/QUICKSTART.sh](./../client/QUICKSTART.sh)
Automated verification script.

**What it does:**
- Checks Node.js version
- Lists directory structure
- Shows how to run examples
- Provides quick usage patterns

**Run this:** `bash client/QUICKSTART.sh`

---

## 📁 Example Code Files

### [client/examples/consumer.js](./../client/examples/consumer.js)
Example: Basic message consumer.

**Features:**
- Subscribe to multiple topics with wildcards
- Handle received messages
- Graceful shutdown with Ctrl+C

**Usage:** `node examples/consumer.js`

### [client/examples/producer.js](./../client/examples/producer.js)
Example: Publish simulated telemetry data.

**Features:**
- Publish JSON sensor data
- Automatic timestamp generation
- 60-second runtime with random sensor IDs

**Usage:** `node examples/producer.js`

### [client/examples/monitor.js](./../client/examples/monitor.js)
Example: Real-time traffic monitoring.

**Features:**
- Collect statistics per sensor
- Calculate min/max/average speeds
- Detect and display alerts
- Print formatted stats every 10 seconds

**Usage:** `node examples/monitor.js`

---

## 🎯 Core Library Files

### [client/highway-client.js](./../client/highway-client.js)
Main JavaScript MQTT-lite client library.

**Features:**
- Zero external dependencies
- Binary protocol implementation
- Event-driven architecture
- Automatic reconnection
- QoS 0/1/2 support
- Topic wildcards

**Import:** `const { HighwayClient, QoS } = require('./highway-client.js')`

---

## 🔍 C++ Source Documentation

### [ARCHITECTURE.md](ARCHITECTURE.md)#Storage-System
Detailed explanation of C++ storage layer.

**Topics covered:**
- SegmentLog architecture
- FlushWorker background thread
- Buffer pool design
- Sparse index structure
- CRC32 validation
- Recovery on startup

### [src/broker/](src/broker/)
Core C++ implementation files.

Key files:
- `storage_manager.hpp` - Public storage API
- `storage_manager.cpp` - All durability implementations
- `broker.cpp` - Connection handling
- `session.hpp` - Session and QoS management

---

## 📊 Decision Tree: Which Doc to Read?

```
START HERE
    |
    ├─ "How do I install?" → README.md → INSTALL section
    |
    ├─ "How does it work?" → ARCHITECTURE.md
    |
    ├─ "How do I write JavaScript code?" → client/API.md
    |
    ├─ "How do I run examples?" → client/TESTING.md
    |
    ├─ "How do I deploy this?" → INTEGRATION.md
    |
    ├─ "How do I troubleshoot?" → INTEGRATION.md → Troubleshooting Matrix
    |
    └─ "I want to contribute" → ARCHITECTURE.md → Implementation Details
```

---

## 📋 Complete File Map

```
/
├── README.md                          # Main documentation
├── ARCHITECTURE.md                     # System design
├── INSTALL.md                          # Build & installation
├── INTEGRATION.md                      # Full system integration
├── CMakeLists.txt                      # Build configuration
├── build/                              # Compiled binaries
│   ├── broker                          # Main broker executable
│   ├── sensor_simulator                # Test data generator
│   └── traffic_monitor                 # CLI monitoring tool
│
├── src/
│   ├── broker/
│   │   ├── storage_manager.hpp
│   │   ├── storage_manager.cpp         # All durability TODOs implemented
│   │   ├── broker.cpp
│   │   └── ...
│   ├── client/                         # C++ client (optional)
│   └── protocol/                       # MQTT-lite protocol definitions
│
├── client/                             # JavaScript client directory
│   ├── highway-client.js               # Main client library (500+ lines)
│   ├── package.json                    # Node.js metadata
│   │
│   ├── README.md                       # Quick start
│   ├── API.md                          # Complete API reference
│   ├── TESTING.md                      # Testing guide
│   ├── QUICKSTART.sh                   # Verification script
│   │
│   ├── examples/
│   │   ├── consumer.js                 # Subscribe and receive
│   │   ├── producer.js                 # Publish telemetry
│   │   └── monitor.js                  # Real-time stats
│   
├── include/                            # C++ header files
│   ├── broker/
│   ├── protocol/
│   └── ...
│
├── tests/                              # C++ tests
│   └── storage_manager_test.cpp
│
└── assets/                             # Documentation assets
```

---

## 🎓 Learning Paths

### Path 1: "I want to use the broker with JavaScript"
1. [README.md](README.md) - Understand what it is
2. [client/README.md](./../client/README.md) - Get started
3. [client/API.md](./../client/API.md) - Learn the API
4. [client/TESTING.md](./../client/TESTING.md) - Run examples
5. [INTEGRATION.md](INTEGRATION.md) - Deploy

### Path 2: "I want to understand the system design"
1. [ARCHITECTURE.md](ARCHITECTURE.md) - Read full design
2. [README.md](README.md) - Understand features
3. [INTEGRATION.md](INTEGRATION.md) - See it in action
4. [src/broker/storage_manager.cpp](src/broker/storage_manager.cpp) - Study code

### Path 3: "I want to modify/extend the system"
1. [ARCHITECTURE.md](ARCHITECTURE.md) - Understand design
2. [src/broker/storage_manager.hpp](src/broker/storage_manager.hpp) - Study interfaces
3. [src/broker/storage_manager.cpp](src/broker/storage_manager.cpp) - Study implementation
4. [client/highway-client.js](./../client/highway-client.js) - Understand protocol
5. Build and test locally

### Path 4: "I want to deploy and monitor"
1. [README.md](README.md) - Installation
2. [INTEGRATION.md](INTEGRATION.md) - System setup
3. [INTEGRATION.md](INTEGRATION.md)#Monitoring-&-Debugging - Monitor system
4. [client/examples/monitor.js](./../client/examples/monitor.js) - Use monitoring tool

---

## 🔧 Configuration Files

### [CMakeLists.txt](CMakeLists.txt)
C++ build configuration. Key variables:
- `CMAKE_BUILD_TYPE` - Release/Debug
- `CONAN_BUILD_POLICY` - Build from source if needed

### [conanfile.txt](conanfile.txt)
Dependency management (Boost, GTest, libbacktrace)

### [client/package.json](./../client/package.json)
Node.js project metadata

---

## 📞 Quick Reference

### Build the broker
```bash
cd build && cmake .. && make -j$(nproc)
./broker
```

### Run JavaScript examples
```bash
cd client
node examples/consumer.js    # Window 1
node examples/producer.js    # Window 2
node examples/monitor.js     # Window 3
```

### Read API documentation
```bash
# View in terminal
less client/API.md

# Or open in editor
code client/API.md
```

### Run tests
```bash
cd build && ctest
```

### Check broker logs
```bash
./broker 2>&1 | tee broker.log
```

---

## 🚨 Troubleshooting Quick Links

| Issue | Solution |
|-------|----------|
| Build fails | See [INSTALL.md](INSTALL.md) → Troubleshooting |
| Broker won't start | See [INTEGRATION.md](INTEGRATION.md) → Troubleshooting Matrix |
| Client connection refused | See [INTEGRATION.md](INTEGRATION.md) → Troubleshooting Matrix |
| Messages not persisting | See [ARCHITECTURE.md](ARCHITECTURE.md) → Storage System |
| Performance poor | See [INTEGRATION.md](INTEGRATION.md) → Scalability Considerations |

---

## 📈 Documentation Maintenance

Last updated with all document links:
- [+] README.md - Main documentation
- [+] ARCHITECTURE.md - System design
- [+] INSTALL.md - Build instructions
- [+] INTEGRATION.md - Complete integration guide (NEW)
- [+] client/README.md - Client quick start
- [+] client/API.md - Complete JavaScript API reference (NEW)
- [+] client/TESTING.md - Testing and examples guide (NEW)
- [+] client/QUICKSTART.sh - Automated verification

---

## 🎯 Next Steps

1. **Choose your learning path** above
2. **Start with most relevant documentation**
3. **Build and run examples**
4. **Read [INTEGRATION.md](INTEGRATION.md) for deployment**
5. **Explore [ARCHITECTURE.md](ARCHITECTURE.md) for system details**

**Questions?** Check the documentation index above.

**Ready to code?** Start with [client/API.md](./../client/API.md).

**Want to deploy?** Follow [INTEGRATION.md](INTEGRATION.md).
