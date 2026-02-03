#include "broker/broker.hpp"
#include <iostream>

namespace highway {

Broker::Broker() : Broker(Config{}) {}

Broker::Broker(Config config)
    : config_(std::move(config)),
      io_context_(),
      acceptor_(io_context_, tcp::endpoint(tcp::v4(), config_.port)),
      work_guard_(boost::asio::make_work_guard(io_context_)) {
    
    // Configure acceptor for high performance
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    
    // Set socket options for performance
    boost::asio::socket_base::receive_buffer_size recv_buf_option(config_.buffer_size);
    boost::asio::socket_base::send_buffer_size send_buf_option(config_.buffer_size);
    acceptor_.set_option(recv_buf_option);
    acceptor_.set_option(send_buf_option);
}

Broker::~Broker() {
    stop();
}

void Broker::start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }
    
    std::cout << "[BROKER] Starting on port " << config_.port << std::endl;
    std::cout << "[BROKER] IO threads: " << config_.io_threads << std::endl;
    
    // Start accepting connections
    accept_loop();
    
    // Start IO threads
    run_io_threads();
    
    std::cout << "[BROKER] Ready to accept connections" << std::endl;
}

void Broker::stop() {
    if (!running_.exchange(false)) {
        return;  // Not running
    }
    
    std::cout << "[BROKER] Shutting down..." << std::endl;
    
    // Close acceptor
    boost::system::error_code ec;
    acceptor_.close(ec);
    
    // Close all sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [ptr, session] : sessions_) {
            session->close();
        }
        sessions_.clear();
    }
    
    // Stop IO context
    work_guard_.reset();
    io_context_.stop();
    
    // Wait for threads
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    io_threads_.clear();
    
    std::cout << "[BROKER] Shutdown complete" << std::endl;
}

void Broker::accept_loop() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec && running_) {
                // Configure socket for low latency
                socket.set_option(tcp::no_delay(true));
                
                auto session = std::make_shared<Session>(std::move(socket), *this);
                
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    if (sessions_.size() < config_.max_connections) {
                        sessions_[session.get()] = session;
                        session->start();
                        std::cout << "[BROKER] New connection, total: " 
                                  << sessions_.size() << std::endl;
                    } else {
                        std::cerr << "[BROKER] Max connections reached, rejecting" << std::endl;
                        session->close();
                    }
                }
            }
            
            if (running_) {
                accept_loop();
            }
        }
    );
}

void Broker::run_io_threads() {
    io_threads_.reserve(config_.io_threads);
    for (size_t i = 0; i < config_.io_threads; ++i) {
        io_threads_.emplace_back([this, i]() {
            std::cout << "[BROKER] IO thread " << i << " started" << std::endl;
            io_context_.run();
            std::cout << "[BROKER] IO thread " << i << " stopped" << std::endl;
        });
    }
}

void Broker::on_session_closed(Session* session) {
    // Remove all subscriptions for this session
    subscription_manager_.remove_session(session);
    
    // Remove from sessions map
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(session);
    }
    
    std::cout << "[BROKER] Session closed: " << session->client_id() << std::endl;
}

void Broker::on_publish(const std::string& topic, const std::vector<uint8_t>& payload,
                        QoS qos, Session* from) {
    total_messages_in_.fetch_add(1, std::memory_order_relaxed);
    
    // Register topic if new
    topic_manager_.register_topic(topic);
    
    // Find all matching subscribers
    auto subscribers = subscription_manager_.get_subscribers(topic);
    
    // Deliver to each subscriber
    for (const auto& sub : subscribers) {
        if (sub.session != from) {  // Don't echo back to sender
            sub.session->deliver(topic, payload, std::min(qos, sub.qos));
            total_messages_out_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void Broker::on_subscribe(Session* session, const std::string& topic, QoS qos) {
    subscription_manager_.subscribe(session, topic, qos);
    session->add_subscription(topic);
    
    std::cout << "[BROKER] " << session->client_id() 
              << " subscribed to: " << topic << std::endl;
}

void Broker::on_unsubscribe(Session* session, const std::string& topic) {
    subscription_manager_.unsubscribe(session, topic);
    session->remove_subscription(topic);
    
    std::cout << "[BROKER] " << session->client_id() 
              << " unsubscribed from: " << topic << std::endl;
}

Broker::Stats Broker::get_stats() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return Stats{
        .active_connections = sessions_.size(),
        .total_messages_in = total_messages_in_.load(),
        .total_messages_out = total_messages_out_.load(),
        .topics_count = topic_manager_.topic_count(),
        .subscriptions_count = subscription_manager_.subscription_count()
    };
}

} // namespace highway
