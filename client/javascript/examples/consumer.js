#!/usr/bin/env node

/**
 * Consumer Example
 * 
 * Connects to broker and consumes messages from topics
 */

const { HighwayClient, QoS } = require('../highway-client.js');

const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  clientId: 'js-consumer-' + Math.random().toString(36).substr(2, 9),
  autoConnect: true
});

// Handle connection
client.on('connect', () => {
  console.log('\n[+] Connected to broker!\n');

  // Subscribe to sensor telemetry
  client.subscribe('highway/+/telemetry', QoS.AT_LEAST_ONCE, (result) => {
    console.log('[CONSUMER] Subscribed to highway/+/telemetry');
  });

  // Also subscribe to alerts
  client.subscribe('highway/+/alerts', QoS.AT_LEAST_ONCE, (result) => {
    console.log('[CONSUMER] Subscribed to highway/+/alerts');
  });
});

// Handle incoming messages
client.on('message', (msg) => {
  const timestamp = new Date().toLocaleTimeString();
  console.log(`[${timestamp}] Message from "${msg.topic}"`);
  console.log(`  Data: ${msg.data.toString('utf8')}`);
  console.log(`  QoS: ${msg.qos}\n`);
});

// Handle errors
client.on('error', (err) => {
  console.error(`[-] Error: ${err.message}`);
});

// Handle disconnect
client.on('close', () => {
  console.log('[-] Disconnected from broker');
  process.exit(0);
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\n\n[CONSUMER] Shutting down...');
  client.disconnect(() => {
    console.log('[CONSUMER] Disconnected');
    process.exit(0);
  });
});

console.log('🚀 Consumer started, waiting for messages...');
console.log('   Press Ctrl+C to exit\n');
