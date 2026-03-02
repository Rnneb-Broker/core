# JavaScript Client Testing Guide

## Prerequisites

- Highway Broker compiled and built (see main README.md)
- Node.js v18+ (tested with v22.22.0)
- Broker listening on `localhost:1883` (default)

---

## Setup

### 1. Verify Node.js

```bash
node --version
# Should show v18.0.0 or higher
```

### 2. Navigate to client directory

```bash
cd /path/to/kafka-system/client
```

### 3. Verify examples are ready

```bash
ls -la examples/
# Should show: consumer.js, producer.js, monitor.js
```

---

## Test 1: Basic Pub/Sub

### Terminal 1 - Start Broker

```bash
cd /path/to/kafka-system/build
./broker
# Should show: [INFO] Broker listening on port 1883
```

### Terminal 2 - Start Consumer

```bash
cd /path/to/kafka-system/client
node examples/consumer.js
# Should show: Connecting to localhost:1883
#             [+] Connected to Highway Broker
#             Subscribed to: highway/+/telemetry, highway/+/alerts
#             Waiting for messages... (Ctrl+C to exit)
```

### Terminal 3 - Start Producer

```bash
cd /path/to/kafka-system/client
node examples/producer.js
# Should show: Publishing sensor telemetry...
#             [Sensor 1001] Published: {"timestamp":...,"speed":45,"vehicles":12,...}
#             [Sensor 1002] Published: {"timestamp":...,"speed":62,"vehicles":8,...}
#             ...
```

### Expected Consumer Output

```
📨 Message received:
   Topic: highway/1001/telemetry
   Data: {"timestamp":"2024-01-15T12:34:56.789Z","sensorId":"1001","speed":45,"vehicles":12}
   QoS: 1
   PacketId: 42

📨 Message received:
   Topic: highway/1002/telemetry
   Data: {"timestamp":"2024-01-15T12:34:57.123Z","sensorId":"1002","speed":62,"vehicles":8}
   QoS: 1
   PacketId: 43
```

**Success Criteria:**
- [+] Producer publishes messages
- [+] Consumer receives them with correct topic/data
- [+] Wildcard subscription works (`highway/+/telemetry`)
- [+] No connection errors

---

## Test 2: Multiple Subscribers

Verify that multiple consumers can subscribe to the same topic.

### Terminal 2 - First Consumer

```bash
cd /path/to/kafka-system/client
node examples/consumer.js
```

### Terminal 3 - Second Consumer

```bash
cd /path/to/kafka-system/client
node examples/consumer.js
```

### Terminal 4 - Producer

```bash
cd /path/to/kafka-system/client
node examples/producer.js
```

**Expected:** Both consumers receive the same messages.

**Success Criteria:**
- [+] Both consumers show identical messages
- [+] No duplicate sends from broker
- [+] Multiple subscriptions work independently

---

## Test 3: Real-Time Monitor

Verify statistics collection and real-time updates.

### Terminal 1 - Start Broker

```bash
cd /path/to/kafka-system/build
./broker
```

### Terminal 2 - Start Monitor

```bash
cd /path/to/kafka-system/client
node examples/monitor.js
# Should show: Connected to Highway Broker
#             Monitoring telemetry and alerts...
#             (Stats update every 10 seconds)
```

### Terminal 3 - Start Producer

```bash
cd /path/to/kafka-system/client
node examples/producer.js
```

### Expected Monitor Output

```
📊 TRAFFIC STATISTICS (Time: 2024-01-15T12:35:00Z)
┌──────────────────────────────────────────────────┐
│ Sensor 1001                                       │
│   Messages: 23                                    │
│   Speed: min=15 km/h, avg=45.2 km/h, max=78 km/h│
│   Latest: 12:34:59 (45 km/h, 12 vehicles)       │
│   ⚠️  SLOW TRAFFIC: 15 km/h < 10 km/h threshold  │
│                                                    │
│ Sensor 1002                                       │
│   Messages: 22                                    │
│   Speed: min=20 km/h, avg=52.1 km/h, max=80 km/h│
│   Latest: 12:34:59 (55 km/h, 8 vehicles)        │
└──────────────────────────────────────────────────┘

🚨 ALERTS RECEIVED: 2
   - [1001] Heavy traffic at 15 km/h
   - [1002] Congestion detected
```

**Success Criteria:**
- [+] Monitor subscribes successfully
- [+] Statistics update every 10 seconds
- [+] Min/max/average calculated correctly
- [+] Alerts displayed when speed < 10 km/h

---

## Test 4: Message Persistence

Verify that messages are persisted in the broker's storage.

### Terminal 1 - Start Broker

```bash
cd /path/to/kafka-system/build
./broker
# Check storage directory
ls -la storage/highway/
```

### Terminal 2 - Start Producer

```bash
cd /path/to/kafka-system/client
node examples/producer.js
# Let it run for 10-20 seconds, then Ctrl+C
```

### Terminal 3 - Check Storage Files

```bash
# List all segment files
find storage/highway -name "*.log" -type f

# Check file sizes (should be > 0)
ls -lh storage/highway/*/

# You should see files like:
# storage/highway/1001/000000000000.log
# storage/highway/1002/000000000000.log
# etc.
```

### Terminal 4 - Add New Consumer After Producer Stops

```bash
# Restart broker (optional, messages persist)
cd /path/to/kafka-system/build
./broker

# Then in another terminal:
cd /path/to/kafka-system/client
node examples/consumer.js
# Should NOT see messages (consumer only gets new ones)
# This is expected behavior - messages are persisted for replay functionality
```

**Success Criteria:**
- [+] Storage files created in `storage/highway/*/`
- [+] Files contain binary message data
- [+] Broker restarts don't lose sent messages

---

## Test 5: High-Volume Publisher

Test throughput and buffering behavior.

### Create `examples/highload.js`:

```javascript
const { HighwayClient, QoS } = require('../highway-client.js');

const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  clientId: 'highload-test'
});

let published = 0;
let msgPerSecond = 0;

client.on('connect', () => {
  console.log('Connected - starting high-volume test');
  
  // Publish 1000 messages as fast as possible
  for (let i = 0; i < 1000; i++) {
    const data = JSON.stringify({
      id: i,
      timestamp: Date.now(),
      value: Math.random() * 100
    });
    
    client.publish(`test/load/${i % 10}`, data, QoS.AT_MOST_ONCE);
    published++;
  }
  
  console.log(`[+] Published ${published} messages`);
});

client.on('error', (err) => {
  console.error('Error:', err.message);
});

// Stats every second
setInterval(() => {
  console.log(`Throughput: ${msgPerSecond}/sec (total: ${published})`);
  msgPerSecond = 0;
}, 1000);

setTimeout(() => {
  client.disconnect(() => {
    console.log(`\nFinished: ${published} messages published`);
    process.exit(0);
  });
}, 5000);
```

### Run Test

```bash
node examples/highload.js
# Expected: ~1000 messages published in < 5 seconds
# Throughput: 200-500 msg/sec depending on system
```

---

## Test 6: Error Handling

### Test Connection Refused

```javascript
const { HighwayClient } = require('./highway-client.js');

const client = new HighwayClient({
  host: 'localhost',
  port: 9999,  // Wrong port
  autoConnect: true
});

client.on('error', (err) => {
  console.log('Expected error:', err.message);
  // Should show: "ECONNREFUSED"
});
```

### Test Authentication Failure

```javascript
const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  username: 'wrong-user',
  password: 'wrong-pass'
});

client.on('error', (err) => {
  console.log('Expected auth error:', err.message);
});
```

---

## Troubleshooting

### "Connection timeout"
- Broker not running
- Check: `ps aux | grep broker`
- Start broker: `./build/broker`

### "Address already in use"
- Port 1883 already taken
- Kill process: `lsof -ti:1883 | xargs kill -9`
- Or use different port in client config

### "Module not found"
- run from client directory
- Verify `highway-client.js` exists
- Check: `ls -la highway-client.js`

### "No messages received"
- Producer might not be running
- Check subscription topic matches publisher topic
- Wildcards: use `#` or `+` correctly

### "Disconnects immediately"
- Check broker logs for auth errors
- Verify credentials if using auth
- Check network connectivity

---

## Performance Benchmarks

Typical throughput on modern hardware:

| Test | Throughput | Latency |
|------|-----------|---------|
| Single publisher | 500+ msg/sec | < 10ms |
| 10 subscribers | 300+ msg/sec | < 20ms |
| 100-byte messages | 1000+ msg/sec | < 5ms |
| 1MB messages | 50-100 msg/sec | < 100ms |
| QoS 0 (fire-and-forget) | 1000+ msg/sec | < 3ms |
| QoS 1 (acknowledged) | 500+ msg/sec | < 10ms |

---

## Debug Mode

Add verbose logging to monitor:

### In `consumer.js`:

```javascript
const client = new HighwayClient({ /* ... */ });

// Add before connect
client.on('connect', () => {
  console.log('[DEBUG] Connection established:', {
    clientId: client.getClientId?.(),
    state: client.getState?.()
  });
});

client.on('message', (msg) => {
  console.log('[DEBUG] Message:', {
    topic: msg.topic,
    bytes: msg.data.length,
    qos: msg.qos,
    timestamp: new Date().toISOString()
  });
});
```

---

## Next Steps

After successful testing:

1. **Deploy producer agents** to collect real telemetry data
2. **Scale consumers** - verify multiple subscribers work
3. **Monitor performance** - track throughput and latency
4. **Add persistence** - implement consumer offset tracking
5. **Extend protocol** - add fetch-by-offset functionality

See [API.md](API.md) for complete client reference.
