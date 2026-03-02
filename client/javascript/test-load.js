#!/usr/bin/env node

// Quick test of the client library
const { HighwayClient } = require('./highway-client.js');

console.log('[+] JavaScript client library loaded successfully!');

const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  clientId: 'test-client',
  autoConnect: false
});

console.log('[+] Client instantiated');
console.log(`   Host: ${client.config.host}`);
console.log(`   Port: ${client.config.port}`);
console.log(`   ClientId: ${client.config.clientId}`);
console.log(`   State: ${client.getState()}`);
