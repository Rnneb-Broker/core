#!/usr/bin/env node

/**
 * WebSocket to TCP Bridge for Highway Broker
 * 
 * This bridge allows browser clients to connect to the TCP-based
 * Highway Broker through WebSockets.
 */

const WebSocket = require('ws');
const net = require('net');

// Configuration
const WS_PORT = process.env.WS_PORT || 8080;
const BROKER_HOST = process.env.BROKER_HOST || 'localhost';
const BROKER_PORT = process.env.BROKER_PORT || 1883;

// Create WebSocket server
const wss = new WebSocket.Server({ 
  port: WS_PORT,
  perMessageDeflate: false // Disable compression for binary data
});

console.log('╔════════════════════════════════════════════╗');
console.log('║  Highway Broker WebSocket Bridge          ║');
console.log('╚════════════════════════════════════════════╝');
console.log(`\n📡 WebSocket server: ws://localhost:${WS_PORT}`);
console.log(`🎯 Broker endpoint:  ${BROKER_HOST}:${BROKER_PORT}\n`);
console.log('Waiting for connections...\n');

let clientIdCounter = 1;

wss.on('connection', (ws, req) => {
  const clientId = clientIdCounter++;
  const clientIp = req.socket.remoteAddress;
  
  console.log(`[${clientId}] 🌐 Browser client connected from ${clientIp}`);
  
  // Create TCP connection to broker
  const tcpClient = net.createConnection({
    host: BROKER_HOST,
    port: BROKER_PORT,
    keepAlive: true
  }, () => {
    console.log(`[${clientId}] ✅ Connected to broker at ${BROKER_HOST}:${BROKER_PORT}`);
  });

  let bytesToBroker = 0;
  let bytesFromBroker = 0;

  // Forward WebSocket messages to TCP broker
  ws.on('message', (data) => {
    try {
      const buffer = Buffer.from(data);
      bytesToBroker += buffer.length;
      tcpClient.write(buffer);
      // console.log(`[${clientId}] → Sent ${buffer.length} bytes to broker`);
    } catch (err) {
      console.error(`[${clientId}] ❌ Error forwarding to broker:`, err.message);
    }
  });

  // Forward TCP broker messages to WebSocket
  tcpClient.on('data', (data) => {
    try {
      bytesFromBroker += data.length;
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(data, { binary: true });
        // console.log(`[${clientId}] ← Received ${data.length} bytes from broker`);
      }
    } catch (err) {
      console.error(`[${clientId}] ❌ Error forwarding from broker:`, err.message);
    }
  });

  // Handle WebSocket close
  ws.on('close', (code, reason) => {
    console.log(`[${clientId}] 🔌 Browser client disconnected (code: ${code})`);
    console.log(`[${clientId}] 📊 Stats: Sent ${bytesToBroker} bytes, Received ${bytesFromBroker} bytes`);
    tcpClient.destroy();
  });

  // Handle WebSocket errors
  ws.on('error', (err) => {
    console.error(`[${clientId}] ❌ WebSocket error:`, err.message);
    tcpClient.destroy();
  });

  // Handle TCP close
  tcpClient.on('close', () => {
    console.log(`[${clientId}] 🔌 Broker connection closed`);
    if (ws.readyState === WebSocket.OPEN) {
      ws.close();
    }
  });

  // Handle TCP errors
  tcpClient.on('error', (err) => {
    console.error(`[${clientId}] ❌ TCP error:`, err.message);
    if (ws.readyState === WebSocket.OPEN) {
      ws.close();
    }
  });

  // Ping interval to keep connection alive
  const pingInterval = setInterval(() => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.ping();
    } else {
      clearInterval(pingInterval);
    }
  }, 30000); // 30 seconds

  ws.on('pong', () => {
    // Client is alive
  });

  // Cleanup on close
  ws.on('close', () => {
    clearInterval(pingInterval);
  });
});

// Handle server errors
wss.on('error', (err) => {
  console.error('❌ WebSocket server error:', err.message);
  process.exit(1);
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\n\n🛑 Shutting down WebSocket bridge...');
  
  wss.clients.forEach((ws) => {
    ws.close();
  });
  
  wss.close(() => {
    console.log('✅ WebSocket server closed');
    process.exit(0);
  });
});

process.on('SIGTERM', () => {
  console.log('\n🛑 Received SIGTERM, shutting down...');
  wss.close(() => {
    process.exit(0);
  });
});

// Log active connections every 30 seconds
setInterval(() => {
  const activeConnections = wss.clients.size;
  if (activeConnections > 0) {
    console.log(`\n📊 Active connections: ${activeConnections}`);
  }
}, 30000);
