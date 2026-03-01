#!/usr/bin/env node

/**
 * Producer Example
 * 
 * Publishes messages to broker topics
 */

const { HighwayClient, QoS } = require('../../highway-client.js');


const isDebug = true;
const client = new HighwayClient({
  host: 'localhost',
  port: 1883,
  clientId: 'js-producer-' + Math.random().toString(36).substr(2, 9),
  autoConnect: true
});

let messageCount = 0;
const MAX_SENSORS_COUNT = 10;

client.on('connect', () => {
  console.log('\nConnected to broker!\n');

  // Start publishing messages
  
  const sensorIds = [];

  console.log(`Init ${MAX_SENSORS_COUNT} sensors ids ...`);
  for(let i=0;i<MAX_SENSORS_COUNT;i++)
    sensorIds.push(1000 + i);
  console.log(`${MAX_SENSORS_COUNT} sensors ids is ready to use`);
  
  const publishMessage = () => {
    const sensorId = sensorIds[Math.floor(Math.random() * sensorIds.length)];
    const speed = Math.floor(Math.random() * 100);
    const topic = `highway/${sensorId}/telemetry`;
    
    const message = JSON.stringify({
      timestamp: new Date().toISOString(),
      sensorId,
      speed,
      vehicles: Math.floor(Math.random() * 50),
      temperature: 20 + Math.random() * 10,
      cordinate: {
        x: Math.floor(Math.random()* 1000),
        y: Math.floor(Math.random()* 1000),
        z: Math.floor(Math.random()* 50),
      },
      roadId: sensorId,
    });

    client.publish(topic, message, QoS.AT_LEAST_ONCE, (success) => {
      if (success) {
        messageCount++;
        console.log(`[PUBLISH #${messageCount}] Topic: ${topic}`);
        console.log(`  Data: ${message}\n`);
      }
    });
  };

  // Publish initial message
  publishMessage();

  // Continue publishing every 2 seconds
  const interval = setInterval(() => {
    publishMessage();
  }, 10);

  // Stop after 60 seconds

  !isDebug && setTimeout(() => {
    console.log(`\n\n[PRODUCER] Published ${messageCount} messages`);
    clearInterval(interval);
    client.disconnect();
  }, 60000);
});

client.on('error', (err) => {
  console.error(`❌ Error: ${err.message}`);
});

client.on('close', () => {
  console.log('❌ Disconnected from broker');
  process.exit(0);
});

process.on('SIGINT', () => {
  console.log('\n\n[PRODUCER] Shutting down...');
  console.log(`[PRODUCER] Published ${messageCount} messages total`);
  client.disconnect(() => {
    console.log('[PRODUCER] Disconnected');
    process.exit(0);
  });
});

console.log('🚀 Producer started, publishing messages...');
console.log('   Press Ctrl+C to exit\n');
