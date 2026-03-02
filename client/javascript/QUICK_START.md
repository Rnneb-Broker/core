# Quick Reference - Web Consumer

## One-Command Start

```bash
./start-web-consumer.sh
```

This script will:
1. ✅ Check dependencies
2. ✅ Install required packages
3. ✅ Start WebSocket bridge
4. ✅ Start HTTP server
5. ✅ Open browser automatically

Then visit: **http://localhost:8000/consumer.html**

---

## Manual Start (3 terminals)

### Terminal 1: Broker
```bash
cd /home/ghost/dev/projects/ilisi-projecrs/kafka-system
./build/broker
```

### Terminal 2: WebSocket Bridge
```bash
cd client/javascript
npm install ws
node ws-bridge.js
```

### Terminal 3: Web Server
```bash
cd client/javascript
python3 -m http.server 8000
# or: npx http-server -p 8000
```

**Open:** http://localhost:8000/consumer.html

---

## Test with Producer

While web consumer is running, test it:

```bash
# Terminal 4: Send test messages
cd client/javascript
node examples/producer.js
```

You should see messages appear in the web interface!

---

## Common Topics

| Topic Pattern | Description |
|--------------|-------------|
| `highway/+/telemetry` | All sensor telemetry |
| `highway/+/alerts` | All sensor alerts |
| `highway/sensor1/telemetry` | Specific sensor |
| `highway/#` | Everything |

---

## Files Overview

- **consumer.html** - Web interface
- **highway-client-browser.js** - Browser client library
- **highway-client.js** - Node.js client library
- **ws-bridge.js** - WebSocket-to-TCP bridge
- **start-web-consumer.sh** - Quick start script
- **examples/** - Example scripts
  - `consumer.js` - Node.js consumer
  - `producer.js` - Node.js producer
  - `monitor.js` - Traffic monitor

---

## Architecture

```
┌──────────────┐
│   Browser    │  Open: http://localhost:8000/consumer.html
│ (consumer.   │
│   html)      │
└──────┬───────┘
       │ WebSocket
       │ ws://localhost:8080
       ▼
┌──────────────┐
│  WS Bridge   │  node ws-bridge.js
│ (port 8080)  │
└──────┬───────┘
       │ TCP
       │ localhost:1883
       ▼
┌──────────────┐
│   Highway    │  ./build/broker
│   Broker     │
│ (port 1883)  │
└──────────────┘
       ▲
       │ TCP
       │
┌──────┴───────┐
│  Producer    │  node examples/producer.js
│ (publisher)  │
└──────────────┘
```

---

## Troubleshooting

### Port Already in Use

```bash
# Find what's using the port
lsof -i :8080  # WebSocket bridge
lsof -i :8000  # HTTP server
lsof -i :1883  # Broker

# Kill the process
kill -9 <PID>
```

### WebSocket Connection Failed

1. ✅ Check broker is running: `nc -zv localhost 1883`
2. ✅ Check bridge is running: `ps aux | grep ws-bridge`
3. ✅ Check browser console (F12) for errors

### No Messages Appearing

1. ✅ Verify subscription topic is correct
2. ✅ Check if producer is sending messages
3. ✅ Look at bridge logs for activity
4. ✅ Check browser console for errors

### Permission Denied

```bash
chmod +x start-web-consumer.sh
```

---

## Environment Variables

Customize the WebSocket bridge:

```bash
# Change ports
WS_PORT=9090 BROKER_PORT=1883 node ws-bridge.js

# Connect to remote broker
BROKER_HOST=remote.server.com node ws-bridge.js
```

---

## Production Notes

For production deployment:

1. **Use WSS**: Configure SSL/TLS for secure WebSocket
2. **Authentication**: Add auth middleware to bridge
3. **CORS**: Configure proper CORS headers
4. **Rate Limiting**: Implement rate limits
5. **Monitoring**: Add logging and metrics
6. **Load Balancing**: Use nginx or similar

---

## Support

- 📖 Full docs: [WEB_CONSUMER_README.md](WEB_CONSUMER_README.md)
- 📖 API docs: [README.md](README.md)
- 🐛 Issues: Check browser console (F12)
- 💡 Examples: See `examples/` directory

---

**Made with ❤️ for Highway Broker**
