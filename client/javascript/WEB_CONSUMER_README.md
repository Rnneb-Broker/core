# Highway Broker - Web Consumer

A minimal web-based consumer interface for the Highway Broker system. This allows users to subscribe to topics and view messages in real-time directly from their web browser.

## Features

- ✅ **Real-time Message Display**: See messages as they arrive from the broker
- ✅ **Topic Subscription**: Subscribe to multiple topics with QoS support
- ✅ **Visual Statistics**: Track message count, subscriptions, and unique topics
- ✅ **Clean UI**: Modern, responsive interface with smooth animations
- ✅ **Message History**: Keep track of recent messages (last 50)
- ✅ **JSON Pretty-printing**: Automatically formats JSON messages

## Files

- **`consumer.html`** - The main web consumer page
- **`highway-client-browser.js`** - Browser-compatible WebSocket client
- **`highway-client.js`** - Node.js TCP client (for server-side applications)

## Quick Start

### 1. Setup WebSocket Bridge

Since browsers can't directly connect to TCP sockets, you need a WebSocket-to-TCP bridge. You have several options:

#### Option A: Use a WebSocket Proxy (Recommended)

Install and run `websockify`:

```bash
# Install websockify
pip install websockify

# Run the proxy (maps ws://localhost:8080 to tcp://localhost:1883)
websockify 8080 localhost:1883
```

#### Option B: Simple Node.js WebSocket Bridge

Create a simple bridge using the provided script below or use a tool like `ws` npm package.

### 2. Start the Broker

Make sure your Highway Broker is running:

```bash
# From the kafka-system directory
./build/broker
```

### 3. Open the Web Consumer

Simply open `consumer.html` in your web browser:

```bash
# You can use a simple HTTP server
python3 -m http.server 8000

# Or with Node.js
npx http-server
```

Then navigate to: `http://localhost:8000/consumer.html`

### 4. Connect and Subscribe

1. **Connect**: Click the "Connect" button (default: `ws://localhost:8080`)
2. **Subscribe**: Enter a topic pattern (e.g., `highway/+/telemetry`) and click "Subscribe"
3. **Watch Messages**: Messages will appear in real-time as they're published

## Usage Examples

### Common Topic Patterns

```
highway/+/telemetry        # All telemetry from any sensor
highway/+/alerts           # All alerts from any sensor
highway/sensor1/telemetry  # Specific sensor telemetry
highway/#                  # All highway topics
```

### Connection Settings

- **WebSocket URL**: The WebSocket proxy address (default: `ws://localhost:8080`)
- **Client ID**: Unique identifier for this client (auto-generated)
- **Keep Alive**: Heartbeat interval in seconds (default: 60)

### QoS Levels

- **0 - At Most Once**: Fire and forget (fastest)
- **1 - At Least Once**: Guaranteed delivery (recommended)
- **2 - Exactly Once**: Guaranteed single delivery (most reliable)

## Creating a WebSocket Bridge

If you need to create a custom WebSocket-to-TCP bridge, here's a simple Node.js example:

```javascript
const WebSocket = require('ws');
const net = require('net');

const WS_PORT = 8080;
const BROKER_HOST = 'localhost';
const BROKER_PORT = 1883;

const wss = new WebSocket.Server({ port: WS_PORT });

console.log(`WebSocket bridge running on ws://localhost:${WS_PORT}`);
console.log(`Forwarding to ${BROKER_HOST}:${BROKER_PORT}`);

wss.on('connection', (ws) => {
  console.log('Browser client connected');
  
  // Create TCP connection to broker
  const tcpClient = net.createConnection({
    host: BROKER_HOST,
    port: BROKER_PORT
  }, () => {
    console.log('Connected to broker');
  });

  // Forward WebSocket -> TCP
  ws.on('message', (data) => {
    tcpClient.write(Buffer.from(data));
  });

  // Forward TCP -> WebSocket
  tcpClient.on('data', (data) => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(data);
    }
  });

  // Handle disconnections
  ws.on('close', () => {
    console.log('Browser client disconnected');
    tcpClient.destroy();
  });

  tcpClient.on('close', () => {
    console.log('Broker connection closed');
    ws.close();
  });

  tcpClient.on('error', (err) => {
    console.error('TCP error:', err);
    ws.close();
  });
});
```

Save this as `ws-bridge.js` and run:

```bash
npm install ws
node ws-bridge.js
```

## Architecture

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│   Browser   │ WebSocket│  WS Bridge   │   TCP   │   Highway   │
│  (consumer. │◄────────►│  (port 8080) │◄────────►│   Broker    │
│    html)    │          │              │         │ (port 1883) │
└─────────────┘         └──────────────┘         └─────────────┘
```

## Browser Compatibility

The web consumer works in all modern browsers that support:
- WebSocket API
- ES6 JavaScript
- Uint8Array and ArrayBuffer

Tested on:
- ✅ Chrome/Edge 90+
- ✅ Firefox 88+
- ✅ Safari 14+

## Troubleshooting

### Connection Failed

- **Check WebSocket Bridge**: Make sure the bridge is running on the correct port
- **Check Broker**: Ensure the Highway Broker is running and accepting connections
- **Firewall**: Verify firewall rules allow WebSocket connections

### No Messages Appearing

- **Check Subscription**: Make sure you're subscribed to the correct topic
- **Check Producer**: Verify messages are being published to the broker
- **Console Logs**: Open browser DevTools (F12) and check for errors

### WebSocket Connection Refused

```
Error: WebSocket connection to 'ws://localhost:8080' failed
```

**Solution**: Start the WebSocket bridge/proxy first

### CORS Issues

If you're getting CORS errors, make sure you're:
- Serving the HTML file through HTTP (not `file://`)
- Using the correct WebSocket URL

## Production Deployment

For production environments:

1. **Use WSS (Secure WebSocket)**: Configure SSL/TLS certificates
2. **Authentication**: Add authentication to the WebSocket bridge
3. **Rate Limiting**: Implement rate limiting on the bridge
4. **Monitoring**: Add logging and monitoring for connections
5. **Load Balancing**: Use multiple bridge instances behind a load balancer

## API Reference

### HighwayBrowserClient

```javascript
// Create client
const client = new HighwayBrowserClient({
  url: 'ws://localhost:8080',
  clientId: 'my-client',
  keepalive: 60
});

// Connect
client.connect((success, err) => {
  if (success) console.log('Connected!');
});

// Subscribe
client.subscribe('my/topic', QoS.AT_LEAST_ONCE);

// Handle messages
client.on('message', (msg) => {
  console.log('Topic:', msg.topic);
  console.log('Data:', msg.data);
});

// Publish (optional)
client.publish('my/topic', 'Hello World', QoS.AT_LEAST_ONCE);

// Disconnect
client.disconnect();
```

## License

MIT

## Support

For issues or questions:
- Check the main project documentation
- Review browser console for error messages
- Verify WebSocket bridge is configured correctly
