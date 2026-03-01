#!/usr/bin/env node

/**
 * Traffic Monitor Example
 * 
 * Monitors sensor telemetry and processes alerts
 */

const { HighwayClient, QoS } = require('../../highway-client.js');

const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  clientId: 'traffic-monitor-' + Math.random().toString(36).substr(2, 9),
  autoConnect: true
});

// Track sensor statistics
const sensorStats = new Map();

// Message counters
let telemetryCount = 0;
let alertCount = 0;

function initSensorStats(sensorId) {
  if (!sensorStats.has(sensorId)) {
    sensorStats.set(sensorId, {
      count: 0,
      avgSpeed: 0,
      minSpeed: Infinity,
      maxSpeed: -Infinity,
      lastMessage: null
    });
  }
}

function updateSensorStats(sensorId, speed) {
  initSensorStats(sensorId);
  const stats = sensorStats.get(sensorId);
  
  stats.count++;
  stats.avgSpeed = (stats.avgSpeed * (stats.count - 1) + speed) / stats.count;
  stats.minSpeed = Math.min(stats.minSpeed, speed);
  stats.maxSpeed = Math.max(stats.maxSpeed, speed);
  stats.lastMessage = new Date();
}

function printStats() {
  console.log('\n📊 Sensor Statistics:');
  console.log('─'.repeat(60));
  
  for (const [sensorId, stats] of sensorStats) {
    console.log(`Sensor ${sensorId}:`);
    console.log(`  Messages: ${stats.count}`);
    console.log(`  Avg Speed: ${stats.avgSpeed.toFixed(2)} km/h`);
    console.log(`  Min Speed: ${stats.minSpeed !== Infinity ? stats.minSpeed : 'N/A'} km/h`);
    console.log(`  Max Speed: ${stats.maxSpeed !== -Infinity ? stats.maxSpeed : 'N/A'} km/h`);
    console.log(`  Last Update: ${stats.lastMessage?.toLocaleTimeString() || 'N/A'}`);
  }
  
  console.log('─'.repeat(60));
  console.log(`Total Telemetry: ${telemetryCount} | Total Alerts: ${alertCount}\n`);
}

client.on('connect', () => {
  console.log('\n✅ Connected to broker!\n');

  // Subscribe to telemetry with wildcard
  client.subscribe('highway/+/telemetry', QoS.AT_LEAST_ONCE, () => {
    console.log('[MONITOR] Subscribed to highway/+/telemetry');
  });

  // Subscribe to alerts
  client.subscribe('highway/+/alerts', QoS.AT_LEAST_ONCE, () => {
    console.log('[MONITOR] Subscribed to highway/+/alerts');
  });

  // Print stats every 10 seconds
  setInterval(printStats, 10000);
});

client.on('message', (msg) => {
  if (msg.topic.includes('/telemetry')) {
    telemetryCount++;
    
    try {
      const data = JSON.parse(msg.data.toString('utf8'));
      const sensorId = data.sensorId;
      const speed = data.speed || 0;
      
      updateSensorStats(sensorId, speed);
      
      // Alert on slow traffic
      if (speed < 10) {
        console.log(`⚠️  SLOW TRAFFIC: Sensor ${sensorId} speed = ${speed} km/h`);
      }
    } catch (err) {
      console.error(`Failed to parse telemetry: ${err.message}`);
    }
  }
  else if (msg.topic.includes('/alerts')) {
    alertCount++;
    console.log(`\n🚨 ALERT from ${msg.topic}:`);
    console.log(`   ${msg.data.toString('utf8')}\n`);
  }
});

client.on('error', (err) => {
  console.error(`❌ Error: ${err.message}`);
});

client.on('close', () => {
  console.log('\n❌ Disconnected from broker');
  printStats();
  process.exit(0);
});

process.on('SIGINT', () => {
  console.log('\n\n[MONITOR] Shutting down...');
  client.disconnect(() => {
    console.log('[MONITOR] Disconnected');
    process.exit(0);
  });
});

console.log('🚀 Traffic Monitor started');
console.log('   Watching: highway/+/telemetry');
console.log('   Watching: highway/+/alerts');
console.log('   Press Ctrl+C to exit\n');
