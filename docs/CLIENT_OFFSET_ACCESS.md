# Rabbit Client Library Updates - Offset-Based Access (v1.1)

## Overview

The JavaScript and Python client libraries have been updated to support the new offset-based access features:

1. **FETCH_ONE** - Stateless random offset read
2. **SUBSCRIBE_FROM_OFFSET** - Catch-up-then-push subscription mode
3. **Offset Metadata** - All delivered messages include offset information

## JavaScript Client Updates

### New Packet Types

```javascript
const PacketType = {
  // ... existing types ...
  FETCH_ONE: 0x50,
  FETCH_RESPONSE: 0x51,
  SUBSCRIBE_FROM_OFFSET: 0x81,
  OFFSET_NOT_FOUND: 0x52
};

const SubscriptionMode = {
  PUSH_LIVE: 0,
  CATCHUP_THEN_PUSH: 1
};
```

### New Methods

#### 1. fetchOne(topic, offset, callback)

Fetch a single message by offset (stateless operation).

```javascript
client.fetchOne('rabbit/A1/telemetry', 42, (data, error, offset) => {
  if (error) {
    console.log(`Error: ${error.message}`);
    console.log(`Available range: ${error.oldestAvailable}-${error.newestAvailable}`);
    return;
  }
  
  console.log(`Message at offset ${offset}: ${data.toString()}`);
});
```

**Error Handling:**
- Returns error with `oldestAvailable` and `newestAvailable` if offset is invalid
- No subscription is created

#### 2. subscribeFromOffset(topic, startOffset, qos, callback)

Subscribe to a topic starting from a specific offset, replaying messages until caught up.

```javascript
client.subscribeFromOffset('rabbit/A1/telemetry', 0, QoS.AT_LEAST_ONCE, (ack) => {
  console.log(`Subscription acknowledged: ${JSON.stringify(ack)}`);
});

// Messages will automatically flow with offset metadata
client.on('message', (msg) => {
  console.log(`Offset: ${msg.offset}, Data: ${msg.data}`);
  // Automatically switches to PUSH_LIVE when caught up
});
```

**Behavior:**
- Creates subscription in `CATCHUP_THEN_PUSH` mode
- Validates start_offset against available range
- Automatically switches to `PUSH_LIVE` when caught up
- Messages include offset metadata

### Updated Message Events

Messages now include offset metadata:

```javascript
client.on('message', (msg) => {
  console.log({
    topic: msg.topic,
    offset: msg.offset,        // NEW: Message offset
    data: msg.data,
    qos: msg.qos,
    packetId: msg.packetId
  });
});

// Message handlers receive offset parameter
client.onMessage((topic, data, offset) => {
  console.log(`[${topic}] offset=${offset}: ${data}`);
});
```

### New Events

```javascript
// FETCH_RESPONSE received
client.on('fetchResponse', (msg) => {
  console.log(`Response: offset=${msg.offset}, size=${msg.data.length}`);
});

// OFFSET_NOT_FOUND error
client.on('offsetNotFound', (error) => {
  console.log(`Offset ${error.requestedOffset} not found`);
  console.log(`Available: ${error.oldestAvailable}-${error.newestAvailable}`);
});
```

## Python Client Updates

### New Packet Types

```python
class PacketType(IntEnum):
    # ... existing types ...
    FETCH_ONE = 0x50
    FETCH_RESPONSE = 0x51
    SUBSCRIBE_FROM_OFFSET = 0x81
    OFFSET_NOT_FOUND = 0x52

class SubscriptionMode(IntEnum):
    PUSH_LIVE = 0
    CATCHUP_THEN_PUSH = 1
```

### New Methods

#### 1. fetch_one(topic, offset, callback)

Fetch a single message by offset (stateless operation).

```python
def handle_fetch_response(data, error, offset):
    if error:
        print(f"Error: {error['message']}")
        print(f"Available range: {error['oldest_available']}-{error['newest_available']}")
        return
    
    print(f"Message at offset {offset}: {data}")

client.fetch_one('rabbit/A1/telemetry', 42, handle_fetch_response)
```

**Error Response:**
```python
{
    'topic': 'rabbit/A1/telemetry',
    'requested_offset': 42,
    'oldest_available': 0,
    'newest_available': 100,
    'message': 'Offset not found: 42, available range: 0-100'
}
```

#### 2. subscribe_from_offset(topic, start_offset, qos, callback)

Subscribe to a topic starting from a specific offset with catch-up replay.

```python
def on_subscribe_ack(ack):
    print(f"Subscription acknowledged: {ack}")

client.subscribe_from_offset('rabbit/A1/telemetry', 0, 
                            QoS.AT_LEAST_ONCE, on_subscribe_ack)

# Messages flow with offset metadata
def handle_message(msg):
    print(f"Offset: {msg['offset']}, Data: {msg['data']}")
    # Automatically switches to PUSH_LIVE when caught up

client.on_message(handle_message)
```

**Behavior:**
- Creates subscription in `CATCHUP_THEN_PUSH` mode
- Validates start_offset against available range
- Returns error in SUBACK if offset invalid
- Automatically switches to `PUSH_LIVE` when caught up

### Updated Message Events

Messages now include offset metadata:

```python
# Message event
def handle_message(msg):
    print({
        'topic': msg['topic'],
        'offset': msg['offset'],        # NEW: Message offset
        'data': msg['data'],
        'qos': msg['qos'],
        'packet_id': msg['packet_id']
    })

client.on_message(handle_message)

# Direct handler
def on_msg(topic, data, offset):
    print(f"[{topic}] offset={offset}: {data}")

client.on_message(on_msg)
```

### New Events

```python
# FETCH_RESPONSE received
def handle_fetch_response(msg):
    print(f"Response: offset={msg['offset']}, size={len(msg['data'])}")

client.on('fetchResponse', handle_fetch_response)

# OFFSET_NOT_FOUND error
def handle_offset_not_found(error):
    print(f"Offset {error['requested_offset']} not found")
    print(f"Available: {error['oldest_available']}-{error['newest_available']}")

client.on('offsetNotFound', handle_offset_not_found)
```

## Usage Examples

### JavaScript - Complete Example

```javascript
const RabbitClient = require('./rabbit-client');

const client = new RabbitClient({
  host: 'localhost',
  port: 1883,
  clientId: 'my-consumer',
  autoConnect: true
});

client.on('connect', () => {
  console.log('Connected to RabbitBroker');
  
  // Example 1: Fetch a single message by offset
  client.fetchOne('rabbit/A1/telemetry', 42, (data, error, offset) => {
    if (error) {
      console.log(`Fetch failed: ${error.message}`);
      return;
    }
    console.log(`Got message: offset=${offset}, data=${data}`);
  });
  
  // Example 2: Subscribe from offset (catch-up then live)
  client.subscribeFromOffset('rabbit/A2/telemetry', 0, 1, (ack) => {
    console.log(`Subscription created: ${JSON.stringify(ack)}`);
  });
});

client.on('message', (msg) => {
  console.log(`[${msg.topic}] offset=${msg.offset}: ${msg.data}`);
});

client.on('error', (error) => {
  console.error(`Client error: ${error.message}`);
});

client.on('offsetNotFound', (error) => {
  console.log(`Offset ${error.requestedOffset} not in range [${error.oldestAvailable}, ${error.newestAvailable}]`);
});
```

### Python - Complete Example

```python
from rabbit_client import RabbitClient, QoS

client = RabbitClient('localhost', 1883, 'my-consumer')

def on_connect():
    print('Connected to RabbitBroker')
    
    # Example 1: Fetch a single message by offset
    def handle_fetch(data, error, offset):
        if error:
            print(f'Fetch failed: {error}')
            return
        print(f'Got message: offset={offset}, data={data}')
    
    client.fetch_one('rabbit/A1/telemetry', 42, handle_fetch)
    
    # Example 2: Subscribe from offset (catch-up then live)
    def handle_subscribe_ack(ack):
        print(f'Subscription created: {ack}')
    
    client.subscribe_from_offset('rabbit/A2/telemetry', 0, QoS.AT_LEAST_ONCE, handle_subscribe_ack)

def on_message(msg):
    print(f"[{msg['topic']}] offset={msg['offset']}: {msg['data']}")
    # Automatically tracks catch-up progress when in CATCHUP_THEN_PUSH mode

def on_offset_error(error):
    print(f"Offset {error['requested_offset']} not in range [{error['oldest_available']}, {error['newest_available']}]")

client.on('connect', on_connect)
client.on_message(on_message)
client.on('offsetNotFound', on_offset_error)

client.connect()

# Keep running
import time
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    client.close()
```

## Protocol Details

### FETCH_ONE Request

```
Payload:
  topic (string)      - Topic name
  offset (u64)        - Message offset to fetch
```

### FETCH_RESPONSE Response

```
Payload:
  topic (string)      - Topic name
  offset (u64)        - Message offset
  data (bytes)        - Message payload
```

### SUBSCRIBE_FROM_OFFSET Request

```
Payload:
  packet_id (u16)     - Packet ID for acknowledgment
  topic (string)      - Topic name
  start_offset (u64)  - Starting offset for replay
  qos (u8)            - Requested QoS
```

### OFFSET_NOT_FOUND Response

```
Payload:
  topic (string)           - Topic name
  requested_offset (u64)   - Offset that was requested
  oldest_available (u64)   - First available offset (after retention)
  newest_available (u64)   - Latest available offset
```

### PUBLISH Message (Updated with Offset)

```
Payload (v1.1):
  topic (string)      - Topic name
  packet_id (u16)     - Packet ID for QoS > 0
  offset (u64)        - Message offset (NEW)
  data (bytes)        - Message payload
```

## Backward Compatibility

- Existing code using push-only SUBSCRIBE continues to work unchanged
- New offset field in PUBLISH is optional (defaults to 0 for non-storage messages)
- New packet types don't conflict with existing types
- Subscription tracking accommodates both simple QoS and complex mode+offset tracking

## Migration Guide

### From v1.0 to v1.1

**No breaking changes!** Existing code works as-is:

```javascript
// This still works exactly the same
client.subscribe('rabbit/A1/telemetry', QoS.AT_MOST_ONCE);

client.on('message', (msg) => {
  // Can ignore offset if using v1.0 mode
  console.log(msg.data);
  // Or use new offset if needed
  console.log(`offset: ${msg.offset}`);
});
```

**To use new features, just add calls:**

```javascript
// Opt-in to offset-based features
client.fetchOne(topic, offset, callback);
client.subscribeFromOffset(topic, startOffset, qos, callback);
```

## Error Handling

### JavaScript

```javascript
client.on('offsetNotFound', (error) => {
  console.log(`Topic: ${error.topic}`);
  console.log(`Requested: ${error.requestedOffset}`);
  console.log(`Available: ${error.oldestAvailable}-${error.newestAvailable}`);
});
```

### Python

```python
def handle_offset_error(error):
    print(f"Topic: {error['topic']}")
    print(f"Requested: {error['requested_offset']}")
    print(f"Available: {error['oldest_available']}-{error['newest_available']}")

client.on('offsetNotFound', handle_offset_error)
```

## Performance Considerations

1. **FETCH_ONE**: Stateless, no subscription overhead - good for random access
2. **SUBSCRIBE_FROM_OFFSET**: Subscription maintained on broker - good for continuous replay
3. **Offset Metadata**: Minimal overhead (8 bytes per message)
4. **Automatic Catch-Up**: Non-blocking - doesn't stall live message delivery

## Limitations and Future Work

- Catch-up replay currently processes messages synchronously (can block briefly)
- No explicit backpressure mechanism yet (relies on socket buffers)
- Offset tracking is per-subscription, not persistent (client must maintain state if needed)
- Mode switching currently automatic (could add explicit control in future)

## Support and Debugging

Enable verbose logging:

```javascript
// JavaScript
const client = new RabbitClient({
  host: 'localhost',
  port: 1883,
  clientId: 'debug-client',
  debug: true  // Enable detailed logging
});
```

```python
# Python
import logging
logging.basicConfig(level=logging.DEBUG)
client = RabbitClient('localhost', 1883, 'debug-client')
```

## See Also

- [OFFSET_ACCESS_IMPLEMENTATION.md](OFFSET_ACCESS_IMPLEMENTATION.md) - Broker implementation details
- [Rabbit Protocol](PROTOCOL.md) - Full protocol specification
- [JavaScript Client Documentation](../client/javascript/README.md)
- [Python Client Documentation](../client/python/README.md)
