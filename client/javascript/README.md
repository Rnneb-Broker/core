# Highway Broker JavaScript Client

A lightweight JavaScript client for the Highway message broker, implementing the MQTT-lite protocol.

## Features

[+] **Publish/Subscribe messaging** - Full pub/sub support  
[+] **Quality of Service (QoS)** - Support for QoS 0, 1, 2  
[+] **Topic wildcards** - Subscribe to multiple topics with patterns  
[+] **Connection management** - Automatic reconnection and keepalive  
[+] **Error handling** - Comprehensive error callbacks  
[+] **Event emitters** - Node.js EventEmitter interface
[+] **Web Consumer** - Browser-based consumer with real-time UI

## Installation

No external dependencies required! Uses only Node.js built-in `net` module.

```bash
cd /path/to/kafka-system/client
npm install
```

## 🌐 Web Consumer (NEW!)

A ready-to-use web interface for consuming messages! Perfect for testing and monitoring.

**Quick Start:**

```bash
# Terminal 1: Start the broker
./build/broker

# Terminal 2: Start the WebSocket bridge
cd client/javascript
npm install ws
npm run bridge

# Terminal 3: Start web server
npm run web
```

Then open: **http://localhost:8000/consumer.html**

See [WEB_CONSUMER_README.md](WEB_CONSUMER_README.md) for full documentation.

## Quick Start

### 1. Consumer (Receive Messages)

```javascript
const { HighwayClient, QoS } = require('./highway-client.js');

const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  clientId: 'my-consumer'
});

client.on('connect', () => {
  console.log('Connected!');
  
  // Subscribe to topic
  client.subscribe('highway/sensor1/telemetry', QoS.AT_LEAST_ONCE);
});

client.on('message', (msg) => {
  console.log(`Received: ${msg.data.toString('utf8')}`);
});
```

### 2. Producer (Send Messages)

```javascript
const { HighwayClient, QoS } = require('./highway-client.js');

const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  clientId: 'my-producer'
});

client.on('connect', () => {
  // Publish message
  client.publish('highway/sensor1/telemetry', 'speed=50km/h', QoS.AT_LEAST_ONCE);
});
```

## API Reference

### Constructor

```javascript
new HighwayClient(config)
```

**Config options:**
- `host` (string): Broker host (default: `localhost`)
- `port` (number): Broker port (default: `1883`)
- `clientId` (string): Unique client identifier
- `username` (string): Authentication username (optional)
- `password` (string): Authentication password (optional)
- `keepalive` (number): Keepalive interval in seconds (default: `60`)
- `autoConnect` (boolean): Auto-connect on creation (default: `true`)

### Methods

#### connect(callback)

Connect to broker.

```javascript
client.connect((success, err) => {
  if (success) {
    console.log('Connected!');
  } else {
    console.error('Failed:', err.message);
  }
});
```

#### subscribe(topic, qos, callback)

Subscribe to topic.

```javascript
client.subscribe('highway/+/telemetry', QoS.AT_LEAST_ONCE, (result) => {
  console.log('Subscribed!');
});
```

**QoS levels:**
- `QoS.AT_MOST_ONCE` (0) - Fire and forget
- `QoS.AT_LEAST_ONCE` (1) - Acknowledged delivery
- `QoS.EXACTLY_ONCE` (2) - Guaranteed single delivery

#### unsubscribe(topic, callback)

Unsubscribe from topic.

```javascript
client.unsubscribe('highway/sensor1/telemetry', (result) => {
  console.log('Unsubscribed!');
});
```

#### publish(topic, data, qos, callback)

Publish message.

```javascript
client.publish(
  'highway/sensor1/telemetry',
  'speed=50km/h',
  QoS.AT_LEAST_ONCE,
  (success) => {
    console.log('Published!');
  }
);
```

Data can be:
- String: `'speed=50km/h'`
- Buffer: `Buffer.from('speed=50km/h')`
- Object: `{ speed: 50 }` (converts to JSON)

#### disconnect(callback)

Disconnect from broker.

```javascript
client.disconnect(() => {
  console.log('Disconnected!');
  process.exit(0);
});
```

#### isConnected()

Check if connected.

```javascript
if (client.isConnected()) {
  console.log('Online');
}
```

#### getState()

Get current state.

```javascript
console.log(client.getState());
// Returns: 'DISCONNECTED', 'CONNECTING', 'CONNECTED', 'AUTHENTICATED'
```

#### getSubscriptions()

Get list of subscribed topics.

```javascript
const topics = client.getSubscriptions();
console.log(topics); // ['highway/+/telemetry', 'highway/+/alerts']
```

### Events

#### connect

Emitted when successfully connected and authenticated.

```javascript
client.on('connect', () => {
  console.log('Ready to publish/subscribe');
});
```

#### message

Emitted when message received.

```javascript
client.on('message', (msg) => {
  console.log(msg.topic);        // Topic name
  console.log(msg.data);         // Message data (Buffer)
  console.log(msg.qos);          // QoS level
  console.log(msg.packetId);     // Packet ID
});
```

#### error

Emitted on error.

```javascript
client.on('error', (err) => {
  console.error('Error:', err.message);
});
```

#### close

Emitted when disconnected.

```javascript
client.on('close', () => {
  console.log('Disconnected');
});
```

#### suback

Emitted when subscription acknowledged.

```javascript
client.on('suback', (result) => {
  console.log(result.packetId);
  console.log(result.grantedQoSList);
});
```

#### puback

Emitted when publish acknowledged (QoS 1+).

```javascript
client.on('puback', (result) => {
  console.log(result.packetId);
});
```

## Examples

### Run Consumer
```bash
npm run consumer
```

Subscribes to `highway/+/telemetry` and `highway/+/alerts` and prints all messages.

### Run Producer
```bash
npm run producer
```

Publishes simulated sensor data every 2 seconds for 60 seconds.

### Run Monitor
```bash
npm run monitor
```

Real-time traffic monitoring with statistics display.

## Topic Patterns

The broker supports MQTT-style topic wildcards:

- `highway/+/telemetry` - Single level wildcard (matches `highway/sensor1/telemetry`)
- `highway/#` - Multi-level wildcard (matches any topic under `highway/`)
- `highway/sensor1/telemetry` - Exact match

## Message Format

### Publishing

```javascript
client.publish('cars', 'My message', QoS.AT_LEAST_ONCE);
```

### Receiving

```javascript
client.on('message', (msg) => {
  const text = msg.data.toString('utf8');
  const json = JSON.parse(msg.data.toString('utf8'));
});
```

## Error Handling

```javascript
client.on('error', (err) => {
  if (err.message.includes('timeout')) {
    console.log('Connection timeout');
  } else if (err.message.includes('Connection rejected')) {
    console.log('Authentication failed');
  }
});
```

## Performance

- **Connection**: ~100ms
- **Publish (QoS 0)**: ~1ms per message
- **Publish (QoS 1)**: ~5-10ms per message (with ACK)
- **Subscribe**: ~10ms
- **Message delivery**: <50ms latency

## Thread/Process Safety

The client is safe to use from a single Node.js process. For multi-process use, create separate client instances.

## Troubleshooting

### Connection fails
- Check broker is running: `./broker` in build directory
- Verify host/port are correct
- Check firewall settings

### Messages not received
- Verify producer is publishing to correct topic
- Check consumer is subscribed to matching topic pattern
- Look at log output for errors

### Slow message delivery
- Check network latency
- Reduce QoS level if not needed (AT_MOST_ONCE is fastest)
- Check broker CPU usage

## License

MIT
