# Integration Guide: C++ Broker + JavaScript Client

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                  Highway Broker (C++)                        │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Network Layer (TCP/MQTT-lite protocol)             │   │
│  │  - Handles multiple concurrent connections          │   │
│  │  - Packet serialization/deserialization             │   │
│  │  - QoS management (0, 1, 2)                         │   │
│  └──────────────────┬──────────────────────────────────┘   │
│                     ↓                                        │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Routing Engine                                     │   │
│  │  - Topic subscriptions with wildcard matching       │   │
│  │  - Message routing to subscribers                   │   │
│  │  - QoS acknowledgments                              │   │
│  └──────────────────┬──────────────────────────────────┘   │
│                     ↓                                        │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Storage Layer (Persistent message log)             │   │
│  │  - SegmentLog: 512MB max files per topic            │   │
│  │  - FlushWorker: Automatic background flushing       │   │
│  │  - SparseIndex: O(log N) offset lookup              │   │
│  │  - CRC32 validation: Data integrity checks          │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
               ↑                              ↑
        [TCP Port 1883]                 [Persistent Storage]
               ↑                              ↑
┌──────────────┴──────────────────────────────────────────────┐
│              JavaScript Clients (Node.js)                    │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  HighwayClient (highway-client.js)                  │   │
│  │  - Binary packet protocol                           │   │
│  │  - Event-driven architecture (EventEmitter)         │   │
│  │  - Automatic reconnection                           │   │
│  │  - Topic wildcards (+ and #)                        │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌──────────────┬──────────────┬─────────────────────────┐  │
│  │  Producer    │  Consumer    │  Monitor                │  │
│  │  - Publish   │  - Subscribe │  - Real-time stats      │  │
│  │  - QoS 0-2   │  - Receive   │  - Aggregate data       │  │
│  │  - Callbacks │  - Async     │  - Alerts               │  │
│  └──────────────┴──────────────┴─────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

---

## Building the Complete System

### Step 1: Build C++ Broker

```bash
cd /path/to/kafka-system

# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Verify broker executable
./broker --help
```

### Step 2: Set Up JavaScript Client

```bash
cd /path/to/kafka-system/client

# Verify Node.js
node --version  # Should be v18+

# Verify client files
ls -la *.js
# Should show: highway-client.js

# Verify examples
ls -la examples/
# Should show: consumer.js, producer.js, monitor.js
```

### Step 3: Run First Test

**Terminal 1 - Start Broker:**
```bash
cd /path/to/kafka-system/build
./broker
# Output: [INFO] Broker listening on port 1883
```

**Terminal 2 - Start Consumer:**
```bash
cd /path/to/kafka-system/client
node examples/consumer.js
# Output: [+] Connected to Highway Broker
#         Waiting for messages...
```

**Terminal 3 - Start Producer:**
```bash
cd /path/to/kafka-system/client
node examples/producer.js
# Output: Publishing sensor telemetry...
#         [Sensor 1001] Published: {...}
```

**Consumer Terminal Output:**
```
📨 Message received:
   Topic: highway/1001/telemetry
   Data: {"timestamp":"...","sensorId":"1001","speed":45,...}
```

---

## Protocol Details

### Message Flow

```
JavaScript Client               C++ Broker
      |                              |
      |------ TCP SYN ------------->|
      |<----- TCP ACK --------------|
      |                              |
      | CONNECT packet              |
      |------- (binary) ---------->|
      |                    [Validate credentials]
      |<------ CONNACK packet ------|
      |        (success)             |
      |                              |
      | SUBSCRIBE packet            |
      |------- (binary) ---------->|
      |          (topic: "highway/+/telemetry")
      |                    [Match subscriptions]
      |<------ SUBACK packet -------|
      |                              |
      |                         [Waiting for publish]
      |                              |
      |<--- PUBLISH packet ---------|
      |     (message from storage)   |
      |       (QoS determines next)  |
      |                              |
      | (QoS 0 = done)               |
      | (QoS 1) --- PUBACK packet -->|
      | (QoS 2) --- PUBREC packet -->|
      |            PUBREL packet <---|
      |            PUBCOMP packet -->|
```

### Packet Types

| Type | ID | Direction | Purpose |
|------|-----|-----------|---------|
| CONNECT | 1 | → | Client initiates connection |
| CONNACK | 2 | ← | Server accepts/rejects |
| PUBLISH | 3 | ↔ | Send message |
| PUBACK | 4 | ↔ | Acknowledge QoS 1 |
| SUBSCRIBE | 8 | → | Subscribe to topic |
| SUBACK | 9 | ← | Subscription acknowledged |
| DISCONNECT | 14 | → | Client disconnects |
| PINGREQ | 12 | → | Keepalive ping |
| PINGRESP | 13 | ← | Keepalive pong |

### Binary Format

All packets follow this structure:

```
[1 byte] [1-4 bytes] [variable]
  Type    Remaining    Payload
         Length

Example CONNECT:
┌──┬──┬──┬──┬──┬──┐
│01│.. │1883│.. │..│
└──┴──┴──┴──┴──┴──┘
 ^                ^
Type         Payload data
```

---

## Broker Configuration

### Runtime Options

The broker automatically:
1. **Creates storage directories**: `storage/highway/<topic>/`
2. **Enables persistent logging**: Each topic gets `.log` files
3. **Maintains sparse index**: One entry per 1024 messages
4. **Validates data**: CRC32 checksums on all writes
5. **Flushes periodically**: Background worker writes 16MB buffers

### Tuning Parameters (in C++):

```cpp
// In StorageManager::StorageManager()
const size_t BUFFER_SIZE = 16 * 1024 * 1024;  // 16MB buffers
const size_t FLUSH_THRESHOLD = BUFFER_SIZE * 0.95;  // 95% = 15.2MB
const size_t SEGMENT_MAX_SIZE = 512 * 1024 * 1024; // 512MB segments
const size_t SPARSE_INDEX_INTERVAL = 1024; // Index every 1024 messages
```

**For testing:** Defaults work fine
**For production:** Adjust based on disk I/O and memory

---

## Client Configuration

### Recommended Settings

```javascript
const client = new HighwayClient({
  // Connection
  host: process.env.BROKER_HOST || 'localhost',
  port: process.env.BROKER_PORT || 1883,
  
  // Client identity (should be unique per instance)
  clientId: `${require('os').hostname()}-${process.pid}`,
  
  // Authentication (optional)
  username: process.env.BROKER_USER,
  password: process.env.BROKER_PASS,
  
  // Keepalive (in seconds)
  keepalive: 60,
  
  // Auto-connect on creation
  autoConnect: true
});
```

### Topic Design

Follow MQTT conventions:

```
hardware/  - Physical sensor data
  └─ sensors/
     └─ {sensorId}/
        ├─ telemetry      # Continuous data
        ├─ alerts         # Threshold violations
        └─ status         # Online/offline

software/  - Application events
  └─ {appName}/
     ├─ logs              # Application logs
     ├─ errors            # Error events
     └─ metrics           # Performance metrics

system/    - Infrastructure
  └─ health
     ├─ cpu
     ├─ memory
     └─ disk
```

Example topics:
- `hardware/sensors/1001/telemetry` - Single sensor
- `hardware/sensors/+/telemetry` - All sensors telemetry
- `hardware/sensors/#` - All sensor topics
- `alerts/+` - All alert topics

---

## Scalability Considerations

### Message Volume Capacity

Based on reference system (Intel i7, 16GB RAM, SSD):

| Per Broker | Limit | Notes |
|-----------|-------|-------|
| Messages/sec | 5,000-10,000 | Depends on QoS |
| Concurrent clients | 500-1000 | Per CPU core |
| Topics | Unlimited | No hardcoded limit |
| Messages on disk | 1TB+ | Storage limited |
| Message size | 16MB | Protocol limit |

**To scale beyond this:**
1. Horizontal scaling: Multiple brokers per topic
2. Sharding: Route messages by key to different brokers
3. Clustering: Implement broker-to-broker replication

### Storage Requirements

Typical storage use:

```
Message size: 1KB
Topics: 10
Publish rate: 1000 msg/sec
Retention period: 24 hours

Storage = 1000 msg/sec × 86,400 sec × 10 topics × 1KB
        = ~860 GB per day

Allocation: 1-2 TB storage for 1-2 days retention
```

### Network Bandwidth

Typical network use:

```
1000 msg/sec × 1KB = 1MB/sec = 8Mbps per topic
With 10 topics: 80Mbps sustained

Recommendation: 1Gbps+ network for production
```

---

## Monitoring & Debugging

### Check Broker Status

```bash
# Process running
ps aux | grep broker

# Port listening
netstat -tlnp | grep 1883

# Storage usage
du -sh storage/

# File count
find storage -name "*.log" | wc -l
```

### Monitor Broker Logs

```bash
# If broker outputs to file:
tail -f broker.log

# Real-time stats:
watch -n 1 'du -sh storage/'
```

### Debug Client Issues

```javascript
// Add to consumer example for debugging
const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  clientId: 'debug-client'
});

// Log all state changes
setInterval(() => {
  console.log('[STATE]', {
    connected: client.isConnected?.(),
    state: client.getState?.(),
    subscriptions: client.getSubscriptions?.()
  });
}, 5000);

// Log all packets
client.on('message', (msg) => {
  console.log('[PACKET]', {
    to: msg.topic,
    bytes: msg.data.length,
    qos: msg.qos,
    time: new Date().toISOString()
  });
});
```

---

## Hands-On Example: Telemetry System

### 1. Create Telemetry Producer

**File: `examples/telemetry.js`**

```javascript
const { HighwayClient, QoS } = require('../highway-client.js');
const os = require('os');

const client = new HighwayClient({
  clientId: `telemetry-${os.hostname()}`
});

function getSensorReading() {
  return {
    timestamp: new Date().toISOString(),
    sensorId: 'SENSOR-001',
    cpuTemp: 35 + Math.random() * 30,
    memUsage: Math.random() * 100,
    diskUsage: Math.random() * 100,
    uptime: process.uptime()
  };
}

client.on('connect', () => {
  console.log('Telemetry producer connected');
  
  setInterval(() => {
    const data = getSensorReading();
    client.publish(
      'hardware/sensors/001/telemetry',
      JSON.stringify(data),
      QoS.AT_LEAST_ONCE
    );
  }, 2000);
});

client.on('error', (err) => {
  console.error('Error:', err.message);
});
```

### 2. Create Alert Subscriber

**File: `examples/alerts.js`**

```javascript
const { HighwayClient, QoS } = require('../highway-client.js');

const client = new HighwayClient({
  clientId: 'alert-monitor'
});

const THRESHOLDS = {
  cpuTemp: 80,
  memUsage: 90,
  diskUsage: 95
};

client.on('connect', () => {
  client.subscribe('hardware/sensors/+/telemetry', QoS.AT_LEAST_ONCE);
});

client.on('message', (msg) => {
  const data = JSON.parse(msg.data.toString());
  
  // Check thresholds
  if (data.cpuTemp > THRESHOLDS.cpuTemp) {
    console.log(`🔥 HIGH TEMP on ${data.sensorId}: ${data.cpuTemp}°C`);
  }
  if (data.memUsage > THRESHOLDS.memUsage) {
    console.log(`💾 HIGH MEMORY on ${data.sensorId}: ${data.memUsage}%`);
  }
  if (data.diskUsage > THRESHOLDS.diskUsage) {
    console.log(`💿 HIGH DISK on ${data.sensorId}: ${data.diskUsage}%`);
  }
});

client.on('error', (err) => {
  console.error('Error:', err.message);
});
```

### 3. Run the System

```bash
# Terminal 1: Broker
cd build && ./broker

# Terminal 2: Telemetry producer
cd client && node examples/telemetry.js

# Terminal 3: Alert subscriber
cd client && node examples/alerts.js

# Output:
# 🔥 HIGH TEMP on SENSOR-001: 85.2°C
# 💾 HIGH MEMORY on SENSOR-001: 92.1%
```

---

## Deployment Checklist

- [ ] Broker compiled in Release mode: `cmake -DCMAKE_BUILD_TYPE=Release`
- [ ] Storage directory writable: `chmod 755 storage/`
- [ ] Network connectivity verified: `ping broker-host`
- [ ] Port 1883 accessible: `telnet broker-host 1883`
- [ ] Node.js version ≥ 18: `node --version`
- [ ] Firewall rules allowing port 1883
- [ ] Process monitor for broker uptime (systemd/supervisor)
- [ ] Log rotation for persistent logs
- [ ] Disk space monitoring (alert when < 10% free)
- [ ] Backup strategy for persistent storage

---

## Troubleshooting Matrix

| Problem | Broker | Client | Resolution |
|---------|--------|--------|------------|
| Connection refused | ✗ | ✓ | Start broker: `./broker` |
| Timeout | ✓ | ✓ | Check network: `ping broker` |
| No messages received | ✓ | ✓ | Check topic match, verify wildcard syntax |
| Storage not persisting | ✓ | ✓ | Check disk permissions, available space |
| High memory usage | ✓ | | Reduce keepalive, enable compression |
| CPU at 100% | ✓ | | Profile broker, check message rate |

---

## Next Steps

1. **Deploy producer agents** - Stream real telemetry data
2. **Scale readers** - Multiple consumers per topic
3. **Add persistence** - Implement offset tracking per consumer
4. **Build dashboards** - Visualize real-time data
5. **Set up alerts** - Automated escalation for thresholds
6. **Monitor SLAs** - Track delivery guarantees

See [API.md](API.md) and [TESTING.md](TESTING.md) for detailed references.
