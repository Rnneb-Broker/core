# Highway Message Broker - Architecture Documentation

## Table of Contents
1. [System Overview](#system-overview)
2. [Business Use Case](#business-use-case)
3. [Architecture Diagrams](#architecture-diagrams)
4. [Class Responsibilities](#class-responsibilities)
5. [Data Flow Pipeline](#data-flow-pipeline)
6. [Protocol Specification](#protocol-specification)
7. [Topic System](#topic-system)
8. [Sequence Diagrams](#sequence-diagrams)

---

## 1. System Overview

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           HIGHWAY TELEMETRY SYSTEM                              │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                         PUBLISHERS (Sensors)                            │   │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐       │   │
│  │  │Sensor 1 │  │Sensor 2 │  │Sensor 3 │  │Sensor N │  │   ...   │       │   │
│  │  │ ID: 1   │  │ ID: 2   │  │ ID: 3   │  │ ID: N   │  │         │       │   │
│  │  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘       │   │
│  │       │            │            │            │            │             │   │
│  │       └────────────┴─────┬──────┴────────────┴────────────┘             │   │
│  └──────────────────────────┼──────────────────────────────────────────────┘   │
│                             │                                                   │
│                             │ PUBLISH: highway/{sensor_id}/telemetry            │
│                             │ Data: {car_id, sensor_id, timestamp, speed}       │
│                             ▼                                                   │
│  ┌──────────────────────────────────────────────────────────────────────────┐  │
│  │                          BROKER SERVER                                    │  │
│  │  ┌──────────────────────────────────────────────────────────────────┐    │  │
│  │  │                      TCP ACCEPTOR (Port 1883)                    │    │  │
│  │  └──────────────────────────────────┬───────────────────────────────┘    │  │
│  │                                     │                                     │  │
│  │  ┌──────────────────────────────────▼───────────────────────────────┐    │  │
│  │  │                      SESSION MANAGER                              │    │  │
│  │  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐              │    │  │
│  │  │  │Session 1│  │Session 2│  │Session 3│  │Session N│              │    │  │
│  │  │  └─────────┘  └─────────┘  └─────────┘  └─────────┘              │    │  │
│  │  └──────────────────────────────────┬───────────────────────────────┘    │  │
│  │                                     │                                     │  │
│  │  ┌──────────────────────────────────▼───────────────────────────────┐    │  │
│  │  │                      MESSAGE ROUTER                               │    │  │
│  │  │  ┌─────────────────┐        ┌──────────────────────┐             │    │  │
│  │  │  │  TopicManager   │◄──────►│ SubscriptionManager  │             │    │  │
│  │  │  │                 │        │                      │             │    │  │
│  │  │  │ - Topic Tree    │        │ - Pattern Matching   │             │    │  │
│  │  │  │ - Wildcards     │        │ - Session Tracking   │             │    │  │
│  │  │  └─────────────────┘        └──────────────────────┘             │    │  │
│  │  └──────────────────────────────────┬───────────────────────────────┘    │  │
│  └─────────────────────────────────────┼────────────────────────────────────┘  │
│                                        │                                        │
│                                        │ DELIVER: highway/+/telemetry           │
│                                        ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                         SUBSCRIBERS (Consumers)                         │   │
│  │  ┌───────────────────┐  ┌───────────────────┐  ┌───────────────────┐   │   │
│  │  │  Traffic Monitor  │  │     Dashboard     │  │   Alert Service   │   │   │
│  │  │                   │  │                   │  │                   │   │   │
│  │  │ Subscribes to:    │  │ Subscribes to:    │  │ Subscribes to:    │   │   │
│  │  │ highway/+/telemetry│ │ highway/#         │  │ highway/+/alerts  │   │   │
│  │  └───────────────────┘  └───────────────────┘  └───────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Business Use Case

### Highway Traffic Monitoring System

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              HIGHWAY A1 (Physical)                              │
│                                                                                 │
│     KM 0          KM 10         KM 20         KM 30         KM 40              │
│       │             │             │             │             │                 │
│   ════╬═════════════╬═════════════╬═════════════╬═════════════╬════►           │
│       │             │             │             │             │                 │
│    [S-001]       [S-002]       [S-003]       [S-004]       [S-005]             │
│    Sensor        Sensor        Sensor        Sensor        Sensor              │
│                                                                                 │
│  Each sensor detects:                                                          │
│  • Car ID (license plate reader)                                               │
│  • Speed (radar)                                                               │
│  • Timestamp                                                                   │
│  • GPS coordinates                                                             │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Data Flow for Traffic Alert Detection

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                         TRAFFIC ALERT DETECTION FLOW                           │
│                                                                                │
│  ┌─────────┐                                                                   │
│  │ Sensor  │──── Detects car at 3 km/h ────┐                                  │
│  │  S-003  │                                │                                  │
│  └─────────┘                                ▼                                  │
│                                    ┌─────────────────┐                         │
│                                    │  PUBLISH EVENT  │                         │
│                                    │                 │                         │
│                                    │  Topic: highway/3/telemetry               │
│                                    │  Data:                                    │
│                                    │    car_id: 12345                          │
│                                    │    speed: 3.0 km/h                        │
│                                    │    timestamp: 1706990400000               │
│                                    └────────┬────────┘                         │
│                                             │                                  │
│                                             ▼                                  │
│                                    ┌─────────────────┐                         │
│                                    │     BROKER      │                         │
│                                    │  Routes message │                         │
│                                    └────────┬────────┘                         │
│                                             │                                  │
│                                             ▼                                  │
│                                    ┌─────────────────┐                         │
│                                    │ Traffic Monitor │                         │
│                                    │                 │                         │
│                                    │ Subscribes to:  │                         │
│                                    │ highway/+/telemetry                       │
│                                    └────────┬────────┘                         │
│                                             │                                  │
│                              ┌──────────────┴──────────────┐                   │
│                              ▼                              ▼                  │
│                    ┌─────────────────┐            ┌─────────────────┐          │
│                    │  Speed < 5 km/h │            │ Duration > 3min │          │
│                    │       YES       │            │      YES        │          │
│                    └────────┬────────┘            └────────┬────────┘          │
│                             │                              │                   │
│                             └──────────────┬───────────────┘                   │
│                                            ▼                                   │
│                                   ┌────────────────┐                           │
│                                   │  TRAFFIC ALERT │                           │
│                                   │                │                           │
│                                   │  🚨 WARNING!   │                           │
│                                   │  Possible      │                           │
│                                   │  blockage at   │                           │
│                                   │  Sensor S-003  │                           │
│                                   └────────────────┘                           │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Architecture Diagrams

### Layer Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              APPLICATION LAYER                                  │
│  ┌─────────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐     │
│  │  sensor_simulator   │  │  traffic_monitor    │  │   Other Apps...     │     │
│  │  (Publisher)        │  │  (Subscriber)       │  │                     │     │
│  └──────────┬──────────┘  └──────────┬──────────┘  └──────────┬──────────┘     │
└─────────────┼─────────────────────────┼─────────────────────────┼───────────────┘
              │                         │                         │
              ▼                         ▼                         ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              CLIENT LIBRARY LAYER                               │
│  ┌──────────────────────────────────────────────────────────────────────────┐  │
│  │                           highway::Client                                 │  │
│  │                                                                           │  │
│  │  • connect()      - Establish connection to broker                       │  │
│  │  • publish()      - Send message to topic                                │  │
│  │  • subscribe()    - Register interest in topic pattern                   │  │
│  │  • disconnect()   - Close connection                                     │  │
│  └──────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        │ TCP/IP
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              PROTOCOL LAYER                                     │
│  ┌──────────────────────────────────────────────────────────────────────────┐  │
│  │                          MQTT-lite Protocol                               │  │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐        │  │
│  │  │   CONNECT   │ │   PUBLISH   │ │  SUBSCRIBE  │ │ DISCONNECT  │        │  │
│  │  │   CONNACK   │ │   PUBACK    │ │   SUBACK    │ │   PINGREQ   │        │  │
│  │  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘        │  │
│  └──────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              BROKER CORE LAYER                                  │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                           highway::Broker                                │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │   │
│  │  │   Session   │  │   Topic     │  │Subscription │  │    I/O      │    │   │
│  │  │   Manager   │  │   Manager   │  │   Manager   │  │   Threads   │    │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘    │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              NETWORK LAYER                                      │
│  ┌──────────────────────────────────────────────────────────────────────────┐  │
│  │                          Boost.Asio                                       │  │
│  │  • Async TCP acceptor                                                     │  │
│  │  • Async read/write operations                                            │  │
│  │  • Multi-threaded io_context                                              │  │
│  └──────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Component Interaction Diagram

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                        BROKER INTERNAL ARCHITECTURE                            │
│                                                                                │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │                              Broker                                      │  │
│  │                         (Main Controller)                                │  │
│  │                                                                          │  │
│  │   Responsibilities:                                                      │  │
│  │   • Accept TCP connections on port 1883                                 │  │
│  │   • Create Session for each client                                      │  │
│  │   • Manage IO thread pool                                               │  │
│  │   • Coordinate message routing                                          │  │
│  │   • Track statistics                                                    │  │
│  └───────────────────────────────────┬─────────────────────────────────────┘  │
│                                      │                                         │
│         ┌────────────────────────────┼────────────────────────────┐           │
│         │                            │                            │           │
│         ▼                            ▼                            ▼           │
│  ┌─────────────────┐  ┌─────────────────────────┐  ┌─────────────────────┐   │
│  │    Session      │  │    TopicManager         │  │SubscriptionManager  │   │
│  │  (Per Client)   │  │                         │  │                     │   │
│  │                 │  │  • Topic validation     │  │  • Subscribe/       │   │
│  │  • Protocol     │  │  • Wildcard matching    │  │    Unsubscribe      │   │
│  │    parsing      │  │  • Topic registration   │  │  • Pattern matching │   │
│  │  • State        │  │                         │  │  • Session cleanup  │   │
│  │    machine      │  │  Topics stored:         │  │                     │   │
│  │  • Send queue   │  │  highway/1/telemetry    │  │  Subscriptions:     │   │
│  │                 │  │  highway/2/telemetry    │  │  highway/+/telemetry│   │
│  │  States:        │  │  highway/3/alerts       │  │  highway/#          │   │
│  │  • Connected    │  │  ...                    │  │  ...                │   │
│  │  • Authenticated│  │                         │  │                     │   │
│  │  • Disconnecting│  └─────────────────────────┘  └─────────────────────┘   │
│  │  • Disconnected │                                                          │
│  └─────────────────┘                                                          │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Class Responsibilities

### 4.1 Protocol Layer Classes

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              PROTOCOL CLASSES                                   │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  PacketType (enum)                                                        │ │
│  │  ─────────────────                                                        │ │
│  │  Defines all MQTT-lite packet types:                                      │ │
│  │                                                                           │ │
│  │  CONNECT (0x10)    ──►  Client → Broker: "Hello, I'm sensor_1"           │ │
│  │  CONNACK (0x20)    ◄──  Broker → Client: "Welcome, connected!"           │ │
│  │  PUBLISH (0x30)    ──►  Client → Broker: "Here's sensor data"            │ │
│  │  PUBACK (0x40)     ◄──  Broker → Client: "Got it" (QoS 1)                │ │
│  │  SUBSCRIBE (0x80)  ──►  Client → Broker: "I want highway/+/telemetry"    │ │
│  │  SUBACK (0x90)     ◄──  Broker → Client: "Subscribed!"                   │ │
│  │  PINGREQ (0xC0)    ──►  Client → Broker: "Are you alive?"                │ │
│  │  PINGRESP (0xD0)   ◄──  Broker → Client: "Yes!"                          │ │
│  │  DISCONNECT (0xE0) ──►  Client → Broker: "Goodbye"                       │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  PacketHeader (struct) - 4 bytes, packed                                  │ │
│  │  ─────────────────────                                                    │ │
│  │                                                                           │ │
│  │  ┌─────────┬─────────┬──────────────────┐                                │ │
│  │  │  type   │  flags  │  remaining_len   │                                │ │
│  │  │ 1 byte  │ 1 byte  │     2 bytes      │                                │ │
│  │  └─────────┴─────────┴──────────────────┘                                │ │
│  │                                                                           │ │
│  │  • type: PacketType enum value                                           │ │
│  │  • flags: QoS level, retain flag, duplicate flag                         │ │
│  │  • remaining_len: Size of payload that follows                           │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  QoS (enum) - Quality of Service                                          │ │
│  │  ──────────                                                               │ │
│  │                                                                           │ │
│  │  AtMostOnce (0)   ───►  Fire and forget, no ACK                          │ │
│  │                         Best performance, may lose messages               │ │
│  │                                                                           │ │
│  │  AtLeastOnce (1)  ───►  Acknowledged delivery                            │ │
│  │                         May receive duplicates                            │ │
│  │                                                                           │ │
│  │  ExactlyOnce (2)  ───►  Guaranteed single delivery                       │ │
│  │                         (Not implemented yet)                             │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Broker Class

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           CLASS: Broker                                         │
│                      File: include/broker/broker.hpp                            │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  PURPOSE: Central message broker server - the heart of the system              │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Configuration                                                            │ │
│  │  ─────────────                                                            │ │
│  │  struct Config {                                                          │ │
│  │      port = 1883           // TCP port to listen on                       │ │
│  │      max_connections = 10000   // Max simultaneous clients                │ │
│  │      io_threads = 4        // Worker threads for async I/O                │ │
│  │      buffer_size = 65536   // Socket buffer size                          │ │
│  │  }                                                                        │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Public Methods                                                           │ │
│  │  ──────────────                                                           │ │
│  │                                                                           │ │
│  │  start()        ──►  Start accepting connections                          │ │
│  │                      Launch IO threads                                    │ │
│  │                                                                           │ │
│  │  stop()         ──►  Graceful shutdown                                    │ │
│  │                      Close all sessions                                   │ │
│  │                      Stop IO threads                                      │ │
│  │                                                                           │ │
│  │  get_stats()    ──►  Returns runtime statistics                           │ │
│  │                      (connections, messages, topics)                      │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Internal Callbacks (called by Session)                                   │ │
│  │  ──────────────────────────────────────                                   │ │
│  │                                                                           │ │
│  │  on_publish(topic, payload, qos, from)                                    │ │
│  │      │                                                                    │ │
│  │      ├──► Register topic in TopicManager                                  │ │
│  │      ├──► Find matching subscribers                                       │ │
│  │      └──► Deliver message to each subscriber                              │ │
│  │                                                                           │ │
│  │  on_subscribe(session, topic, qos)                                        │ │
│  │      │                                                                    │ │
│  │      └──► Add subscription to SubscriptionManager                         │ │
│  │                                                                           │ │
│  │  on_session_closed(session)                                               │ │
│  │      │                                                                    │ │
│  │      ├──► Remove all subscriptions for session                            │ │
│  │      └──► Remove session from sessions map                                │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Internal Components                                                      │ │
│  │  ───────────────────                                                      │ │
│  │                                                                           │ │
│  │  io_context_            ──►  Boost.Asio event loop                        │ │
│  │  acceptor_              ──►  TCP listener on port 1883                    │ │
│  │  sessions_              ──►  Map of active client sessions                │ │
│  │  topic_manager_         ──►  Manages topic registry                       │ │
│  │  subscription_manager_  ──►  Manages pub/sub routing                      │ │
│  │  io_threads_            ──►  Worker thread pool                           │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 4.3 Session Class

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           CLASS: Session                                        │
│                      File: include/broker/session.hpp                           │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  PURPOSE: Represents ONE client connection (sensor or consumer)                │
│           Handles protocol parsing and connection lifecycle                    │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  State Machine                                                            │ │
│  │  ─────────────                                                            │ │
│  │                                                                           │ │
│  │        ┌──────────────┐                                                   │ │
│  │        │  Connected   │ ◄── TCP connection established                    │ │
│  │        └──────┬───────┘                                                   │ │
│  │               │ CONNECT packet received                                   │ │
│  │               ▼                                                           │ │
│  │        ┌──────────────┐                                                   │ │
│  │        │Authenticated │ ◄── CONNACK sent, ready for pub/sub              │ │
│  │        └──────┬───────┘                                                   │ │
│  │               │ DISCONNECT or error                                       │ │
│  │               ▼                                                           │ │
│  │        ┌──────────────┐                                                   │ │
│  │        │Disconnecting │ ◄── Graceful shutdown in progress                 │ │
│  │        └──────┬───────┘                                                   │ │
│  │               │                                                           │ │
│  │               ▼                                                           │ │
│  │        ┌──────────────┐                                                   │ │
│  │        │ Disconnected │ ◄── Session ended, cleanup done                   │ │
│  │        └──────────────┘                                                   │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Read Pipeline (Async)                                                    │ │
│  │  ─────────────────────                                                    │ │
│  │                                                                           │ │
│  │  read_header()  ──►  Read 4-byte PacketHeader                            │ │
│  │       │                                                                   │ │
│  │       ▼                                                                   │ │
│  │  read_payload() ──►  Read remaining_len bytes                            │ │
│  │       │                                                                   │ │
│  │       ▼                                                                   │ │
│  │  process_packet() ──►  Dispatch to handler based on type                 │ │
│  │       │                                                                   │ │
│  │       ├─► handle_connect()    → Send CONNACK                             │ │
│  │       ├─► handle_publish()    → Call broker.on_publish()                 │ │
│  │       ├─► handle_subscribe()  → Call broker.on_subscribe()               │ │
│  │       ├─► handle_pingreq()    → Send PINGRESP                            │ │
│  │       └─► handle_disconnect() → Close session                            │ │
│  │                                                                           │ │
│  │       ▼                                                                   │ │
│  │  read_header()  ──►  Loop back to read next packet                       │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Write Queue (Non-blocking)                                               │ │
│  │  ──────────────────────────                                               │ │
│  │                                                                           │ │
│  │  send(packet)                                                             │ │
│  │       │                                                                   │ │
│  │       ▼                                                                   │ │
│  │  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐              │ │
│  │  │   Packet 1   │────►│   Packet 2   │────►│   Packet 3   │              │ │
│  │  └──────────────┘     └──────────────┘     └──────────────┘              │ │
│  │       write_queue_ (std::deque)                                          │ │
│  │                                                                           │ │
│  │  write_next()  ──►  Pop front, async_write, repeat if more               │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 4.4 TopicManager Class

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                        CLASS: TopicManager                                      │
│                   File: include/broker/topic_manager.hpp                        │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  PURPOSE: Manages topic registry and wildcard matching                         │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Topic Hierarchy Example                                                  │ │
│  │  ───────────────────────                                                  │ │
│  │                                                                           │ │
│  │                          highway                                          │ │
│  │                             │                                             │ │
│  │            ┌────────────────┼────────────────┐                           │ │
│  │            │                │                │                           │ │
│  │            1                2                3     (sensor IDs)          │ │
│  │            │                │                │                           │ │
│  │      ┌─────┴─────┐    ┌─────┴─────┐    ┌─────┴─────┐                    │ │
│  │      │           │    │           │    │           │                    │ │
│  │  telemetry   alerts  telemetry  alerts  telemetry  alerts               │ │
│  │                                                                           │ │
│  │  Full topic paths:                                                       │ │
│  │  • highway/1/telemetry                                                   │ │
│  │  • highway/1/alerts                                                      │ │
│  │  • highway/2/telemetry                                                   │ │
│  │  • ...                                                                   │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Wildcard Matching                                                        │ │
│  │  ─────────────────                                                        │ │
│  │                                                                           │ │
│  │  '+' (Single Level Wildcard)                                             │ │
│  │  ────────────────────────────                                            │ │
│  │                                                                           │ │
│  │  Pattern: highway/+/telemetry                                            │ │
│  │                                                                           │ │
│  │  ✓ MATCHES:    highway/1/telemetry                                       │ │
│  │  ✓ MATCHES:    highway/2/telemetry                                       │ │
│  │  ✓ MATCHES:    highway/999/telemetry                                     │ │
│  │  ✗ NO MATCH:   highway/1/alerts                                          │ │
│  │  ✗ NO MATCH:   highway/1/2/telemetry                                     │ │
│  │                                                                           │ │
│  │  '#' (Multi Level Wildcard)                                              │ │
│  │  ──────────────────────────                                              │ │
│  │                                                                           │ │
│  │  Pattern: highway/#                                                       │ │
│  │                                                                           │ │
│  │  ✓ MATCHES:    highway/1/telemetry                                       │ │
│  │  ✓ MATCHES:    highway/1/alerts                                          │ │
│  │  ✓ MATCHES:    highway/2/telemetry                                       │ │
│  │  ✓ MATCHES:    highway/a/b/c/d/e                                         │ │
│  │                                                                           │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Key Methods                                                              │ │
│  │  ───────────                                                              │ │
│  │                                                                           │ │
│  │  matches(pattern, topic)     ──►  Check if pattern matches topic         │ │
│  │  split_topic(topic)          ──►  "a/b/c" → ["a", "b", "c"]             │ │
│  │  is_valid_topic(topic)       ──►  Validate for publishing (no wildcards)│ │
│  │  is_valid_pattern(pattern)   ──►  Validate for subscribing              │ │
│  │  register_topic(topic)       ──►  Add topic to registry                 │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 4.5 SubscriptionManager Class

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                     CLASS: SubscriptionManager                                  │
│                File: include/broker/subscription_manager.hpp                    │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  PURPOSE: Routes messages from publishers to matching subscribers              │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Internal Data Structures                                                 │ │
│  │  ────────────────────────                                                 │ │
│  │                                                                           │ │
│  │  subscriptions_by_pattern_:                                               │ │
│  │  ┌─────────────────────────────────────────────────────────────────┐     │ │
│  │  │  Pattern                    │  Subscribers                      │     │ │
│  │  ├─────────────────────────────┼───────────────────────────────────┤     │ │
│  │  │  "highway/+/telemetry"      │  [Monitor1, Monitor2]             │     │ │
│  │  │  "highway/#"                │  [Dashboard]                      │     │ │
│  │  │  "highway/1/alerts"         │  [AlertService]                   │     │ │
│  │  └─────────────────────────────────────────────────────────────────┘     │ │
│  │                                                                           │ │
│  │  patterns_by_session_:                                                    │ │
│  │  ┌─────────────────────────────────────────────────────────────────┐     │ │
│  │  │  Session                    │  Subscribed Patterns              │     │ │
│  │  ├─────────────────────────────┼───────────────────────────────────┤     │ │
│  │  │  Monitor1                   │  {"highway/+/telemetry"}          │     │ │
│  │  │  Dashboard                  │  {"highway/#"}                    │     │ │
│  │  │  AlertService               │  {"highway/1/alerts"}             │     │ │
│  │  └─────────────────────────────────────────────────────────────────┘     │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Message Routing Flow                                                     │ │
│  │  ────────────────────                                                     │ │
│  │                                                                           │ │
│  │  get_subscribers("highway/1/telemetry")                                  │ │
│  │       │                                                                   │ │
│  │       ▼                                                                   │ │
│  │  ┌────────────────────────────────────────────────────────────┐          │ │
│  │  │  For each pattern in subscriptions_by_pattern_:            │          │ │
│  │  │                                                            │          │ │
│  │  │    "highway/+/telemetry" ──► matches? YES ──► add subs    │          │ │
│  │  │    "highway/#"           ──► matches? YES ──► add subs    │          │ │
│  │  │    "highway/1/alerts"    ──► matches? NO                  │          │ │
│  │  └────────────────────────────────────────────────────────────┘          │ │
│  │       │                                                                   │ │
│  │       ▼                                                                   │ │
│  │  Returns: [Monitor1, Monitor2, Dashboard]                                │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 4.6 Client Class

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           CLASS: Client                                         │
│                      File: include/client/client.hpp                            │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  PURPOSE: Library for applications to connect to the broker                    │
│           Used by both sensors (publishers) and monitors (subscribers)         │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Usage as Publisher (Sensor)                                              │ │
│  │  ───────────────────────────                                              │ │
│  │                                                                           │ │
│  │  Client::Config config;                                                   │ │
│  │  config.host = "127.0.0.1";                                              │ │
│  │  config.port = 1883;                                                      │ │
│  │  config.client_id = "sensor_1";                                          │ │
│  │                                                                           │ │
│  │  auto client = std::make_shared<Client>(config);                         │ │
│  │  client->connect([](bool ok) { ... });                                   │ │
│  │                                                                           │ │
│  │  // Publish sensor data                                                   │ │
│  │  SensorEvent event = SensorEvent::create(car_id, sensor_id, speed);      │ │
│  │  client->publish(event.topic(), event.serialize());                      │ │
│  │                                                                           │ │
│  │  client->run_async();  // Run in background                              │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  Usage as Subscriber (Monitor)                                            │ │
│  │  ─────────────────────────────                                            │ │
│  │                                                                           │ │
│  │  Client::Config config;                                                   │ │
│  │  config.client_id = "traffic_monitor";                                   │ │
│  │                                                                           │ │
│  │  auto client = std::make_shared<Client>(config);                         │ │
│  │                                                                           │ │
│  │  client->set_message_handler([](const std::string& topic,                │ │
│  │                                  const std::vector<uint8_t>& payload) {  │ │
│  │      SensorEvent event = SensorEvent::deserialize(payload);              │ │
│  │      // Process event...                                                  │ │
│  │  });                                                                      │ │
│  │                                                                           │ │
│  │  client->connect([&](bool ok) {                                          │ │
│  │      client->subscribe("highway/+/telemetry");  // Wildcard!             │ │
│  │  });                                                                      │ │
│  │                                                                           │ │
│  │  client->run();  // Blocking - receives messages                         │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 4.7 Data Classes

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           DATA CLASSES                                          │
│                    File: include/data/sensor_event.hpp                          │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  SensorEvent (36 bytes binary)                                            │ │
│  │  ───────────────────────────────                                          │ │
│  │                                                                           │ │
│  │  ┌──────────────────────────────────────────────────────────────────┐    │ │
│  │  │ car_id    │ sensor_id │ timestamp │ speed  │ latitude │longitude│    │ │
│  │  │ 8 bytes   │ 8 bytes   │ 8 bytes   │4 bytes │ 4 bytes  │ 4 bytes │    │ │
│  │  └──────────────────────────────────────────────────────────────────┘    │ │
│  │                                                                           │ │
│  │  Example:                                                                 │ │
│  │  {                                                                        │ │
│  │      car_id: 12345,           // Vehicle identifier                      │ │
│  │      sensor_id: 3,            // Sensor that detected it                 │ │
│  │      timestamp: 1706990400000, // Unix ms                                │ │
│  │      speed_kmh: 85.5,         // Speed in km/h                           │ │
│  │      latitude: 33.5731,       // GPS (optional)                          │ │
│  │      longitude: -7.5898       // GPS (optional)                          │ │
│  │  }                                                                        │ │
│  │                                                                           │ │
│  │  Methods:                                                                 │ │
│  │  • create(car_id, sensor_id, speed) → Auto-timestamp                     │ │
│  │  • serialize() → Binary for network                                      │ │
│  │  • deserialize(bytes) → Reconstruct object                               │ │
│  │  • topic() → "highway/{sensor_id}/telemetry"                             │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐ │
│  │  TrafficAlert                                                             │ │
│  │  ────────────                                                             │ │
│  │                                                                           │ │
│  │  Generated when traffic anomaly detected:                                │ │
│  │                                                                           │ │
│  │  {                                                                        │ │
│  │      type: SlowTraffic,       // Alert type                              │ │
│  │      sensor_id: 3,            // Where detected                          │ │
│  │      timestamp: 1706990400000,                                           │ │
│  │      duration_ms: 185000,     // 3+ minutes of slow traffic              │ │
│  │      avg_speed: 3.2,          // Average during alert                    │ │
│  │      vehicle_count: 47        // Cars affected                           │ │
│  │  }                                                                        │ │
│  │                                                                           │ │
│  │  Published to: highway/{sensor_id}/alerts                                │ │
│  └───────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Data Flow Pipeline

### Complete Message Flow

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    COMPLETE MESSAGE FLOW PIPELINE                               │
│                                                                                 │
│  ╔═══════════════════════════════════════════════════════════════════════════╗ │
│  ║  STEP 1: SENSOR GENERATES EVENT                                           ║ │
│  ╚═══════════════════════════════════════════════════════════════════════════╝ │
│                                                                                 │
│     Highway Sensor                                                              │
│     ┌────────────────────────────────────┐                                     │
│     │  Car detected!                     │                                     │
│     │  • License: ABC-123                │                                     │
│     │  • Speed: 85 km/h                  │                                     │
│     │  • Time: 2026-02-03 15:30:00       │                                     │
│     └─────────────────┬──────────────────┘                                     │
│                       │                                                         │
│                       ▼                                                         │
│     SensorEvent::create(car_id=12345, sensor_id=3, speed=85.0)                │
│                       │                                                         │
│                       ▼                                                         │
│  ╔═══════════════════════════════════════════════════════════════════════════╗ │
│  ║  STEP 2: SERIALIZE TO BINARY                                              ║ │
│  ╚═══════════════════════════════════════════════════════════════════════════╝ │
│                                                                                 │
│     event.serialize()                                                           │
│     ┌─────────────────────────────────────────────────────────────────────┐    │
│     │ 00 00 00 00 00 00 30 39 │ car_id (12345)                           │    │
│     │ 00 00 00 00 00 00 00 03 │ sensor_id (3)                            │    │
│     │ 00 00 01 8D 5A B2 F4 00 │ timestamp                                │    │
│     │ 42 AA 00 00             │ speed (85.0 as float)                    │    │
│     │ 00 00 00 00             │ latitude (0.0)                           │    │
│     │ 00 00 00 00             │ longitude (0.0)                          │    │
│     └─────────────────────────────────────────────────────────────────────┘    │
│                       │                                                         │
│                       ▼                                                         │
│  ╔═══════════════════════════════════════════════════════════════════════════╗ │
│  ║  STEP 3: CREATE PUBLISH PACKET                                            ║ │
│  ╚═══════════════════════════════════════════════════════════════════════════╝ │
│                                                                                 │
│     client->publish("highway/3/telemetry", binary_data)                        │
│                                                                                 │
│     ┌───────────────────────────────────────────────────────────────────┐      │
│     │  PUBLISH Packet                                                    │      │
│     │  ┌──────────────────────────────────────────────────────────────┐ │      │
│     │  │ Header (4 bytes)                                             │ │      │
│     │  │ ┌──────┬───────┬────────────────┐                           │ │      │
│     │  │ │ 0x30 │ 0x00  │    0x003A      │                           │ │      │
│     │  │ │ type │ flags │ remaining_len  │                           │ │      │
│     │  │ └──────┴───────┴────────────────┘                           │ │      │
│     │  ├──────────────────────────────────────────────────────────────┤ │      │
│     │  │ Payload                                                      │ │      │
│     │  │ ┌────────────────────────────────────────────────────────┐  │ │      │
│     │  │ │ Topic: "highway/3/telemetry" (len-prefixed string)     │  │ │      │
│     │  │ │ Packet ID: 0x0000                                      │  │ │      │
│     │  │ │ Data: [36 bytes of SensorEvent]                        │  │ │      │
│     │  │ └────────────────────────────────────────────────────────┘  │ │      │
│     │  └──────────────────────────────────────────────────────────────┘ │      │
│     └───────────────────────────────────────────────────────────────────┘      │
│                       │                                                         │
│                       │ TCP/IP                                                  │
│                       ▼                                                         │
│  ╔═══════════════════════════════════════════════════════════════════════════╗ │
│  ║  STEP 4: BROKER RECEIVES PACKET                                           ║ │
│  ╚═══════════════════════════════════════════════════════════════════════════╝ │
│                                                                                 │
│     Session::read_header() → Session::read_payload() → Session::process_packet()│
│                       │                                                         │
│                       ▼                                                         │
│     Session::handle_publish()                                                   │
│     ┌────────────────────────────────────────────────────────────────────┐     │
│     │  1. Deserialize PublishPayload                                     │     │
│     │  2. Extract topic = "highway/3/telemetry"                         │     │
│     │  3. Extract data = [36 bytes]                                      │     │
│     │  4. Call broker_.on_publish(topic, data, qos, this)               │     │
│     └────────────────────────────────────────────────────────────────────┘     │
│                       │                                                         │
│                       ▼                                                         │
│  ╔═══════════════════════════════════════════════════════════════════════════╗ │
│  ║  STEP 5: BROKER ROUTES MESSAGE                                            ║ │
│  ╚═══════════════════════════════════════════════════════════════════════════╝ │
│                                                                                 │
│     Broker::on_publish()                                                        │
│     ┌────────────────────────────────────────────────────────────────────┐     │
│     │  1. topic_manager_.register_topic("highway/3/telemetry")          │     │
│     │                                                                    │     │
│     │  2. subscription_manager_.get_subscribers("highway/3/telemetry")  │     │
│     │                                                                    │     │
│     │     Pattern Matching:                                              │     │
│     │     ┌─────────────────────────────────────────────────────────┐   │     │
│     │     │ "highway/+/telemetry"  →  MATCH!  →  [Monitor1, Monitor2]│  │     │
│     │     │ "highway/#"            →  MATCH!  →  [Dashboard]         │  │     │
│     │     │ "highway/1/alerts"     →  NO MATCH                       │  │     │
│     │     └─────────────────────────────────────────────────────────┘   │     │
│     │                                                                    │     │
│     │  3. For each matching subscriber:                                 │     │
│     │     subscriber.session->deliver(topic, payload, qos)             │     │
│     └────────────────────────────────────────────────────────────────────┘     │
│                       │                                                         │
│                       ▼                                                         │
│  ╔═══════════════════════════════════════════════════════════════════════════╗ │
│  ║  STEP 6: DELIVER TO SUBSCRIBERS                                           ║ │
│  ╚═══════════════════════════════════════════════════════════════════════════╝ │
│                                                                                 │
│     Session::deliver(topic, payload, qos)                                       │
│     ┌────────────────────────────────────────────────────────────────────┐     │
│     │  1. Create PUBLISH packet for subscriber                          │     │
│     │  2. Add to write_queue_                                           │     │
│     │  3. write_next() → async_write to socket                          │     │
│     └────────────────────────────────────────────────────────────────────┘     │
│                       │                                                         │
│                       │ TCP/IP                                                  │
│                       ▼                                                         │
│  ╔═══════════════════════════════════════════════════════════════════════════╗ │
│  ║  STEP 7: SUBSCRIBER PROCESSES MESSAGE                                     ║ │
│  ╚═══════════════════════════════════════════════════════════════════════════╝ │
│                                                                                 │
│     TrafficMonitor                                                              │
│     ┌────────────────────────────────────────────────────────────────────┐     │
│     │  on_message("highway/3/telemetry", payload):                      │     │
│     │                                                                    │     │
│     │      SensorEvent event = SensorEvent::deserialize(payload);       │     │
│     │                                                                    │     │
│     │      // Process: check for slow traffic                           │     │
│     │      if (event.speed_kmh < 5.0) {                                 │     │
│     │          // Track duration...                                      │     │
│     │          if (duration > 3_minutes) {                              │     │
│     │              🚨 GENERATE TRAFFIC ALERT!                           │     │
│     │          }                                                         │     │
│     │      }                                                             │     │
│     └────────────────────────────────────────────────────────────────────┘     │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Protocol Specification

### Packet Format

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                          PACKET WIRE FORMAT                                     │
│                                                                                 │
│     ┌─────────────────────────────────────────────────────────────────────┐    │
│     │                         PACKET STRUCTURE                             │    │
│     ├─────────────────────────────────────────────────────────────────────┤    │
│     │                                                                      │    │
│     │   Byte 0      Byte 1      Byte 2-3         Byte 4+                  │    │
│     │  ┌────────┬───────────┬──────────────┬─────────────────────────┐   │    │
│     │  │  Type  │   Flags   │ Remaining Len│       Payload           │   │    │
│     │  │ 1 byte │  1 byte   │   2 bytes    │   Variable length       │   │    │
│     │  └────────┴───────────┴──────────────┴─────────────────────────┘   │    │
│     │                                                                      │    │
│     └─────────────────────────────────────────────────────────────────────┘    │
│                                                                                 │
│     ┌─────────────────────────────────────────────────────────────────────┐    │
│     │  TYPE BYTE                                                          │    │
│     │  ──────────                                                         │    │
│     │                                                                      │    │
│     │  0x10 = CONNECT         0x80 = SUBSCRIBE                           │    │
│     │  0x20 = CONNACK         0x90 = SUBACK                              │    │
│     │  0x30 = PUBLISH         0xA0 = UNSUBSCRIBE                         │    │
│     │  0x40 = PUBACK          0xB0 = UNSUBACK                            │    │
│     │  0xC0 = PINGREQ         0xD0 = PINGRESP                            │    │
│     │  0xE0 = DISCONNECT                                                  │    │
│     └─────────────────────────────────────────────────────────────────────┘    │
│                                                                                 │
│     ┌─────────────────────────────────────────────────────────────────────┐    │
│     │  FLAGS BYTE (for PUBLISH)                                           │    │
│     │  ─────────────────────────                                          │    │
│     │                                                                      │    │
│     │  Bit 7  6  5  4  3  2  1  0                                        │    │
│     │      └──┴──┴──┴──┴──┼──┴──┘                                        │    │
│     │       Reserved       │                                              │    │
│     │                  QoS Level                                          │    │
│     │                  00 = At most once                                  │    │
│     │                  01 = At least once                                 │    │
│     │                  10 = Exactly once                                  │    │
│     └─────────────────────────────────────────────────────────────────────┘    │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### String Encoding

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                          STRING ENCODING                                        │
│                                                                                 │
│     Length-prefixed UTF-8 strings:                                             │
│                                                                                 │
│     ┌─────────────┬────────────────────────────────────────────┐               │
│     │ Length (2B) │              UTF-8 Data                    │               │
│     └─────────────┴────────────────────────────────────────────┘               │
│                                                                                 │
│     Example: "highway/3/telemetry" (20 characters)                             │
│                                                                                 │
│     ┌────┬────┬───────────────────────────────────────────────┐                │
│     │ 00 │ 14 │ h  i  g  h  w  a  y  /  3  /  t  e  l  e ... │                │
│     └────┴────┴───────────────────────────────────────────────┘                │
│       └─────┘                                                                   │
│       Length = 20 (0x0014)                                                      │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Topic System

### Topic Naming Convention

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                        TOPIC NAMING FOR HIGHWAY SYSTEM                          │
│                                                                                 │
│     Format: {domain}/{identifier}/{data_type}                                  │
│                                                                                 │
│     ┌─────────────────────────────────────────────────────────────────────┐    │
│     │  Telemetry Topics (Published by Sensors)                            │    │
│     │  ──────────────────────────────────────                             │    │
│     │                                                                      │    │
│     │  highway/1/telemetry     ─►  Sensor 1 data                          │    │
│     │  highway/2/telemetry     ─►  Sensor 2 data                          │    │
│     │  highway/3/telemetry     ─►  Sensor 3 data                          │    │
│     │  highway/{N}/telemetry   ─►  Sensor N data                          │    │
│     └─────────────────────────────────────────────────────────────────────┘    │
│                                                                                 │
│     ┌─────────────────────────────────────────────────────────────────────┐    │
│     │  Alert Topics (Published by Monitor)                                │    │
│     │  ─────────────────────────────────────                              │    │
│     │                                                                      │    │
│     │  highway/1/alerts        ─►  Alerts for sensor 1 area               │    │
│     │  highway/2/alerts        ─►  Alerts for sensor 2 area               │    │
│     │  highway/{N}/alerts      ─►  Alerts for sensor N area               │    │
│     └─────────────────────────────────────────────────────────────────────┘    │
│                                                                                 │
│     ┌─────────────────────────────────────────────────────────────────────┐    │
│     │  Subscription Patterns                                              │    │
│     │  ─────────────────────                                              │    │
│     │                                                                      │    │
│     │  highway/+/telemetry     ─►  All sensor telemetry                   │    │
│     │  highway/+/alerts        ─►  All alerts                             │    │
│     │  highway/#               ─►  Everything (telemetry + alerts)        │    │
│     │  highway/1/#             ─►  All data from sensor 1                 │    │
│     └─────────────────────────────────────────────────────────────────────┘    │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Sequence Diagrams

### Connection Sequence

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                       CONNECTION SEQUENCE                                       │
│                                                                                 │
│   Client                          Broker                                        │
│     │                               │                                           │
│     │───────── TCP Connect ────────►│                                           │
│     │                               │                                           │
│     │          TCP ACK              │                                           │
│     │◄──────────────────────────────│                                           │
│     │                               │                                           │
│     │        CONNECT Packet         │  {client_id: "sensor_1",                  │
│     │──────────────────────────────►│   keepalive: 60}                          │
│     │                               │                                           │
│     │                               │──► Create Session                         │
│     │                               │──► Set state = Authenticated              │
│     │                               │                                           │
│     │        CONNACK Packet         │  {result: Accepted}                       │
│     │◄──────────────────────────────│                                           │
│     │                               │                                           │
│     │      Ready for Pub/Sub        │                                           │
│     │                               │                                           │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Publish/Subscribe Sequence

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    PUBLISH/SUBSCRIBE SEQUENCE                                   │
│                                                                                 │
│   Sensor              Broker              Monitor                               │
│     │                   │                   │                                   │
│     │                   │    SUBSCRIBE      │                                   │
│     │                   │◄──────────────────│  "highway/+/telemetry"            │
│     │                   │                   │                                   │
│     │                   │    SUBACK         │                                   │
│     │                   │──────────────────►│                                   │
│     │                   │                   │                                   │
│     │    PUBLISH        │                   │                                   │
│     │──────────────────►│                   │  topic: "highway/3/telemetry"     │
│     │                   │                   │  data: SensorEvent                │
│     │                   │                   │                                   │
│     │                   │──► Route Message  │                                   │
│     │                   │                   │                                   │
│     │                   │    PUBLISH        │  (forwarded)                      │
│     │                   │──────────────────►│                                   │
│     │                   │                   │                                   │
│     │                   │                   │──► Process event                  │
│     │                   │                   │──► Check for slow traffic         │
│     │                   │                   │                                   │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Alert Generation Sequence

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    TRAFFIC ALERT GENERATION                                     │
│                                                                                 │
│  Sensor           Broker           Monitor           AlertService              │
│    │                │                 │                   │                     │
│    │  speed=3km/h   │                 │                   │                     │
│    │───────────────►│                 │                   │                     │
│    │                │────────────────►│                   │                     │
│    │                │                 │──►Store speed     │                     │
│    │                │                 │                   │                     │
│    │  speed=2km/h   │                 │                   │                     │
│    │───────────────►│                 │                   │                     │
│    │                │────────────────►│                   │                     │
│    │                │                 │──►Store speed     │                     │
│    │                │                 │                   │                     │
│    │     ...        │                 │                   │                     │
│    │  (3 minutes    │                 │                   │                     │
│    │   of slow      │                 │                   │                     │
│    │   traffic)     │                 │                   │                     │
│    │     ...        │                 │                   │                     │
│    │                │                 │                   │                     │
│    │                │                 │──►Check duration  │                     │
│    │                │                 │   Duration > 3min │                     │
│    │                │                 │   Speed < 5 km/h  │                     │
│    │                │                 │                   │                     │
│    │                │                 │  🚨 ALERT!        │                     │
│    │                │                 │                   │                     │
│    │                │  PUBLISH        │                   │                     │
│    │                │◄────────────────│                   │                     │
│    │                │  topic: highway/3/alerts            │                     │
│    │                │                 │                   │                     │
│    │                │  PUBLISH (forward)                  │                     │
│    │                │─────────────────────────────────────►                     │
│    │                │                 │                   │                     │
│    │                │                 │                   │──►Display Alert     │
│    │                │                 │                   │──►Notify Operators  │
│    │                │                 │                   │                     │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## Summary

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           SYSTEM SUMMARY                                        │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  COMPONENTS:                                                                    │
│  ───────────                                                                    │
│  • Broker          - Central message router (port 1883)                        │
│  • Session         - Per-client connection handler                             │
│  • TopicManager    - Topic registry + wildcard matching                        │
│  • SubscriptionManager - Pub/sub routing                                       │
│  • Client          - Library for sensors/monitors                              │
│  • SensorEvent     - Binary telemetry data format                              │
│                                                                                 │
│  PROTOCOL:                                                                      │
│  ─────────                                                                      │
│  • MQTT-lite (simplified MQTT v3.1.1)                                          │
│  • Binary wire format for efficiency                                           │
│  • QoS 0/1 support                                                             │
│                                                                                 │
│  FEATURES:                                                                      │
│  ─────────                                                                      │
│  • Wildcard subscriptions (+, #)                                               │
│  • Multi-threaded async I/O                                                    │
│  • Low-latency message routing                                                 │
│  • Traffic alert detection                                                     │
│                                                                                 │
│  DATA FLOW:                                                                     │
│  ──────────                                                                     │
│  Sensor → PUBLISH → Broker → Pattern Match → PUBLISH → Subscribers            │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

This documentation is now saved at [ARCHITECTURE.md](ARCHITECTURE.md). It provides:

1. **System Overview** - High-level architecture diagram
2. **Business Use Case** - Highway sensor monitoring explained
3. **Architecture Diagrams** - Layer and component views
4. **Class Responsibilities** - What each class does
5. **Data Flow Pipeline** - Step-by-step message routing
6. **Protocol Specification** - Wire format details
7. **Topic System** - Naming conventions and wildcards
8. **Sequence Diagrams** - Connection, pub/sub, and alert flows