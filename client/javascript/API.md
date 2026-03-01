# JavaScript Client API Reference

## HighwayClient Class

The main client for connecting to Highway Broker.

### Constructor

```javascript
const client = new HighwayClient(config)
```

#### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `host` | string | `localhost` | Broker hostname or IP |
| `port` | number | `1883` | Broker port |
| `clientId` | string | `node-{random}` | Unique client identifier |
| `username` | string | `''` | Authentication username |
| `password` | string | `''` | Authentication password |
| `keepalive` | number | `60` | Keepalive interval (seconds) |
| `autoConnect` | boolean | `true` | Auto-connect on creation |

#### Example

```javascript
const { HighwayClient } = require('./highway-client.js');

const client = new HighwayClient({
  host: 'broker.example.com',
  port: 1883,
  clientId: 'my-app-client',
  autoConnect: true
});
```

---

## Core Methods

### `connect(callback)`

Connect to the broker.

```javascript
client.connect((success, err) => {
  if (success) {
    console.log('Connected!');
  } else {
    console.error('Connection failed:', err.message);
  }
});
```

**Returns:** `void`

---

### `disconnect(callback)`

Gracefully disconnect from the broker.

```javascript
client.disconnect(() => {
  console.log('Disconnected');
  process.exit(0);
});
```

**Returns:** `void`

---

### `subscribe(topic, qos, callback)`

Subscribe to a topic. Supports MQTT-style wildcards:
- `+` - Single level wildcard
- `#` - Multi-level wildcard

```javascript
// Single topic
client.subscribe('cars/speed', QoS.AT_LEAST_ONCE);

// Multiple topics (call subscribe multiple times)
client.subscribe('highway/+/telemetry', QoS.AT_LEAST_ONCE);
client.subscribe('highway/+/alerts', QoS.AT_MOST_ONCE);

// All topics under highway
client.subscribe('highway/#', QoS.AT_LEAST_ONCE);
```

**Parameters:**
- `topic` (string): Topic name with optional wildcards
- `qos` (number): Quality of Service level
- `callback` (function): Optional callback

**Returns:** `void`

---

### `unsubscribe(topic, callback)`

Unsubscribe from a topic.

```javascript
client.unsubscribe('cars/speed', () => {
  console.log('Unsubscribed');
});
```

**Returns:** `void`

---

### `publish(topic, data, qos, callback)`

Publish a message to a topic.

```javascript
// String data
client.publish('cars/speed', 'speed=50km/h', QoS.AT_LEAST_ONCE);

// Buffer
client.publish('cars/binary', Buffer.from([1, 2, 3, 4]));

// JSON
client.publish('cars/telemetry', JSON.stringify({
  speed: 50,
  temperature: 25
}), QoS.AT_LEAST_ONCE);

// With callback
client.publish('cars/speed', 'speed=50km/h', QoS.AT_LEAST_ONCE, (success) => {
  console.log('Published:', success);
});
```

**Parameters:**
- `topic` (string): Topic name
- `data` (string|Buffer): Message data
- `qos` (number): Quality of Service level
- `callback` (function): Optional callback(success)

**Returns:** `void`

---

### `isConnected()`

Check if client is connected and authenticated.

```javascript
if (client.isConnected()) {
  client.publish('test', 'message');
}
```

**Returns:** `boolean`

---

### `getState()`

Get the current connection state.

```javascript
const state = client.getState();
console.log(state);
// 'DISCONNECTED' | 'CONNECTING' | 'CONNECTED' | 'AUTHENTICATED' | 'DISCONNECTING'
```

**Returns:** `string`

---

### `getSubscriptions()`

Get list of currently subscribed topics.

```javascript
const topics = client.getSubscriptions();
console.log(topics);
// ['highway/+/telemetry', 'highway/+/alerts']
```

**Returns:** `string[]`

---

## Events

### `connect`

Emitted when successfully connected and authenticated.

```javascript
client.on('connect', () => {
  console.log('Ready to publish/subscribe');
});
```

---

### `message`

Emitted when a message is received.

```javascript
client.on('message', (msg) => {
  console.log('Topic:', msg.topic);           // string
  console.log('Data:', msg.data);             // Buffer
  console.log('Size:', msg.data.length);      // number
  console.log('QoS:', msg.qos);              // number (0,1,2)
  console.log('PacketId:', msg.packetId);    // number
  
  // Convert buffer to string
  const text = msg.data.toString('utf8');
  
  // Parse JSON
  const json = JSON.parse(msg.data.toString('utf8'));
});
```

---

### `error`

Emitted when an error occurs.

```javascript
client.on('error', (err) => {
  console.error('Error:', err.message);
});
```

---

### `close`

Emitted when the connection is closed.

```javascript
client.on('close', () => {
  console.log('Disconnected from broker');
});
```

---

### `suback`

Emitted when subscription is acknowledged.

```javascript
client.on('suback', (result) => {
  console.log('Subscribed - PacketId:', result.packetId);
  console.log('QoS grants:', result.grantedQoSList);
});
```

---

### `puback`

Emitted when publish is acknowledged (QoS > 0).

```javascript
client.on('puback', (result) => {
  console.log('Published - PacketId:', result.packetId);
});
```

---

## Quality of Service (QoS)

```javascript
const { QoS } = require('./highway-client.js');

QoS.AT_MOST_ONCE   // 0 - Fire and forget
QoS.AT_LEAST_ONCE  // 1 - Guaranteed delivery
QoS.EXACTLY_ONCE   // 2 - Single delivery
```

---

## Complete Example

```javascript
const { HighwayClient, QoS } = require('./highway-client.js');

const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  clientId: 'node-app-1'
});

// Handle connection
client.on('connect', () => {
  console.log('[+] Connected');
  
  // Subscribe to topics
  client.subscribe('sensor/+/data', QoS.AT_LEAST_ONCE);
  client.subscribe('alerts/#', QoS.AT_MOST_ONCE);
});

// Handle messages
client.on('message', (msg) => {
  console.log(`Message from ${msg.topic}:`);
  console.log(msg.data.toString('utf8'));
});

// Publish messages
let counter = 0;
setInterval(() => {
  client.publish('app/counter', String(++counter), QoS.AT_LEAST_ONCE);
}, 5000);

// Handle errors
client.on('error', (err) => {
  console.error('Error:', err.message);
});

// Graceful shutdown
process.on('SIGINT', () => {
  client.disconnect(() => {
    console.log('Disconnected');
    process.exit(0);
  });
});
```

---

## Data Types

### QoS (Enum)
- `0` - AT_MOST_ONCE (fire and forget, fastest)
- `1` - AT_LEAST_ONCE (with acknowledgment)
- `2` - EXACTLY_ONCE (guaranteed single delivery)

### Connection State (Enum)
- `DISCONNECTED` - Not connected
- `CONNECTING` - Connection in progress
- `CONNECTED` - Connected but not authenticated
- `AUTHENTICATED` - Connected and ready
- `DISCONNECTING` - Disconnection in progress

### Message Object
```javascript
{
  topic: string         // Topic name
  data: Buffer          // Message bytes
  qos: number          // QoS level (0, 1, 2)
  packetId: number     // Packet identifier
}
```

---

## Error Handling

Common errors and how to handle them:

```javascript
client.on('error', (err) => {
  if (err.message.includes('Connection timeout')) {
    console.log('Broker unreachable - check host/port');
  } else if (err.message.includes('Address already in use')) {
    console.log('Client already connected');
  } else if (err.message.includes('Connection rejected')) {
    console.log('Authentication failed');
  } else {
    console.log('Unknown error:', err.message);
  }
});
```

---

## Performance Tips

1. **Use AT_MOST_ONCE (QoS 0)** for non-critical data (fastest)
2. **Batch messages** instead of publishing one-by-one
3. **Use topic wildcards** to reduce subscription count
4. **Handle `message` events** synchronously (don't await)
5. **Don't reconnect** too frequently on errors

---

## Limits

| Limit | Value |
|-------|-------|
| Max topic length | 1024 bytes |
| Max message size | 16 MB |
| Max ClientId length | 256 bytes |
| Max subscriptions per client | Unlimited |
| Max connections per broker | 10,000 |

---

## See Also

- [README.md](README.md) - Main documentation
- [examples/consumer.js](examples/consumer.js) - Consumer example
- [examples/producer.js](examples/producer.js) - Producer example
- [examples/monitor.js](examples/monitor.js) - Real-time monitor
