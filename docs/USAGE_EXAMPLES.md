# Usage Examples - RabbitBroker

Comprehensive code examples for using RabbitBroker in different scenarios.

## Table of Contents

- [C++ Examples](#c-examples)
- [JavaScript Examples](#javascript-examples)
- [Python Examples](#python-examples)
- [Common Patterns](#common-patterns)
- [Error Handling](#error-handling)
- [Performance Tips](#performance-tips)

## C++ Examples

### Example 1: Basic Publisher

Publish sensor telemetry data:

```cpp
#include "client/client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace rabbit;

int main() {
    // Configure client
    Client::Config config;
    config.host = "127.0.0.1";
    config.port = 1883;
    config.client_id = "sensor_001";
    config.keepalive = 60;
    
    // Create client
    auto client = std::make_shared<Client>(config);
    
    // Set connection handler
    client->set_connect_handler([](bool success) {
        if (success) {
            std::cout << "[INFO] Connected to broker\n";
        } else {
            std::cout << "[ERROR] Failed to connect\n";
        }
    });
    
    // Wait for connection
    client->connect();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Publish sensor data every second
    for (int i = 0; i < 100; ++i) {
        // Create sensor data (JSON format)
        std::string data = R"({
            "sensor_id": "sensor_001",
            "value": )" + std::to_string(20 + (i % 10)) + R"(,
            "unit": "celsius",
            "timestamp": )" + std::to_string(std::time(nullptr)) + R"(
        })";
        
        // Publish with AtLeastOnce QoS
        client->publish("sensors/temperature/indoor", data, QoS::AtLeastOnce);
        
        std::cout << "[PUBLISH] Message " << i << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    client->disconnect();
    return 0;
}
```

**Compile & Run**:
```bash
g++ -std=c++17 -I../include -L../build/lib \
    publisher.cpp -o publisher \
    -lrabbit_client -lboost_system -lpthread
./publisher
```

### Example 2: Basic Subscriber

Receive and process messages:

```cpp
#include "client/client.hpp"
#include <iostream>
#include <atomic>

using namespace rabbit;

std::atomic<int> message_count(0);

int main() {
    // Configure client
    Client::Config config;
    config.host = "127.0.0.1";
    config.port = 1883;
    config.client_id = "monitor_001";
    config.keepalive = 60;
    
    // Create client
    auto client = std::make_shared<Client>(config);
    
    // Set connection handler
    client->set_connect_handler([client](bool success) {
        if (success) {
            std::cout << "[INFO] Connected to broker\n";
            
            // Subscribe to topic with wildcard
            // '+' matches one level: sensors/+/indoor matches sensors/temperature/indoor
            client->subscribe("sensors/+/indoor", QoS::AtMostOnce);
            std::cout << "[SUBSCRIBE] Subscribed to sensors/+/indoor\n";
        }
    });
    
    // Set message handler
    client->set_message_handler([](const std::string& topic,
                                   const std::vector<uint8_t>& payload) {
        // Convert payload to string
        std::string msg(payload.begin(), payload.end());
        
        std::cout << "[MESSAGE] Topic: " << topic << "\n";
        std::cout << "[MESSAGE] Payload: " << msg << "\n";
        
        message_count++;
    });
    
    // Set error handler
    client->set_error_handler([](const std::string& error) {
        std::cerr << "[ERROR] " << error << "\n";
    });
    
    // Connect and run
    client->connect();
    client->run();  // Blocks and processes messages
    
    return 0;
}
```

**Compile & Run**:
```bash
g++ -std=c++17 -I../include -L../build/lib \
    subscriber.cpp -o subscriber \
    -lrabbit_client -lboost_system -lpthread
./subscriber
```

### Example 3: Request-Response Pattern

Publisher sends request, another client responds:

```cpp
#include "client/client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace rabbit;

// Responder: listens for requests and responds
void responder() {
    auto client = std::make_shared<Client>(Client::Config{
        .host = "127.0.0.1",
        .port = 1883,
        .client_id = "responder_001"
    });
    
    client->set_connect_handler([client](bool success) {
        if (success) {
            client->subscribe("requests/get_status", QoS::AtLeastOnce);
        }
    });
    
    client->set_message_handler([client](const std::string& topic,
                                         const std::vector<uint8_t>& payload) {
        std::string request(payload.begin(), payload.end());
        std::cout << "[RESPONDER] Received: " << request << "\n";
        
        // Send response
        std::string response = "{ \"status\": \"ok\", \"uptime\": 3600 }";
        client->publish("responses/status", response, QoS::AtLeastOnce);
        std::cout << "[RESPONDER] Sent response\n";
    });
    
    client->connect();
    client->run();
}

// Requester: sends request and waits for response
void requester() {
    auto client = std::make_shared<Client>(Client::Config{
        .host = "127.0.0.1",
        .port = 1883,
        .client_id = "requester_001"
    });
    
    client->set_connect_handler([client](bool success) {
        if (success) {
            // Subscribe to responses
            client->subscribe("responses/status", QoS::AtLeastOnce);
            
            // Send request
            client->publish("requests/get_status", "{ \"request\": \"status\" }",
                          QoS::AtLeastOnce);
            std::cout << "[REQUESTER] Sent request\n";
        }
    });
    
    client->set_message_handler([client](const std::string& topic,
                                         const std::vector<uint8_t>& payload) {
        std::string response(payload.begin(), payload.end());
        std::cout << "[REQUESTER] Received response: " << response << "\n";
    });
    
    client->connect();
    client->run();
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "responder") {
        responder();
    } else {
        requester();
    }
    return 0;
}
```

### Example 4: Wildcard Subscriptions

Subscribe to multiple topics using patterns:

```cpp
#include "client/client.hpp"

using namespace rabbit;

int main() {
    auto client = std::make_shared<Client>(Client::Config{
        .host = "127.0.0.1",
        .port = 1883,
        .client_id = "wildcard_subscriber"
    });
    
    client->set_connect_handler([client](bool success) {
        if (success) {
            std::cout << "[SUBSCRIBE] Connected\n";
            
            // Single-level wildcard (+): matches exactly one level
            // Matches: sensors/temperature/room1
            //          sensors/humidity/room1
            // Does NOT match: sensors/room1 (missing level)
            client->subscribe("sensors/+/room1", QoS::AtMostOnce);
            
            // Multi-level wildcard (#): matches zero or more levels
            // Matches anything under sensors/
            client->subscribe("sensors/#", QoS::AtMostOnce);
            
            // Combination
            client->subscribe("building/floor_+/zone_#", QoS::AtMostOnce);
        }
    });
    
    client->set_message_handler([](const std::string& topic,
                                   const std::vector<uint8_t>& payload) {
        std::string msg(payload.begin(), payload.end());
        std::cout << "[" << topic << "] " << msg << "\n";
    });
    
    client->connect();
    client->run();
    
    return 0;
}
```

### Example 5: Binary Data Publishing

Publish structured binary data:

```cpp
#include "client/client.hpp"
#include <vector>
#include <cstring>

using namespace rabbit;

// Struct for sensor data
struct SensorReading {
    uint32_t sensor_id;
    float temperature;
    float humidity;
    uint64_t timestamp;
};

// Serialize struct to binary
std::vector<uint8_t> serialize_reading(const SensorReading& reading) {
    std::vector<uint8_t> data(sizeof(SensorReading));
    std::memcpy(data.data(), &reading, sizeof(SensorReading));
    return data;
}

int main() {
    auto client = std::make_shared<Client>(Client::Config{
        .host = "127.0.0.1",
        .port = 1883,
        .client_id = "binary_publisher"
    });
    
    client->set_connect_handler([client](bool success) {
        if (success) {
            // Create and publish sensor reading
            SensorReading reading{
                .sensor_id = 42,
                .temperature = 23.5f,
                .humidity = 65.0f,
                .timestamp = std::time(nullptr)
            };
            
            auto data = serialize_reading(reading);
            client->publish("sensors/binary/room1", data, QoS::AtLeastOnce);
            std::cout << "[BINARY] Published structured data\n";
        }
    });
    
    client->connect();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    return 0;
}
```

## JavaScript Examples

### Example 1: Node.js Publisher

```javascript
const { Client } = require('./rabbit-client');

const client = new Client({
    host: '127.0.0.1',
    port: 1883,
    clientId: 'js_sensor_001'
});

// Connection handler
client.on('connect', () => {
    console.log('[CONNECT] Connected to broker');
    
    // Publish temperature reading
    const reading = {
        sensor_id: 'js_001',
        temperature: 22.5,
        humidity: 60,
        timestamp: Date.now()
    };
    
    client.publish('sensors/temperature', JSON.stringify(reading), {
        qos: 1,
        retain: false
    });
    
    console.log('[PUBLISH] Sent sensor reading');
});

// Error handler
client.on('error', (error) => {
    console.error('[ERROR]', error);
});

// Connect to broker
client.connect();

// Disconnect after 5 seconds
setTimeout(() => {
    client.disconnect();
    console.log('[DISCONNECT] Disconnected');
}, 5000);
```

### Example 2: Node.js Subscriber

```javascript
const { Client } = require('./rabbit-client');

const client = new Client({
    host: '127.0.0.1',
    port: 1883,
    clientId: 'js_monitor_001'
});

// Connection handler
client.on('connect', () => {
    console.log('[CONNECT] Connected to broker');
    
    // Subscribe to topics
    client.subscribe('sensors/+', { qos: 0 });
    client.subscribe('sensors/#', { qos: 1 });
    
    console.log('[SUBSCRIBE] Subscribed to sensor topics');
});

// Message handler
client.on('message', (topic, payload) => {
    try {
        const message = JSON.parse(payload.toString());
        console.log(`[${topic}]`, message);
    } catch (e) {
        console.log(`[${topic}] Binary data:`, payload);
    }
});

// Error handler
client.on('error', (error) => {
    console.error('[ERROR]', error);
});

client.connect();
```

### Example 3: Browser WebSocket Client

```html
<!DOCTYPE html>
<html>
<head>
    <title>RabbitBroker Dashboard</title>
    <script src="rabbit-client.js"></script>
</head>
<body>
    <h1>Live Sensor Dashboard</h1>
    <div id="messages"></div>
    
    <script>
        const client = new Client({
            host: 'localhost',
            port: 1883,
            clientId: 'browser_client'
        });
        
        const messagesDiv = document.getElementById('messages');
        
        client.on('connect', () => {
            console.log('Connected to broker');
            client.subscribe('sensors/#', { qos: 0 });
        });
        
        client.on('message', (topic, payload) => {
            try {
                const data = JSON.parse(payload.toString());
                const div = document.createElement('div');
                div.innerHTML = `<b>${topic}</b>: ${JSON.stringify(data)}`;
                messagesDiv.appendChild(div);
                
                // Keep only last 20 messages
                if (messagesDiv.children.length > 20) {
                    messagesDiv.removeChild(messagesDiv.firstChild);
                }
            } catch (e) {
                console.error('Parse error:', e);
            }
        });
        
        client.connect();
    </script>
</body>
</html>
```

## Python Examples

### Example 1: Python Publisher

```python
import json
import time
import random
from rabbit_client import Client

# Configuration
client = Client(
    host='127.0.0.1',
    port=1883,
    client_id='py_sensor_001'
)

def on_connect():
    """Called when client connects"""
    print("[CONNECT] Connected to broker")
    
    # Publish sensor readings
    for i in range(10):
        reading = {
            'sensor_id': 'py_001',
            'temperature': 20 + random.uniform(-5, 5),
            'humidity': 50 + random.uniform(-10, 10),
            'timestamp': int(time.time() * 1000)
        }
        
        payload = json.dumps(reading)
        client.publish('sensors/temperature', payload, qos=1)
        print(f"[PUBLISH] Message {i+1}")
        time.sleep(1)
    
    client.disconnect()

def on_error(error):
    """Called on error"""
    print(f"[ERROR] {error}")

# Set callbacks
client.on_connect = on_connect
client.on_error = on_error

# Connect to broker
client.connect()

# Run event loop
client.loop_forever()
```

### Example 2: Python Subscriber

```python
import json
from rabbit_client import Client

client = Client(
    host='127.0.0.1',
    port=1883,
    client_id='py_monitor_001'
)

def on_connect():
    print("[CONNECT] Connected to broker")
    client.subscribe('sensors/#', qos=0)
    print("[SUBSCRIBE] Subscribed to sensors/#")

def on_message(topic, payload):
    try:
        data = json.loads(payload)
        print(f"[{topic}] {data}")
    except json.JSONDecodeError:
        print(f"[{topic}] Binary: {len(payload)} bytes")

def on_error(error):
    print(f"[ERROR] {error}")

# Set callbacks
client.on_connect = on_connect
client.on_message = on_message
client.on_error = on_error

# Connect
client.connect()

# Run event loop
try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\n[DISCONNECT] User interrupted")
    client.disconnect()
```

### Example 3: Data Processing Pipeline

```python
import json
import threading
import time
from collections import defaultdict
from rabbit_client import Client

class SensorAnalyzer:
    def __init__(self):
        self.client = Client(
            host='127.0.0.1',
            port=1883,
            client_id='analyzer_001'
        )
        
        # Stats
        self.readings = defaultdict(list)
        self.lock = threading.Lock()
        
        # Set callbacks
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_error = self.on_error
    
    def on_connect(self):
        print("[ANALYZER] Connected")
        self.client.subscribe('sensors/temperature', qos=1)
        
        # Start analysis thread
        threading.Thread(target=self.analyze, daemon=True).start()
    
    def on_message(self, topic, payload):
        try:
            data = json.loads(payload)
            sensor_id = data.get('sensor_id')
            temp = data.get('temperature')
            
            with self.lock:
                self.readings[sensor_id].append(temp)
                # Keep only last 100 readings
                if len(self.readings[sensor_id]) > 100:
                    self.readings[sensor_id].pop(0)
            
            print(f"[RECEIVED] {sensor_id}: {temp}°C")
        except Exception as e:
            print(f"[ERROR] Parse error: {e}")
    
    def on_error(self, error):
        print(f"[ERROR] {error}")
    
    def analyze(self):
        """Analyze readings every 10 seconds"""
        while True:
            time.sleep(10)
            
            with self.lock:
                for sensor_id, temps in self.readings.items():
                    if temps:
                        avg = sum(temps) / len(temps)
                        min_t = min(temps)
                        max_t = max(temps)
                        print(f"[STATS] {sensor_id}: avg={avg:.1f}°C, "
                              f"min={min_t:.1f}°C, max={max_t:.1f}°C")
    
    def run(self):
        self.client.connect()
        self.client.loop_forever()

# Run analyzer
analyzer = SensorAnalyzer()
analyzer.run()
```

## Common Patterns

### Pattern 1: Fan-Out (One Publisher, Many Subscribers)

Publisher sends to single topic, multiple subscribers receive:

```cpp
// Publisher
client->publish("news/breaking", "New article available", QoS::AtMostOnce);

// Subscribers (multiple clients)
client1->subscribe("news/breaking", QoS::AtMostOnce);
client2->subscribe("news/breaking", QoS::AtMostOnce);
client3->subscribe("news/breaking", QoS::AtMostOnce);
```

### Pattern 2: Publish-Subscribe with Hierarchies

Organize topics by hierarchy:

```cpp
// Temperature sensor publishes to
client->publish("building/floor1/room1/temperature", "22.5");

// Subscriber can listen at different levels:
client->subscribe("building/floor1/room1/#");  // All sensors in room1
client->subscribe("building/floor1/+/temperature");  // Temps in all rooms
client->subscribe("building/#");  // Everything in building
```

### Pattern 3: Command Channel

One topic for commands, one for responses:

```cpp
// Commander publishes
client->publish("commands/pump_1/control", "ON");

// Device listens and responds
client->subscribe("commands/pump_1/control");
// ... in message handler ...
client->publish("responses/pump_1/status", "STARTED");
```

## Error Handling

### Connection Errors

```cpp
client->set_error_handler([client](const std::string& error) {
    if (error.find("connection refused") != std::string::npos) {
        std::cout << "Broker not running. Reconnecting...\n";
        // Retry logic
        std::this_thread::sleep_for(std::chrono::seconds(5));
        client->connect();
    } else {
        std::cerr << "Critical error: " << error << "\n";
    }
});
```

### Message Processing Errors

```cpp
client->set_message_handler([](const std::string& topic,
                               const std::vector<uint8_t>& payload) {
    try {
        std::string msg(payload.begin(), payload.end());
        // Process message
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << "\n";
    }
});
```

## Performance Tips

### 1. Use Appropriate QoS

```cpp
// Use QoS 0 for high-volume, non-critical data
client->publish("metrics/cpu", "45%", QoS::AtMostOnce);

// Use QoS 1 for important data
client->publish("alerts/temperature", "ALERT", QoS::AtLeastOnce);
```

### 2. Batch Publishing

```cpp
// Instead of:
for (int i = 0; i < 1000; ++i) {
    client->publish("topic", std::to_string(i));
}

// Do:
for (int i = 0; i < 1000; ++i) {
    // Accumulate in buffer
    batch.push_back(i);
    
    // Send when batch reaches 100
    if (batch.size() >= 100) {
        for (auto& val : batch) {
            client->publish("topic", std::to_string(val));
        }
        batch.clear();
    }
}
```

### 3. Efficient Topic Naming

```cpp
// Good: Short, hierarchical
"sensors/temp/room1"
"sensors/humidity/room1"

// Bad: Long, unstructured
"the_temperature_reading_from_room_number_1"
"humidity reading room 1"
```

### 4. Use Wildcards Carefully

```cpp
// Efficient: Specific patterns
client->subscribe("sensors/temp/+", QoS::AtMostOnce);

// Less efficient: Very broad patterns
client->subscribe("#", QoS::AtMostOnce);  // Receives everything!
```

---

For more examples and documentation, see:
- [README.md](README.md) - Project overview
- [QUICKSTART.md](QUICKSTART.md) - Get started quickly
- [ARCHITECTURE.md](docs/ARCHITECTURE.md) - System design
