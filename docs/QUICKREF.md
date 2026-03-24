# Quick Reference Card

## 🚀 Quick Start (30 seconds)

```bash
# Build broker
cd kafka-system/build && make -j$(nproc) && ./broker

# Terminal 2: Run producer
cd kafka-system/client && node examples/producer.js

# Terminal 3: Run consumer
cd kafka-system/client && node examples/consumer.js
```

---

## 🔧 Build Commands

```bash
# Fresh build
cd kafka-system
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Just rebuild (already built)
cd kafka-system/build && make -j$(nproc)

# Run tests
cd kafka-system/build && ctest

# Run specific executable
./broker                    # Start broker
./sensor_simulator          # Generate test data
./traffic_monitor          # CLI monitoring
```

---

## 💻 JavaScript Client Usage

### Import
```javascript
const { RabbitClient, QoS } = require('./rabbit-client.js');
```

### Create Client
```javascript
const client = new RabbitClient({
  host: 'localhost',
  port: 1883,
  clientId: 'my-app'
});
```

### Connect
```javascript
client.on('connect', () => {
  console.log('Connected!');
});

client.on('error', (err) => {
  console.error('Error:', err.message);
});
```

### Subscribe
```javascript
// Single topic
client.subscribe('sensor/001/data', QoS.AT_LEAST_ONCE);

// Wildcard (+)
client.subscribe('sensor/+/data', QoS.AT_LEAST_ONCE);

// Multi-level wildcard (#)
client.subscribe('sensor/#', QoS.AT_LEAST_ONCE);
```

### Publish
```javascript
// QoS 0 (fire and forget)
client.publish('sensor/001/data', 'message', QoS.AT_MOST_ONCE);

// QoS 1 (guaranteed delivery)
client.publish('sensor/001/data', 'message', QoS.AT_LEAST_ONCE);

// QoS 2 (exactly once)
client.publish('sensor/001/data', 'message', QoS.EXACTLY_ONCE);

// JSON data
const data = { temp: 25, humidity: 60 };
client.publish('sensor/001/data', JSON.stringify(data));
```

### Receive Messages
```javascript
client.on('message', (msg) => {
  console.log('Topic:', msg.topic);
  console.log('Data:', msg.data.toString());
  console.log('QoS:', msg.qos);
});
```

### Disconnect
```javascript
client.disconnect(() => {
  console.log('Disconnected');
  process.exit(0);
});
```

---

## 📁 File Structure

```
kafka-system/
├── build/
│   ├── broker              ← Start here: ./broker
│   ├── sensor_simulator
│   └── traffic_monitor
├── src/
│   └── broker/
│       ├── storage_manager.cpp    ← Persistence logic
│       ├── broker.cpp              ← Main broker
│       └── session.hpp             ← Per-client state
├── client/
│   ├── rabbit-client.js   ← Main library
│   ├── API.md              ← API reference
│   ├── TESTING.md          ← Testing guide
│   └── examples/
│       ├── consumer.js
│       ├── producer.js
│       └── monitor.js
├── ARCHITECTURE.md         ← System design
├── INTEGRATION.md          ← Deploy & integrate
└── DOCS_INDEX.md          ← Documentation guide
```

---

## 🎯 QoS Levels

| Level | Name | Delivery | Use Case |
|-------|------|----------|----------|
| 0 | AT_MOST_ONCE | Fire & forget | Metrics, logs |
| 1 | AT_LEAST_ONCE | With ack | Important data |
| 2 | EXACTLY_ONCE | Single delivery | Transactions |

```javascript
QoS.AT_MOST_ONCE   // 0
QoS.AT_LEAST_ONCE  // 1
QoS.EXACTLY_ONCE   // 2
```

---

## 📊 Topic Wildcards

| Pattern | Matches | Doesn't Match |
|---------|---------|---------------|
| `sensor/001/data` | Exact | `sensor/002/data` |
| `sensor/+/data` | `sensor/001/data` | `sensor/001/status` |
| `sensor/#` | All under sensor | Topics outside sensor |

---

## 🐛 Debugging

### Check Broker Running
```bash
ps aux | grep broker
netstat -tlnp | grep 1883
```

### View Storage
```bash
ls -la storage/rabbit/
find storage -name "*.log" | wc -l
du -sh storage/
```

### Check Client
```javascript
console.log('Connected:', client.isConnected());
console.log('State:', client.getState());
console.log('Subscriptions:', client.getSubscriptions());
```

### Common Errors

| Error | Fix |
|-------|-----|
| Connection refused | Start broker: `./broker` |
| Address in use | Kill process: `lsof -ti:1883 \| xargs kill -9` |
| No messages | Check topic match, verify wildcards |
| Broker crash | Check storage permissions |

---

## 🎯 Example Patterns

### Producer Loop
```javascript
let counter = 0;
setInterval(() => {
  client.publish('app/counter', String(++counter), QoS.AT_LEAST_ONCE);
}, 1000);
```

### Consumer Pattern
```javascript
client.on('connect', () => {
  client.subscribe('traffic/+/telemetry', QoS.AT_LEAST_ONCE);
});

client.on('message', (msg) => {
  const data = JSON.parse(msg.data.toString());
  console.log(`[${msg.topic}] Speed: ${data.speed} km/h`);
});
```

### Monitoring Pattern
```javascript
const stats = {};

client.on('message', (msg) => {
  const data = JSON.parse(msg.data.toString());
  const sensor = data.sensorId;
  
  if (!stats[sensor]) {
    stats[sensor] = { count: 0, sum: 0 };
  }
  
  stats[sensor].count++;
  stats[sensor].sum += data.speed;
});

setInterval(() => {
  for (const [sensor, stat] of Object.entries(stats)) {
    console.log(`${sensor}: avg=${(stat.sum/stat.count).toFixed(1)}`);
  }
}, 10000);
```

---

## 📈 Performance Quick Tips

1. **Use QoS 0** for non-critical data (fastest)
2. **Batch messages** instead of one-by-one
3. **Use wildcards** to reduce subscription count
4. **Handle events synchronously** (don't await)
5. **Monitor disk space** for storage

---

## 🔗 Documentation Quick Links

| Doc | Purpose |
|-----|---------|
| [README.md](README.md) | Main documentation |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design |
| [INTEGRATION.md](INTEGRATION.md) | Production deployment |
| [client/API.md](client/API.md) | Full API reference |
| [client/TESTING.md](client/TESTING.md) | Testing guide |
| [DOCS_INDEX.md](DOCS_INDEX.md) | Documentation index |

---

## 📞 Command Cheat Sheet

```bash
# Build & Run
make -j$(nproc)               # Build
./broker                       # Start broker
node examples/consumer.js      # Subscribe
node examples/producer.js      # Publish
node examples/monitor.js       # Monitor stats

# Cleanup
rm -rf build storage           # Clean everything
cmake --build build --target clean

# Check Status
ps aux | grep broker           # Broker running?
netstat -tlnp | grep 1883     # Port listening?
ls storage/rabbit/ -la        # Storage files?
du -sh storage/               # Storage size?

# Debug
tail -f broker.log            # Broker logs
find storage -type f | wc -l  # Message count
lsof -i :1883                 # Process on port
```

---

## [+] Checklist: First 5 Minutes

- [ ] Build broker: `cd build && make -j$(nproc)`
- [ ] Start broker: `./broker`
- [ ] Open new terminal: `cd client`
- [ ] Run consumer: `node examples/consumer.js`
- [ ] Open third terminal: `cd client`
- [ ] Run producer: `node examples/producer.js`
- [ ] See messages flow in consumer terminal
- [ ] Stop producer (Ctrl+C)
- [ ] Stop consumer (Ctrl+C)
- [ ] Stop broker (Ctrl+C)

---

## ✨ Next Steps

1. **Run examples** - Get familiar with producer/consumer
2. **Read [client/API.md](client/API.md)** - Understand all available methods
3. **Write your app** - Use examples as template
4. **Check [INTEGRATION.md](INTEGRATION.md)** - When ready to deploy
5. **Study [ARCHITECTURE.md](ARCHITECTURE.md)** - Deep dive into design

---

## 🎓 Learning Resources

- **Quick Start**: [client/README.md](client/README.md)
- **Full API**: [client/API.md](client/API.md)
- **Run Tests**: [client/TESTING.md](client/TESTING.md)
- **Deploy**: [INTEGRATION.md](INTEGRATION.md)
- **Design**: [ARCHITECTURE.md](ARCHITECTURE.md)
- **Index**: [DOCS_INDEX.md](DOCS_INDEX.md)

---

## 🚀 You're Ready!

Everything is working. Now start building!

```bash
cd kafka-system/client
node examples/consumer.js &
node examples/producer.js
```

That's it. Messages are flowing. 🎉
