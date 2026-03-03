/**
 * Highway Broker JavaScript Client
 * Implements MQTT-lite protocol for message broker communication
 */

const net = require('net');
const EventEmitter = require('events');

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
    this.buffer = Buffer.alloc(0);
  }

  write_u8(value) {
    const b = Buffer.alloc(1);
    b.writeUInt8(value, 0);
    this.buffer = Buffer.concat([this.buffer, b]);
    return this;
  }

  write_u16(value) {
    const b = Buffer.alloc(2);
    b.writeUInt16BE(value, 0);
    this.buffer = Buffer.concat([this.buffer, b]);
    return this;
  }

  write_u32(value) {
    const b = Buffer.alloc(4);
    b.writeUInt32BE(value, 0);
    this.buffer = Buffer.concat([this.buffer, b]);
    return this;
  }

  write_u64(value) {
    const b = Buffer.alloc(8);
    b.writeBigUInt64BE(BigInt(value), 0);
    this.buffer = Buffer.concat([this.buffer, b]);
    return this;
  }

  write_string(value) {
    const str_buf = Buffer.from(value, 'utf8');
    const len_buf = Buffer.alloc(2);
    len_buf.writeUInt16BE(str_buf.length, 0);
    this.buffer = Buffer.concat([this.buffer, len_buf, str_buf]);
    return this;
  }

  write_bytes(data) {
    if (Buffer.isBuffer(data)) {
      this.buffer = Buffer.concat([this.buffer, data]);
    } else {
      this.buffer = Buffer.concat([this.buffer, Buffer.from(data)]);
    }
    return this;
  }

  release() {
    return this.buffer;
  }
}

/**
 * Binary packet reader for deserialization
 */
class BinaryReader {
  constructor(buffer) {
    this.buffer = buffer;
    this.pos = 0;
  }

  read_u8() {
    const value = this.buffer.readUInt8(this.pos);
    this.pos += 1;
    return value;
  }

  read_u16() {
    const value = this.buffer.readUInt16BE(this.pos);
    this.pos += 2;
    return value;
  }

  read_u32() {
    const value = this.buffer.readUInt32BE(this.pos);
    this.pos += 4;
    return value;
  }

  read_u64() {
    const value = this.buffer.readBigUInt64BE(this.pos);
    this.pos += 8;
    return Number(value);
  }

  read_string() {
    const len = this.read_u16();
    const value = this.buffer.toString('utf8', this.pos, this.pos + len);
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
  const header = Buffer.alloc(4);
  header.writeUInt8(type, 0);
  header.writeUInt8(flags, 1);
  header.writeUInt16BE(payloadLen, 2);
  return header;
}

/**
 * Main Highway Broker Client
 */
class HighwayClient extends EventEmitter {
  constructor(config = {}) {
    super();

    this.config = {
      host: config.host || 'localhost',
      port: config.port || 1883,
      clientId: config.clientId || `node-${Math.random().toString(36).substr(2, 9)}`,
      username: config.username || '',
      password: config.password || '',
      keepalive: config.keepalive || 60,
      autoConnect: config.autoConnect !== false
    };

    this.state = State.DISCONNECTED;
    this.socket = null;
    this.nextPacketId = 1;
    this.subscriptions = new Map();
    this.messageHandlers = [];
    this.errorHandlers = [];

    // Partial packet buffer for incomplete reads
    this.partialBuffer = Buffer.alloc(0);

    if (this.config.autoConnect) {
      this.connect();
    }
  }

  /**
   * Connect to broker
   */
  connect(callback) {
    if (this.state !== State.DISCONNECTED) {
      const err = new Error('Already connected or connecting');
      if (callback) callback(false, err);
      this.emit('error', err);
      return;
    }

    this.state = State.CONNECTING;

    this.socket = net.createConnection(
      {
        host: this.config.host,
        port: this.config.port,
        keepalive: true
      },
      () => {
        console.log(`[CLIENT] Connected to ${this.config.host}:${this.config.port}`);
        this.sendConnect();
      }
    );

    this.socket.on('data', (data) => this.onData(data));
    this.socket.on('error', (err) => this.onError(err));
    this.socket.on('close', () => this.onClose());

    // Store callback for CONNACK
    this.connectCallback = callback;

    // Timeout after 5 seconds
    this.connectTimeout = setTimeout(() => {
      if (this.state === State.CONNECTING) {
        const err = new Error('Connection timeout');
        this.emitError(err);
        if (callback) callback(false, err);
        this.socket?.destroy();
      }
    }, 5000);
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
    const packet = Buffer.concat([header, payload]);

    this.socket.write(packet);
    console.log('[CLIENT] Sent CONNECT');
  }

  /**
   * Handle incoming data
   */
  onData(data) {
    // Append to partial buffer
    this.partialBuffer = Buffer.concat([this.partialBuffer, data]);

    // Process complete packets
    while (this.partialBuffer.length >= 4) {
      const header = {
        type: this.partialBuffer.readUInt8(0),
        flags: this.partialBuffer.readUInt8(1),
        payloadLen: this.partialBuffer.readUInt16BE(2)
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
      this.emitError(err);
      if (this.connectCallback) this.connectCallback(false, err);
      return;
    }

    const result = payload.readUInt8(1);

    if (result === ConnectResult.ACCEPTED) {
      console.log('[CLIENT] Connected and authenticated');
      this.state = State.AUTHENTICATED;
      this.emit('connect');
      if (this.connectCallback) this.connectCallback(true);
    } else {
      const err = new Error(`Connection rejected: code ${result}`);
      this.emitError(err);
      if (this.connectCallback) this.connectCallback(false, err);
      this.socket?.destroy();
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

      // console.log(`[CLIENT] PUBLISH: topic="${topic}", offset=${offset}, QoS=${qos}, size=${data.length}`);

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

      // Call message handlers
      for (const handler of this.messageHandlers) {
        handler(topic, data, offset);  // Pass offset to handler
      }
    } catch (err) {
      this.emitError(new Error(`Failed to parse PUBLISH: ${err.message}`));
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
      this.emitError(new Error(`Failed to parse SUBACK: ${err.message}`));
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
      this.emitError(new Error(`Failed to parse PUBACK: ${err.message}`));
    }
  }

  /**
   * Handle PINGRESP
   */
  handlePingresp() {
    console.log('[CLIENT] PINGRESP received');
  }

  /**
   * Error handler
   */
  onError(err) {
    console.error(`[CLIENT] Socket error: ${err.message}`);
    this.emitError(err);
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
      this.emitError(err);
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
    const packet = Buffer.concat([header, payload]);

    this.socket.write(packet);
    this.subscriptions.set(topic, qos);

    console.log(`[CLIENT] Sent SUBSCRIBE: topic="${topic}", QoS=${qos}`);

    if (callback) {
      this.once('suback', callback);
    }
  }

  /**
   * Unsubscribe from topic
   */
  unsubscribe(topic, callback) {
    if (this.state !== State.AUTHENTICATED) {
      const err = new Error('Not connected');
      this.emitError(err);
      if (callback) callback(false, err);
      return;
    }

    const packetId = this.nextPacketId++;
    const payload = new BinaryWriter()
      .write_u16(packetId)
      .write_string(topic)
      .release();

    const header = createPacketHeader(PacketType.UNSUBSCRIBE, 0x02, payload.length);
    const packet = Buffer.concat([header, payload]);

    this.socket.write(packet);
    this.subscriptions.delete(topic);

    console.log(`[CLIENT] Sent UNSUBSCRIBE: topic="${topic}"`);

    if (callback) {
      this.once('unsuback', callback);
    }
  }

  /**
   * Publish message
   */
  publish(topic, data, qos = QoS.AT_MOST_ONCE, callback) {
    if (this.state !== State.AUTHENTICATED) {
      const err = new Error('Not connected');
      this.emitError(err);
      if (callback) callback(false, err);
      return;
    }

    const packetId = qos > QoS.AT_MOST_ONCE ? this.nextPacketId++ : 0;

    // Convert string to buffer if needed
    const dataBuffer = typeof data === 'string' ? Buffer.from(data, 'utf8') : data;

    const payload = new BinaryWriter()
      .write_string(topic)
      .write_u16(packetId)
      .write_bytes(dataBuffer)
      .release();

    const flags = (qos << 1) & 0x06;
    const header = createPacketHeader(PacketType.PUBLISH, flags, payload.length);
    const packet = Buffer.concat([header, payload]);

    this.socket.write(packet);

    console.log(`[CLIENT] Sent PUBLISH: topic="${topic}", size=${dataBuffer.length}, QoS=${qos}`);

    if (callback) {
      if (qos === QoS.AT_MOST_ONCE) {
        callback(true);
      } else {
        this.once('puback', () => callback(true));
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
    const packet = Buffer.concat([header, payload]);

    this.socket.write(packet);
  }

  /**
   * Fetch single message by offset (stateless) - v1.1
   * @param {string} topic Topic name
   * @param {number} offset Message offset
   * @param {function} callback Called with (data, offset) or (null, error)
   */
  fetchOne(topic, offset, callback) {
    if (this.state !== State.AUTHENTICATED) {
      const err = new Error('Not connected');
      this.emitError(err);
      if (callback) callback(null, err);
      return;
    }

    const payload = new BinaryWriter()
      .write_string(topic)
      .write_u64(offset)
      .release();

    const header = createPacketHeader(PacketType.FETCH_ONE, 0, payload.length);
    const packet = Buffer.concat([header, payload]);

    this.socket.write(packet);

    console.log(`[CLIENT] Sent FETCH_ONE: topic="${topic}", offset=${offset}`);

    if (callback) {
      // Store callback for response (use topic:offset as key)
      const callbackKey = `fetch_${topic}_${offset}`;
      this.on('fetchResponse', (msg) => {
        if (msg.topic === topic && msg.offset === offset) {
          this.removeAllListeners('fetchResponse');
          callback(msg.data, null, msg.offset);
        }
      });
      this.on('offsetNotFound', (err) => {
        if (err.topic === topic && err.requestedOffset === offset) {
          this.removeAllListeners('offsetNotFound');
          callback(null, err);
        }
      });
    }
  }




  /**
   * Set message handler callback
   */
  onMessage(handler) {
    this.messageHandlers.push(handler);
  }

  /**
   * Set error handler callback
   */
  onError(handler) {
    this.errorHandlers.push(handler);
  }

  /**
   * Emit error
   */
  emitError(err) {
    console.error(`[ERROR] ${err.message}`);
    this.emit('error', err);
    for (const handler of this.errorHandlers) {
      handler(err.message);
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
    this.socket.write(header);

    setTimeout(() => {
      this.socket?.destroy();
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

module.exports = {
  HighwayClient,
  QoS,
  PacketType,
  State,
  ConnectResult,
  BinaryReader,
  BinaryWriter
};
