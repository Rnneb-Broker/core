#include "broker/session.hpp"
#include "broker/broker.hpp"
#include "protocol/serialization.hpp"
#include <iostream>

namespace highway {

Session::Session(tcp::socket socket, Broker &broker)
    : socket_(std::move(socket)), broker_(broker) {}

Session::~Session() { close(); }

void Session::start() { read_header(); }

void Session::close() {
  if (state_ == State::Disconnected) {
    return;
  }

  state_ = State::Disconnected;

  boost::system::error_code ec;

  socket_.shutdown(tcp::socket::shutdown_both, ec);

  if (ec.failed())
    std::cout << "close session failed \n";

  socket_.close(ec);

  broker_.on_session_closed(this);
}

void Session::read_header() {
  auto self = shared_from_this();
  
  // Use shared_ptr to keep the buffer alive during async operation
  auto header_bytes = std::make_shared<std::array<uint8_t, 4>>();
  
  boost::asio::async_read(
      socket_, boost::asio::buffer(*header_bytes, 4),
      [this, self, header_bytes](boost::system::error_code ec, std::size_t bytes_read) {
        if (!ec) {
          // Manually parse the header bytes
          header_.type = (*header_bytes)[0];
          header_.flags = (*header_bytes)[1];
          // Convert from network byte order (big-endian) to host byte order
          header_.remaining_len = ((*header_bytes)[2] << 8) | (*header_bytes)[3];
          
          if (header_.remaining_len > 0) {
            read_payload(header_.remaining_len);
          } else {
            payload_buffer_.clear();
            process_packet();
            read_header();
          }
        } else if (ec != boost::asio::error::operation_aborted) {
          close();
        }
      });
}

void Session::read_payload(uint16_t length) {
  // std::cout << "[DEBUG:READ_PLD] Reading " << length << " bytes payload..." << std::endl;
  payload_buffer_.resize(length);
  auto self = shared_from_this();
  boost::asio::async_read(
      socket_, boost::asio::buffer(payload_buffer_),
      [this, self, length](boost::system::error_code ec, std::size_t bytes_read) {
        if (!ec) {
          // std::cout << "[DEBUG:READ_PLD] Received " << bytes_read << " bytes" << std::endl;
          process_packet();
          read_header();
        } else if (ec != boost::asio::error::operation_aborted) {
          // std::cout << "[DEBUG:READ_PLD] ERROR: " << ec.message() << std::endl;
          close();
        }
      });
}

void Session::process_packet() {
  PacketType type = static_cast<PacketType>(header_.type);
  // std::cout << "[DEBUG:PROCESS] packet type=0x" << std::hex << (int)header_.type << std::dec << std::endl;

  switch (type) {
  case PacketType::CONNECT:
    std::cout << "[DEBUG:PROCESS] CONNECT packet detected" << std::endl;
    handle_connect();
    break;
  case PacketType::PUBLISH:
    handle_publish();
    break;
  case PacketType::SUBSCRIBE:
    handle_subscribe();
    break;
  case PacketType::UNSUBSCRIBE:
    handle_unsubscribe();
    break;
  case PacketType::PINGREQ:
    handle_pingreq();
    break;
  case PacketType::DISCONNECT:
    handle_disconnect();
    break;
  default:
    std::cerr << "[SESSION] Unknown packet type: "
              << static_cast<int>(header_.type) << std::endl;
    break;
  }

  messages_received_.fetch_add(1, std::memory_order_relaxed);
}

void Session::handle_connect() {
  if (state_ != State::Connected) {
    send_connack(ConnectResult::IdentifierRejected);
    return;
  }

  try {
    auto payload = ConnectPayload::deserialize(payload_buffer_.data(),
                                               payload_buffer_.size());

    client_id_ = payload.client_id;
    username_ = payload.username;
    keepalive_ = payload.keepalive;

    state_ = State::Authenticated;

    std::cout << "[SESSION] Client connected: " << client_id_ << std::endl;

    send_connack(ConnectResult::Accepted);
  } catch (const std::exception &e) {
    std::cerr << "[SESSION] CONNECT parse error: " << e.what() << std::endl;
    send_connack(ConnectResult::IdentifierRejected);
  }
}

void Session::handle_publish() {
  if (!is_authenticated()) {
    return;
  }

  try {
    auto payload = PublishPayload::deserialize(payload_buffer_.data(),
                                               payload_buffer_.size());

    QoS qos = static_cast<QoS>(header_.flags & 0x06);

    broker_.on_publish(payload.topic, payload.data, qos, this);

    // Send PUBACK for QoS 1
    if (qos == QoS::AtLeastOnce) {
      Packet puback;
      puback.header.type = static_cast<uint8_t>(PacketType::PUBACK);
      puback.header.flags = 0;
      puback.header.remaining_len = 2;

      BinaryWriter writer;
      writer.write_u16(payload.packet_id);
      puback.payload = writer.release();

      send(puback);
    }
  } catch (const std::exception &e) {
    std::cerr << "[SESSION] PUBLISH parse error: " << e.what() << std::endl;
  }
}

void Session::handle_subscribe() {
  if (!is_authenticated()) {
    return;
  }

  try {
    auto payload = SubscribePayload::deserialize(payload_buffer_.data(),
                                                 payload_buffer_.size());

    std::vector<QoS> granted_qos;
    for (const auto &[topic, qos] : payload.topics) {
      broker_.on_subscribe(this, topic, qos);
      granted_qos.push_back(qos);
    }

    send_suback(payload.packet_id, granted_qos);
  } catch (const std::exception &e) {
    std::cerr << "[SESSION] SUBSCRIBE parse error: " << e.what() << std::endl;
  }
}

void Session::handle_unsubscribe() {
  if (!is_authenticated()) {
    return;
  }

  try {
    BinaryReader reader(payload_buffer_.data(), payload_buffer_.size());
    uint16_t packet_id = reader.read_u16();

    while (!reader.empty()) {
      std::string topic = reader.read_string();
      broker_.on_unsubscribe(this, topic);
    }

    send_unsuback(packet_id);
  } catch (const std::exception &e) {
    std::cerr << "[SESSION] UNSUBSCRIBE parse error: " << e.what() << std::endl;
  }
}

void Session::handle_pingreq() { send_pingresp(); }

void Session::handle_disconnect() {
  state_ = State::Disconnecting;
  close();
}

void Session::send(const Packet &packet) { send_raw(packet.serialize()); }

void Session::send_raw(std::vector<uint8_t> data) {
  // std::cout << "[DEBUG:SEND_RAW] Sending " << data.size() << " bytes" << std::endl;
  bool start_write = false;
  {
    std::lock_guard<std::mutex> lock(write_mutex_);
    write_queue_.push_back(std::move(data));
    // std::cout << "[DEBUG:SEND_RAW] Queued. Queue size: " << write_queue_.size() << std::endl;
    start_write = !writing_.exchange(true);
  }

  if (start_write) {
    // std::cout << "[DEBUG:SEND_RAW] Posting async write" << std::endl;
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self]() { do_write(); });
  } else {
    // std::cout << "[DEBUG:SEND_RAW] Write already in progress" << std::endl;
  }
}

void Session::do_write() {
  // Copy front of queue while holding lock
  std::vector<uint8_t> data;
  {
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (write_queue_.empty()) {
      // std::cout << "[DEBUG:WRITE] Queue empty, stopping" << std::endl;
      writing_ = false;
      return;
    }
    data = write_queue_.front();
  }

  // std::cout << "[DEBUG:WRITE] Writing " << data.size() << " bytes..." << std::endl;
  
  auto self = shared_from_this();
  boost::asio::async_write(
      socket_, boost::asio::buffer(data),
      [this, self, data](boost::system::error_code ec, std::size_t bytes_written) {
        if (!ec) {
          // std::cout << "[DEBUG:WRITE] ✅ Wrote " << bytes_written << " bytes" << std::endl;
          messages_sent_.fetch_add(1, std::memory_order_relaxed);

          {
            std::lock_guard<std::mutex> lock(write_mutex_);
            write_queue_.pop_front();
          }

          // Continue writing if there's more
          do_write();
        } else if (ec != boost::asio::error::operation_aborted) {
          // std::cout << "[DEBUG:WRITE] ❌ ERROR: " << ec.message() << std::endl;
          writing_ = false;
          close();
        }
      });
}

void Session::write_next() {
  // Deprecated - kept for compatibility, use do_write instead
  do_write();
}

void Session::deliver(const std::string &topic,
                      const std::vector<uint8_t> &payload, QoS qos) {
  PublishPayload pub;
  pub.topic = topic;
  // Generate packet_id for QoS > 0 (required for acknowledgment matching)
  pub.packet_id = (qos > QoS::AtMostOnce) ? ++last_packet_id_ : 0;
  pub.data = payload;

  Packet packet = Packet::create(
      PacketType::PUBLISH, static_cast<uint8_t>(qos) << 1, pub.serialize());

  send(packet);
}

void Session::send_connack(ConnectResult result) {
  Packet packet;
  packet.header.type = static_cast<uint8_t>(PacketType::CONNACK);
  packet.header.flags = 0;
  packet.header.remaining_len = 2;
  packet.payload = {0, static_cast<uint8_t>(result)};
  
  send(packet);
}

void Session::send_suback(uint16_t packet_id,
                          const std::vector<QoS> &granted_qos) {
  BinaryWriter writer;
  writer.write_u16(packet_id);
  for (QoS qos : granted_qos) {
    writer.write_u8(static_cast<uint8_t>(qos));
  }

  Packet packet = Packet::create(PacketType::SUBACK, 0, writer.release());
  send(packet);
}

void Session::send_unsuback(uint16_t packet_id) {
  BinaryWriter writer;
  writer.write_u16(packet_id);

  Packet packet = Packet::create(PacketType::UNSUBACK, 0, writer.release());
  send(packet);
}

void Session::send_pingresp() {
  Packet packet = Packet::create(PacketType::PINGRESP, 0, {});
  send(packet);
}

void Session::add_subscription(const std::string &topic) {
  subscriptions_.insert(topic);
}

void Session::remove_subscription(const std::string &topic) {
  subscriptions_.erase(topic);
}

} // namespace highway
