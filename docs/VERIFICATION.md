# System Verification Checklist

## [+] Pre-Flight Checks (Before Running)

### Environment
- [ ] Linux OS (tested on Ubuntu 22.04)
- [ ] C++17 compiler available: `g++ --version`
- [ ] CMake 3.22+: `cmake --version`
- [ ] Node.js v18+: `node --version`
- [ ] 2GB free disk space: `df -h`
- [ ] 1GB free RAM: `free -h`

### Directory Structure
- [ ] Workspace path: `/home/ghost/dev/projects/ilisi-projecrs/kafka-system`
- [ ] Build directory exists: `ls -d build/`
- [ ] Source files present: `ls src/broker/*.cpp`
- [ ] Client files present: `ls client/highway-client.js`
- [ ] Examples present: `ls client/examples/*.js`

### Dependencies
- [ ] Boost headers available: `pkg-config --cflags boost`
- [ ] CMakePresets.json exists: `test -f CMakePresets.json`
- [ ] Conanfile.txt exists: `test -f conanfile.txt`

---

## 🔨 Build Verification

### Run Build
```bash
cd /home/ghost/dev/projects/ilisi-projecrs/kafka-system/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

**Checklist:**
- [ ] CMake configures without errors
- [ ] Build completes without errors
- [ ] No compiler warnings (check build log)
- [ ] All targets built successfully

### Binaries Created
- [ ] Broker executable: `ls -lh build/broker`
- [ ] Test executables: `ls -lh build/*test*`
- [ ] Size reasonable: `du -sh build/`

### Compile Commands
- [ ] compile_commands.json generated: `test -f build/compile_commands.json`
- [ ] Can be used for IDE integration

---

## 🚀 Broker Startup Verification

### Start Broker
```bash
cd /home/ghost/dev/projects/ilisi-projecrs/kafka-system/build
./broker
```

**Expected Output:**
```
[INFO] Broker listening on port 1883
[INFO] Storage directory initialized
[DEBUG] FlushWorker started
```

**Checklist:**
- [ ] Broker starts without crashes
- [ ] Port 1883 listening: `netstat -tlnp | grep 1883`
- [ ] No permission errors
- [ ] No segmentation faults
- [ ] Process visible: `ps aux | grep broker`

### Storage Initialization
- [ ] Storage directory created: `ls -d storage/`
- [ ] Directory is writable: `touch storage/test && rm storage/test`
- [ ] No disk space errors
- [ ] Permissions correct: `stat storage/`

### Keep Broker Running
- [ ] Leave broker running in Terminal 1
- [ ] Ready for client connections

---

## 🎯 JavaScript Client Verification

### Environment Check
```bash
node --version              # Should be v18+
node -e "console.log(process.platform)"  # Should be 'linux'
```

**Checklist:**
- [ ] Node.js version v18+
- [ ] Platform is Linux
- [ ] npm available (optional)

### Client Library Syntax
```bash
cd /home/ghost/dev/projects/ilisi-projecrs/kafka-system/client
node -c highway-client.js   # Syntax check
```

**Checklist:**
- [ ] No syntax errors
- [ ] File is readable
- [ ] Size ~15KB: `ls -lh highway-client.js`

### Client Files Present
- [ ] Main library: `test -f highway-client.js`
- [ ] Examples: `test -f examples/consumer.js && test -f examples/producer.js && test -f examples/monitor.js`
- [ ] Documentation: `test -f README.md && test -f API.md`

---

## 💬 Producer Test

### Terminal 2: Start Producer
```bash
cd /home/ghost/dev/projects/ilisi-projecrs/kafka-system/client
node examples/producer.js
```

**Expected Output:**
```
Connecting to localhost:1883...
[+] Connected to Highway Broker
Publishing sensor telemetry...
[Sensor 1001] Published: {"timestamp":"...","speed":45,...}
[Sensor 1002] Published: {"timestamp":"...","speed":62,...}
```

**Checklist:**
- [ ] Producer connects to broker
- [ ] Messages published successfully
- [ ] Timestamps are current
- [ ] Speed values between 10-80 km/h
- [ ] Vehicle counts between 5-20
- [ ] Runs for ~60 seconds
- [ ] Clean exit (no errors/hangs)

---

## 👂 Consumer Test

### Terminal 3: Start Consumer
```bash
cd /home/ghost/dev/projects/ilisi-projecrs/kafka-system/client
node examples/consumer.js
```

**Expected Output:**
```
Connecting to localhost:1883...
[+] Connected to Highway Broker
Subscribed to: highway/+/telemetry, highway/+/alerts
Waiting for messages... (Ctrl+C to exit)

📨 Message received:
   Topic: highway/1001/telemetry
   Data: {"timestamp":"...","sensorId":"1001",...}
   QoS: 1
   PacketId: 42
```

**Checklist:**
- [ ] Consumer connects to broker
- [ ] Consumer subscribes successfully
- [ ] Messages received from producer
- [ ] Topic names correct
- [ ] Data is valid JSON
- [ ] QoS levels correct (0 or 1)
- [ ] PacketIds increment
- [ ] Clean exit on Ctrl+C

### Message Flow Verification
- [ ] Producer publishes message
- [ ] Consumer receives within 100ms
- [ ] Data matches published format
- [ ] No message loss

---

## 📊 Monitor Test

### Terminal 3 (Alternative): Start Monitor
```bash
cd /home/ghost/dev/projects/ilisi-projecrs/kafka-system/client
node examples/monitor.js
```

**Expected Output:**
```
Connected to Highway Broker
Monitoring telemetry and alerts...

📊 TRAFFIC STATISTICS (Time: 2024-01-15T12:35:00Z)
┌──────────────────────────────────────────────────┐
│ Sensor 1001                                       │
│   Messages: 23                                    │
│   Speed: min=15 km/h, avg=45.2 km/h, max=78 km/h│
│   Latest: 12:34:59 (45 km/h, 12 vehicles)       │
└──────────────────────────────────────────────────┘
```

**Checklist:**
- [ ] Monitor connects successfully
- [ ] Aggregates statistics correctly
- [ ] Updates every 10 seconds
- [ ] Min/max/average calculated accurately
- [ ] Displays sensor IDs correctly
- [ ] Shows latest message timestamp
- [ ] No calculation errors

---

## 💾 Storage Verification

### While Producer Running
```bash
ls -la storage/highway/
find storage -name "*.log" -type f
du -sh storage/
```

**Checklist:**
- [ ] Directory `storage/highway/` exists
- [ ] Subdirectories for each topic created
- [ ] `.log` files created (at least 1)
- [ ] Files are non-empty (> 0 bytes)
- [ ] Permissions correct (readable)
- [ ] Growing in real-time

### File Format Validation
```bash
hexdump -C storage/highway/1001/000000000000.log | head -20
```

**Checklist:**
- [ ] Files are binary (not text)
- [ ] Header structure valid
- [ ] Size matches expected (multiple messages)

### Storage Growth
```bash
du -sh storage/highway/
# Run for 1 minute, then repeat
du -sh storage/highway/
```

**Checklist:**
- [ ] Storage size increases over time
- [ ] No gigantic jump (normal growth)
- [ ] Roughly: (n messages × avg size) bytes

---

## 🔄 Persistence Verification

### Producer Stop & Restart Broker

1. Stop producer (Ctrl+C in Terminal 2)
2. Stop broker (Ctrl+C in Terminal 1)
3. Check storage files persist:
   ```bash
   ls -la storage/highway/*/
   ```
4. Restart broker:
   ```bash
   cd build && ./broker
   ```

**Checklist:**
- [ ] Storage files still present after broker restart
- [ ] File sizes unchanged
- [ ] Broker recovers successfully
- [ ] No data loss warnings

### New Consumer After Data Persistence

1. Keep broker running
2. Start new consumer:
   ```bash
   node examples/consumer.js
   ```

**Checklist:**
- [ ] New consumer connects
- [ ] Does NOT receive old messages (expected behavior)
- [ ] Only receives new messages from now on

---

## 🔗 Network Verification

### Connection Port
```bash
netstat -tlnp | grep 1883
# or
lsof -i :1883
```

**Checklist:**
- [ ] Broker listening on 0.0.0.0:1883
- [ ] TCP protocol
- [ ] Status: LISTEN

### Client Connections
```bash
netstat -tnp | grep 1883 | grep -v LISTEN
```

**Checklist:**
- [ ] Show active connections
- [ ] Count matches running clients
- [ ] Status: ESTABLISHED

### Firewall/Network
```bash
ping localhost
telnet localhost 1883
```

**Checklist:**
- [ ] Localhost connectivity works
- [ ] Port 1883 reachable
- [ ] No connection refused

---

## ⚠️ Error Handling Verification

### Broker Crash Recovery
```bash
# In broker Terminal, Ctrl+C (kill broker)
# Immediately restart
./broker
```

**Checklist:**
- [ ] Broker restarts cleanly
- [ ] Reconnection auto-occurs in clients
- [ ] No data corruption detected
- [ ] Messages resume flowing

### Consumer Disconnection
```bash
# In consumer Terminal, Ctrl+C
# Restart consumer
node examples/consumer.js
```

**Checklist:**
- [ ] Consumer reconnects automatically
- [ ] Previous messages not re-received
- [ ] New messages arrive normally

### Network Interrupt (if available)
```bash
# Simulate: `sudo tc qdisc add dev lo root netem delay 1000ms`
# Then connect client - should timeout
# Cleanup: `sudo tc qdisc del dev lo root`
```

**Checklist:**
- [ ] Client handles timeout gracefully
- [ ] Error events fired
- [ ] Reconnection attempted
- [ ] No crash

---

## 📈 Performance Verification

### Throughput Test
```bash
time node examples/producer.js | grep "Published" | wc -l
# Should show 30+ published messages in ~60 seconds
```

**Checklist:**
- [ ] Sustained publication rate
- [ ] No timeouts
- [ ] No dropped messages

### Consumer Latency
Measure time from publish to receive:
- Expected: < 100ms
- Document actual value

**Checklist:**
- [ ] Latency < 100ms
- [ ] Consistent (no variance)

### Storage I/O
```bash
# While producer running
watch -n 1 'du -sh storage/'
```

**Checklist:**
- [ ] Storage grows smoothly
- [ ] No sudden jumps (indicates flush)
- [ ] Doesn't fill disk

---

## 📚 Documentation Verification

### All Docs Present
- [ ] [README.md](README.md) - Main docs
- [ ] [ARCHITECTURE.md](ARCHITECTURE.md) - Design
- [ ] [INTEGRATION.md](INTEGRATION.md) - Deployment
- [ ] [COMPLETE_SUMMARY.md](COMPLETE_SUMMARY.md) - Summary
- [ ] [DOCS_INDEX.md](DOCS_INDEX.md) - Navigation
- [ ] [QUICKREF.md](QUICKREF.md) - Quick reference
- [ ] [client/API.md](client/API.md) - API reference
- [ ] [client/README.md](client/README.md) - Client quick start
- [ ] [client/TESTING.md](client/TESTING.md) - Testing guide

### Doc Quality Check
```bash
grep -l "TODO\|FIXME\|XXX" *.md client/*.md 2>/dev/null || echo "No TODOs"
```

**Checklist:**
- [ ] No TODO markers in published docs
- [ ] Links work: `grep "](.*\.md" *.md | wc -l` → 30+
- [ ] Code examples present in API.md

---

## [+] Final Verification

### System Readiness
- [ ] Build compiles without warnings
- [ ] Broker starts instantly
- [ ] Client library loads without errors
- [ ] Producer publishes continuously
- [ ] Consumer receives all messages
- [ ] Messages persist to disk
- [ ] Broker survives restart
- [ ] Storage doesn't fill disk
- [ ] Network connections stable
- [ ] Documentation complete

### Sign-Off
**All tests passed?**
- [ ] YES → System is production-ready [+]
- [ ] NO → Review failed checklist items

---

## 🚀 Deployment Ready Checklist

For production deployment, also verify:

- [ ] Storage on separate partition (not /)
- [ ] Backup strategy defined
- [ ] Monitoring scripts in place
- [ ] Log rotation configured
- [ ] Process manager configured (systemd/supervisor)
- [ ] Memory limits set
- [ ] Disk space alerts active
- [ ] Network bandwidth verified
- [ ] TLS/SSL needed? (not implemented yet)
- [ ] Authentication needed? (not implemented yet)

---

## 📞 Troubleshooting Guide

| Issue | Check | Solution |
|-------|-------|----------|
| Broker won't start | Port 1883 in use | `lsof -ti:1883 \| xargs kill -9` |
| Client timeout | Broker running | `ps aux \| grep broker` |
| No messages received | Topic match | Check wildcard syntax |
| Storage not growing | File permissions | `chmod 755 storage` |
| High CPU usage | Busy loop? | Check broker with `htop` |
| Disk space full | Storage policy | Implement retention |
| Crash on shutdown | Cleanup order | Check signal handlers |

---

## 🎉 Success!

If all checkboxes are [+], you have successfully:

[+] Built the entire system
[+] Verified all components work
[+] Tested pub/sub functionality
[+] Confirmed persistence
[+] Validated documentation
[+] Ready for production

**Next step:** Read [INTEGRATION.md](INTEGRATION.md) for production deployment.

---

## 📋 Quick Summary

```
Build:      [+] Compiles cleanly
Broker:     [+] Listens on 1883
Producer:   [+] Publishes messages
Consumer:   [+] Receives messages
Storage:    [+] Persists data
Recovery:   [+] Restarts cleanly
Docs:       [+] Complete & accurate
Performance:[+] No bottlenecks
Ready:      [+] PRODUCTION-READY
```

---

You've successfully built and verified Highway Broker! 🚀
