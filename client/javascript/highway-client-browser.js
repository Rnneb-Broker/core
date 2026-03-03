/**
 * Highway Broker Browser Client
 * WebSocket-based implementation for browser environments
 */

// Packet Types
const PacketType = {
  CONNECT: 0x10,
  CONNACK: 0x20,
  PUBLISH: 0x30,
  PUBACK: 0x40,
  SUBSCRIBE: 0x80,
  SUBACK: 0x90,
  UNSUBSCRIBE: 0xA0,
  UNSUBACK: 0xB0,
  PINGREQ: 0xC0,
  PINGRESP: 0xD0,
  DISCONNECT: 0xE0,
  // Offset-based access (v1.1)
  FETCH_ONE: 0x50,
  FETCH_RESPONSE: 0x51,
  SUBSCRIBE_FROM_OFFSET: 0x81,
  OFFSET_NOT_FOUND: 0x52
};

// Subscription modes (v1.1)
const SubscriptionMode = {
  PUSH_LIVE: 0,
  CATCHUP_THEN_PUSH: 1
};

// Quality of Service
const QoS = {
  AT_MOST_ONCE: 0,
  AT_LEAST_ONCE: 1,
  EXACTLY_ONCE: 2
};

// Connection states
const State = {
  DISCONNECTED: 'DISCONNECTED',
  CONNECTING: 'CONNECTING',
  CONNECTED: 'CONNECTED',
  AUTHENTICATED: 'AUTHENTICATED',
  DISCONNECTING: 'DISCONNECTING'
};

// Connection result codes
const ConnectResult = {
  ACCEPTED: 0x00,
  UNACCEPTABLE_VERSION: 0x01,
  IDENTIFIER_REJECTED: 0x02,
  SERVER_UNAVAILABLE: 0x03,
  BAD_CREDENTIALS: 0x04,
  NOT_AUTHORIZED: 0x05
};

/**
 * Binary packet writer for serialization
 */
class BinaryWriter {
  constructor() {
    this.buffer = new Uint8Array(0);
  }

  write_u8(value) {
    const b = new Uint8Array(1);
    b[0] = value & 0xFF;
    this.buffer = this._concat(this.buffer, b);
    return this;
  }

  write_u16(value) {
    const b = new Uint8Array(2);
    b[0] = (value >> 8) & 0xFF;
    b[1] = value & 0xFF;
    this.buffer = this._concat(this.buffer, b);
    return this;
  }

  write_u32(value) {
    const b = new Uint8Array(4);
    b[0] = (value >> 24) & 0xFF;
    b[1] = (value >> 16) & 0xFF;
    b[2] = (value >> 8) & 0xFF;
    b[3] = value & 0xFF;
    this.buffer = this._concat(this.buffer, b);
    return this;
  }

  write_u64(value) {
    const b = new Uint8Array(8);
    let bigVal = BigInt(value);
    for (let i = 7; i >= 0; i--) {
      b[i] = Number(bigVal & 0xFFn);
      bigVal = bigVal >> 8n;
    }
    this.buffer = this._concat(this.buffer, b);
    return this;
  }

  write_string(value) {
    const encoder = new TextEncoder();
    const str_buf = encoder.encode(value);
    const len_buf = new Uint8Array(2);
    len_buf[0] = (str_buf.length >> 8) & 0xFF;
    len_buf[1] = str_buf.length & 0xFF;
    this.buffer = this._concat(this.buffer, len_buf);
    this.buffer = this._concat(this.buffer, str_buf);
    return this;
  }

  write_bytes(data) {
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    this.buffer = this._concat(this.buffer, bytes);
    return this;
  }

  release() {
    return this.buffer;
  }

  _concat(a, b) {
    const result = new Uint8Array(a.length + b.length);
    result.set(a, 0);
    result.set(b, a.length);
    return result;
  }
}

/**
 * Binary packet reader for deserialization
 */
class BinaryReader {
  constructor(buffer) {
    this.buffer = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
    this.pos = 0;
  }

  read_u8() {
    const value = this.buffer[this.pos];
    this.pos += 1;
    return value;
  }

  read_u16() {
    const value = (this.buffer[this.pos] << 8) | this.buffer[this.pos + 1];
    this.pos += 2;
    return value;
  }

  read_u32() {
    const value = 
      (this.buffer[this.pos] << 24) |
      (this.buffer[this.pos + 1] << 16) |
      (this.buffer[this.pos + 2] << 8) |
      this.buffer[this.pos + 3];
    this.pos += 4;
    return value >>> 0; // Ensure unsigned
  }

  read_u64() {
    let value = 0n;
    for (let i = 0; i < 8; i++) {
      value = (value << 8n) | BigInt(this.buffer[this.pos + i]);
    }
    this.pos += 8;
    return Number(value);
  }

  read_string() {
    const len = this.read_u16();
    const decoder = new TextDecoder();
    const value = decoder.decode(this.buffer.slice(this.pos, this.pos + len));
    this.pos += len;
    return value;
  }

  read_bytes(len) {
    const value = this.buffer.slice(this.pos, this.pos + len);
    this.pos += len;
    return value;
  }

  read_remaining() {
    const value = this.buffer.slice(this.pos);
    this.pos = this.buffer.length;
    return value;
  }

  empty() {
    return this.pos >= this.buffer.length;
  }
}

/**
 * Packet header structure
 */
function createPacketHeader(type, flags, payloadLen) {
  const header = new Uint8Array(4);
  header[0] = type;
  header[1] = flags;
  header[2] = (payloadLen >> 8) & 0xFF;
  header[3] = payloadLen & 0xFF;
  return header;
}

/**
 * Main Highway Broker Browser Client
 */
class HighwayBrowserClient {
  constructor(config = {}) {
    this.config = {
      url: config.url || 'ws://localhost:8080',
      clientId: config.clientId || `browser-${Math.random().toString(36).substr(2, 9)}`,
      username: config.username || '',
      password: config.password || '',
      keepalive: config.keepalive || 60,
      autoConnect: config.autoConnect !== false
    };

    this.state = State.DISCONNECTED;
    this.socket = null;
    this.nextPacketId = 1;
    this.subscriptions = new Map();
    this.listeners = {
      connect: [],
      message: [],
      error: [],
      close: [],
      suback: [],
      puback: [],
      fetch_response: [],
      offset_not_found: []
    };

    // Partial packet buffer for incomplete reads
    this.partialBuffer = new Uint8Array(0);

    if (this.config.autoConnect) {
      this.connect();
    }
  }

  /**
   * Event listener registration
   */
  on(event, callback) {
    if (!this.listeners[event]) {
      this.listeners[event] = [];
    }
    this.listeners[event].push(callback);
  }

  emit(event, data) {
    if (this.listeners[event]) {
      this.listeners[event].forEach(callback => callback(data));
    }
  }

  removeListener(event, callback) {
    if (this.listeners[event]) {
      const idx = this.listeners[event].indexOf(callback);
      if (idx > -1) {
        this.listeners[event].splice(idx, 1);
      }
    }
  }

  /**
   * Connect to broker via WebSocket
   */
  connect(callback) {
    if (this.state !== State.DISCONNECTED) {
      const err = new Error('Already connected or connecting');
      if (callback) callback(false, err);
      this.emit('error', err);
      return;
    }

    this.state = State.CONNECTING;

    try {
      this.socket = new WebSocket(this.config.url);
      this.socket.binaryType = 'arraybuffer';

      this.socket.onopen = () => {
        console.log(`[CLIENT] WebSocket connected to ${this.config.url}`);
        this.sendConnect();
      };

      this.socket.onmessage = (event) => {
        const data = new Uint8Array(event.data);
        this.onData(data);
      };

      this.socket.onerror = (error) => {
        this.onError(new Error('WebSocket error'));
      };

      this.socket.onclose = () => {
        this.onClose();
      };

      // Store callback for CONNACK
      this.connectCallback = callback;

      // Timeout after 5 seconds
      this.connectTimeout = setTimeout(() => {
        if (this.state === State.CONNECTING) {
          const err = new Error('Connection timeout');
          this.emit('error', err);
          if (callback) callback(false, err);
          this.socket?.close();
        }
      }, 5000);

    } catch (err) {
      this.state = State.DISCONNECTED;
      if (callback) callback(false, err);
      this.emit('error', err);
    }
  }

  /**
   * Send CONNECT packet
   */
  sendConnect() {
    const payload = new BinaryWriter()
      .write_string(this.config.clientId)
      .write_string(this.config.username)
      .write_string(this.config.password)
      .write_u16(this.config.keepalive)
      .release();

    const header = createPacketHeader(PacketType.CONNECT, 0, payload.length);
    const packet = new Uint8Array(header.length + payload.length);
    packet.set(header, 0);
    packet.set(payload, header.length);

    this.socket.send(packet.buffer);
    console.log('[CLIENT] Sent CONNECT');
  }

  /**
   * Handle incoming data
   */
  onData(data) {
    // Append to partial buffer
    const newBuffer = new Uint8Array(this.partialBuffer.length + data.length);
    newBuffer.set(this.partialBuffer, 0);
    newBuffer.set(data, this.partialBuffer.length);
    this.partialBuffer = newBuffer;

    // Process complete packets
    while (this.partialBuffer.length >= 4) {
      const header = {
        type: this.partialBuffer[0],
        flags: this.partialBuffer[1],
        payloadLen: (this.partialBuffer[2] << 8) | this.partialBuffer[3]
      };

      const totalLen = 4 + header.payloadLen;

      if (this.partialBuffer.length < totalLen) {
        break; // Incomplete packet, wait for more data
      }

      // Extract complete packet
      const packet = this.partialBuffer.slice(0, totalLen);
      this.partialBuffer = this.partialBuffer.slice(totalLen);

      // Process packet
      this.processPacket(header, packet.slice(4));
    }
  }

  /**
   * Process incoming packet
   */
  processPacket(header, payload) {
    switch (header.type) {
      case PacketType.CONNACK:
        this.handleConnack(payload);
        break;
      case PacketType.PUBLISH:
        this.handlePublish(header, payload);
        break;
      case PacketType.SUBACK:
        this.handleSuback(payload);
        break;
      case PacketType.PUBACK:
        this.handlePuback(payload);
        break;
      case PacketType.PINGRESP:
        this.handlePingresp();
        break;
      case PacketType.FETCH_RESPONSE:
        this.handleFetchResponse(payload);
        break;
      case PacketType.OFFSET_NOT_FOUND:
        this.handleOffsetNotFound(payload);
        break;
      default:
        console.warn(`[CLIENT] Unknown packet type: 0x${header.type.toString(16)}`);
    }
  }

  /**
   * Handle CONNACK response
   */
  handleConnack(payload) {
    clearTimeout(this.connectTimeout);

    if (payload.length < 2) {
      const err = new Error('Invalid CONNACK packet');
      this.emit('error', err);
      if (this.connectCallback) this.connectCallback(false, err);
      return;
    }

    const result = payload[1];

    if (result === ConnectResult.ACCEPTED) {
      console.log('[CLIENT] Connected and authenticated');
      this.state = State.AUTHENTICATED;
      this.emit('connect');
      if (this.connectCallback) this.connectCallback(true);
    } else {
      const err = new Error(`Connection rejected: code ${result}`);
      this.emit('error', err);
      if (this.connectCallback) this.connectCallback(false, err);
      this.socket?.close();
    }
  }

  /**
   * Handle incoming PUBLISH message (with offset metadata - v1.1)
   */
  handlePublish(header, payload) {
    try {
      const reader = new BinaryReader(payload);
      const topic = reader.read_string();
      const packetId = reader.read_u16();
      const offset = reader.read_u64();  // NEW: Offset metadata
      const data = reader.read_remaining();

      const qos = (header.flags >> 1) & 0x03;

      // Send PUBACK if QoS > 0
      if (qos === QoS.AT_LEAST_ONCE) {
        this.sendPuback(packetId);
      }

      // Emit message event (with offset)
      this.emit('message', {
        topic,
        data,
        qos,
        packetId,
        offset  // NEW: Include offset in message event
      });
    } catch (err) {
      this.emit('error', new Error(`Failed to parse PUBLISH: ${err.message}`));
    }
  }

  /**
   * Handle SUBACK (subscription acknowledgment)
   */
  handleSuback(payload) {
    try {
      const reader = new BinaryReader(payload);
      const packetId = reader.read_u16();

      const grantedQoSList = [];
      while (!reader.empty()) {
        grantedQoSList.push(reader.read_u8());
      }

      console.log(`[CLIENT] SUBACK: packetId=${packetId}, grants=${grantedQoSList}`);
      this.emit('suback', { packetId, grantedQoSList });
    } catch (err) {
      this.emit('error', new Error(`Failed to parse SUBACK: ${err.message}`));
    }
  }

  /**
   * Handle PUBACK (publish acknowledgment)
   */
  handlePuback(payload) {
    try {
      const reader = new BinaryReader(payload);
      const packetId = reader.read_u16();
      console.log(`[CLIENT] PUBACK: packetId=${packetId}`);
      this.emit('puback', { packetId });
    } catch (err) {
      this.emit('error', new Error(`Failed to parse PUBACK: ${err.message}`));
    }
  }

  /**
   * Handle PINGRESP
   */
  handlePingresp() {
    console.log('[CLIENT] PINGRESP received');
  }

  /**
   * Handle FETCH_RESPONSE (v1.1 - offset-based access)
   */
  handleFetchResponse(payload) {
    try {
      const reader = new BinaryReader(payload);
      const topic = reader.read_string();
      const offset = reader.read_u64();
      const data = reader.read_remaining();

      this.emit('fetch_response', {
        topic,
        offset,
        data
      });

      console.log(`[CLIENT] Received FETCH_RESPONSE: topic="${topic}", offset=${offset}, size=${data.length}`);
    } catch (err) {
      this.emit('error', new Error(`Failed to parse FETCH_RESPONSE: ${err.message}`));
    }
  }

  /**
   * Handle OFFSET_NOT_FOUND (v1.1 - offset-based access)
   */
  handleOffsetNotFound(payload) {
    try {
      const reader = new BinaryReader(payload);
      const topic = reader.read_string();
      const requestedOffset = reader.read_u64();
      const oldestAvailable = reader.read_u64();
      const newestAvailable = reader.read_u64();

      this.emit('offset_not_found', {
        topic,
        requestedOffset,
        oldestAvailable,
        newestAvailable
      });

      console.log(`[CLIENT] Offset not found: topic="${topic}", requested=${requestedOffset}, ` +
                  `oldest=${oldestAvailable}, newest=${newestAvailable}`);
    } catch (err) {
      this.emit('error', new Error(`Failed to parse OFFSET_NOT_FOUND: ${err.message}`));
    }
  }

  /**
   * Error handler
   */
  onError(err) {
    console.error(`[CLIENT] Socket error: ${err.message}`);
    this.emit('error', err);
    if (this.connectCallback) {
      this.connectCallback(false, err);
      this.connectCallback = null;
    }
  }

  /**
   * Close handler
   */
  onClose() {
    console.log('[CLIENT] Connection closed');
    this.state = State.DISCONNECTED;
    this.socket = null;
    this.emit('close');
  }

  /**
   * Subscribe to topic
   */
  subscribe(topic, qos = QoS.AT_MOST_ONCE, callback) {
    if (this.state !== State.AUTHENTICATED) {
      const err = new Error('Not connected');
      this.emit('error', err);
      if (callback) callback(false, err);
      return;
    }

    const packetId = this.nextPacketId++;
    const payload = new BinaryWriter()
      .write_u16(packetId)
      .write_string(topic)
      .write_u8(qos)
      .release();

    const header = createPacketHeader(PacketType.SUBSCRIBE, 0x02, payload.length);
    const packet = new Uint8Array(header.length + payload.length);
    packet.set(header, 0);
    packet.set(payload, header.length);

    this.socket.send(packet.buffer);
    this.subscriptions.set(topic, qos);

    console.log(`[CLIENT] Sent SUBSCRIBE: topic="${topic}", QoS=${qos}`);

    if (callback) {
      const handler = () => {
        callback(true);
      };
      this.listeners.suback.push(handler);
      // Remove after first call
      setTimeout(() => {
        const idx = this.listeners.suback.indexOf(handler);
        if (idx > -1) this.listeners.suback.splice(idx, 1);
      }, 5000);
    }
  }

  /**
   * Publish message
   */
  publish(topic, data, qos = QoS.AT_MOST_ONCE, callback) {
    if (this.state !== State.AUTHENTICATED) {
      const err = new Error('Not connected');
      this.emit('error', err);
      if (callback) callback(false, err);
      return;
    }

    const packetId = qos > QoS.AT_MOST_ONCE ? this.nextPacketId++ : 0;

    // Convert string to Uint8Array if needed
    const encoder = new TextEncoder();
    const dataBuffer = typeof data === 'string' ? encoder.encode(data) : new Uint8Array(data);

    const payload = new BinaryWriter()
      .write_string(topic)
      .write_u16(packetId)
      .write_bytes(dataBuffer)
      .release();

    const flags = (qos << 1) & 0x06;
    const header = createPacketHeader(PacketType.PUBLISH, flags, payload.length);
    const packet = new Uint8Array(header.length + payload.length);
    packet.set(header, 0);
    packet.set(payload, header.length);

    this.socket.send(packet.buffer);

    console.log(`[CLIENT] Sent PUBLISH: topic="${topic}", size=${dataBuffer.length}, QoS=${qos}`);

    if (callback) {
      if (qos === QoS.AT_MOST_ONCE) {
        callback(true);
      } else {
        const handler = () => {
          callback(true);
        };
        this.listeners.puback.push(handler);
        setTimeout(() => {
          const idx = this.listeners.puback.indexOf(handler);
          if (idx > -1) this.listeners.puback.splice(idx, 1);
        }, 5000);
      }
    }
  }

  /**
   * Send PUBACK
   */
  sendPuback(packetId) {
    const payload = new BinaryWriter()
      .write_u16(packetId)
      .release();

    const header = createPacketHeader(PacketType.PUBACK, 0, payload.length);
    const packet = new Uint8Array(header.length + payload.length);
    packet.set(header, 0);
    packet.set(payload, header.length);

    this.socket.send(packet.buffer);
  }

  /**
   * Fetch a single message from a specific offset (v1.1 - stateless)
   */
  fetchOne(topic, offset, callback) {
    if (this.state !== State.AUTHENTICATED) {
      const err = new Error('Not connected');
      this.emit('error', err);
      if (callback) callback(null, err);
      return;
    }

    const payload = new BinaryWriter()
      .write_string(topic)
      .write_u64(offset)
      .release();

    const header = createPacketHeader(PacketType.FETCH_ONE, 0, payload.length);
    const packet = new Uint8Array(header.length + payload.length);
    packet.set(header, 0);
    packet.set(payload, header.length);

    this.socket.send(packet.buffer);

    console.log(`[CLIENT] Sent FETCH_ONE: topic="${topic}", offset=${offset}`);

    if (callback) {
      const handler = (event) => {
        // Verify it's for this topic
        if (event.topic === topic && event.offset === offset) {
          callback(event.data, null, event.offset);
          // Remove handler
          this.removeListener('fetch_response', handler);
        }
      };
      
      const errorHandler = (event) => {
        if (event.topic === topic && event.requestedOffset === offset) {
          const err = new Error(`Offset not available for topic "${topic}"`);
          err.oldest = event.oldestAvailable;
          err.newest = event.newestAvailable;
          callback(null, err);
          // Remove handlers
          this.removeListener('offset_not_found', errorHandler);
        }
      };
      
      this.on('fetch_response', handler);
      this.on('offset_not_found', errorHandler);
      
      // Remove after timeout
      setTimeout(() => {
        this.removeListener('fetch_response', handler);
        this.removeListener('offset_not_found', errorHandler);
      }, 5000);
    }
  }

  /**
   * Subscribe from specific offset with catchup mode (v1.1)
   */
  subscribeFromOffset(topic, startOffset, qos = QoS.AT_MOST_ONCE, callback) {
    if (this.state !== State.AUTHENTICATED) {
      const err = new Error('Not connected');
      this.emit('error', err);
      if (callback) callback(false, err);
      return;
    }

    const packetId = this.nextPacketId++;
    const payload = new BinaryWriter()
      .write_u16(packetId)
      .write_string(topic)
      .write_u64(startOffset)  // start_offset comes BEFORE qos
      .write_u8(qos)
      .release();

    const header = createPacketHeader(PacketType.SUBSCRIBE_FROM_OFFSET, 0x02, payload.length);
    const packet = new Uint8Array(header.length + payload.length);
    packet.set(header, 0);
    packet.set(payload, header.length);

    this.socket.send(packet.buffer);
    this.subscriptions.set(topic, qos);

    console.log(`[CLIENT] Sent SUBSCRIBE_FROM_OFFSET: topic="${topic}", offset=${startOffset}, QoS=${qos}`);

    if (callback) {
      const handler = () => {
        callback(true);
      };
      this.on('suback', handler);
      // Remove after first call and timeout
      setTimeout(() => {
        this.removeListener('suback', handler);
      }, 5000);
    }
  }

  /**
   * Disconnect
   */
  disconnect(callback) {
    if (this.state === State.DISCONNECTED) {
      if (callback) callback();
      return;
    }

    this.state = State.DISCONNECTING;

    const header = createPacketHeader(PacketType.DISCONNECT, 0, 0);
    this.socket.send(header.buffer);

    setTimeout(() => {
      this.socket?.close();
      this.state = State.DISCONNECTED;
      if (callback) callback();
    }, 1000);
  }

  /**
   * Is connected
   */
  isConnected() {
    return this.state === State.AUTHENTICATED;
  }

  /**
   * Get current state
   */
  getState() {
    return this.state;
  }

  /**
   * Get subscribed topics
   */
  getSubscriptions() {
    return Array.from(this.subscriptions.keys());
  }
}

// Export for use in HTML
if (typeof window !== 'undefined') {
  window.HighwayBrowserClient = HighwayBrowserClient;
  window.QoS = QoS;
  window.State = State;
}
