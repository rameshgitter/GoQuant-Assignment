#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <map>
#include <condition_variable>
#include <iostream>
#include <chrono>
#include "performance_monitor.h"

using json = nlohmann::json;
using namespace std::chrono;

class DeribitWebSocketClient {
private:
    // WebSocket++ client type
    using client = websocketpp::client<websocketpp::config::asio_tls_client>;
    using message_ptr = websocketpp::config::asio_client::message_type::ptr;
    using context_ptr = websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>;
    
    client m_client;
    websocketpp::connection_hdl m_hdl;
    std::thread m_thread;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_running{false};
    
    // Authentication credentials
    std::string m_clientId;
    std::string m_clientSecret;
    std::string m_accessToken;
    
    // Reference to PerformanceMonitor
    PerformanceMonitor& performanceMonitor;
    
    // Callbacks for different channels
    std::map<std::string, std::function<void(const json&)>> m_callbacks;
    std::mutex m_callbackMutex;
    
    // Connection callback
    std::function<void(bool)> m_connectionCallback;
    
    // TLS initialization
    context_ptr on_tls_init() {
        context_ptr ctx = websocketpp::lib::make_shared<websocketpp::lib::asio::ssl::context>(
            websocketpp::lib::asio::ssl::context::sslv23);
        
        try {
            ctx->set_options(
                websocketpp::lib::asio::ssl::context::default_workarounds |
                websocketpp::lib::asio::ssl::context::no_sslv2 |
                websocketpp::lib::asio::ssl::context::no_sslv3 |
                websocketpp::lib::asio::ssl::context::single_dh_use
            );
        } catch (std::exception& e) {
            std::cerr << "Error in TLS setup: " << e.what() << std::endl;
        }
        
        return ctx;
    }
    
    // Authenticate with Deribit API
    void authenticate() {
        if (m_clientId.empty() || m_clientSecret.empty()) {
            std::cerr << "Client ID or Secret not provided, skipping authentication" << std::endl;
            return;
        }
        
        json authRequest = {
            {"jsonrpc", "2.0"},
            {"id", 9929},
            {"method", "public/auth"},
            {"params", {
                {"grant_type", "client_credentials"},
                {"client_id", m_clientId},
                {"client_secret", m_clientSecret}
            }}
        };
        
        std::string message = authRequest.dump();
        
        try {
            m_client.send(m_hdl, message, websocketpp::frame::opcode::text);
            std::cout << "Authentication request sent" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error sending authentication request: " << e.what() << std::endl;
        }
    }
    
    // Process incoming messages
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
        auto start = high_resolution_clock::now(); // Start timing

        try {
            json data = json::parse(msg->get_payload());
            
            // Handle authentication response
            if (data.contains("id") && data["id"] == 9929 && data.contains("result")) {
                m_accessToken = data["result"]["access_token"];
                std::cout << "Successfully authenticated with Deribit" << std::endl;
                return;
            }
            
            // Handle subscription responses
            if (data.contains("id") && data.contains("result") && data["result"].is_array()) {
                std::cout << "Subscription successful: " << data.dump() << std::endl;
                return;
            }
            
            // Handle channel data
            if (data.contains("method") && data["method"] == "subscription" && 
                data.contains("params") && data["params"].contains("channel")) {
                
                std::string channel = data["params"]["channel"];
                
                // Find and execute the appropriate callback
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                auto it = m_callbacks.find(channel);
                if (it != m_callbacks.end()) {
                    it->second(data);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
        }

        auto end = high_resolution_clock::now(); // End timing
        auto latency = duration_cast<microseconds>(end - start).count();
        performanceMonitor.recordWebSocketApiLatency(latency); // Use the correct method name
    }
    
    // Connection handlers
    void on_open(websocketpp::connection_hdl hdl) {
        m_hdl = hdl;
        m_connected = true;
        
        std::cout << "WebSocket connection established" << std::endl;
        
        // Authenticate if credentials are provided
        if (!m_clientId.empty() && !m_clientSecret.empty()) {
            authenticate();
        }
        
        // Notify via callback if registered
        if (m_connectionCallback) {
            m_connectionCallback(true);
        }
    }
    
    void on_close(websocketpp::connection_hdl hdl) {
        m_connected = false;
        std::cout << "WebSocket connection closed" << std::endl;
        
        // Notify via callback if registered
        if (m_connectionCallback) {
            m_connectionCallback(false);
        }
    }
    
    void on_fail(websocketpp::connection_hdl hdl) {
        m_connected = false;
        std::cerr << "WebSocket connection failed" << std::endl;
        
        // Notify via callback if registered
        if (m_connectionCallback) {
            m_connectionCallback(false);
        }
    }

public:
    DeribitWebSocketClient(const std::string& clientId, const std::string& clientSecret, PerformanceMonitor& pm)
        : m_clientId(clientId), m_clientSecret(clientSecret), performanceMonitor(pm) {
        
        // Initialize WebSocket client
        m_client.clear_access_channels(websocketpp::log::alevel::all);
        m_client.clear_error_channels(websocketpp::log::elevel::all);
        
        m_client.init_asio();
        m_client.set_tls_init_handler([this](websocketpp::connection_hdl) {
            return this->on_tls_init();
        });
        
        // Set handlers
        m_client.set_message_handler([this](websocketpp::connection_hdl hdl, message_ptr msg) {
            this->on_message(hdl, msg);
        });
        
        m_client.set_open_handler([this](websocketpp::connection_hdl hdl) {
            this->on_open(hdl);
        });
        
        m_client.set_close_handler([this](websocketpp::connection_hdl hdl) {
            this->on_close(hdl);
        });
        
        m_client.set_fail_handler([this](websocketpp::connection_hdl hdl) {
            this->on_fail(hdl);
        });
    }
    
    ~DeribitWebSocketClient() {
        disconnect();
    }
    
    // Connect to Deribit WebSocket API
    void connect() {
        if (m_running) {
            std::cout << "WebSocket client is already running" << std::endl;
            return;
        }
        
        m_running = true;
        
        // Start the WebSocket client in a separate thread
        m_thread = std::thread([this]() {
            try {
                // Connect to Deribit WebSocket API
                std::string wsUrl = "wss://test.deribit.com/ws/api/v2";
                websocketpp::lib::error_code ec;
                client::connection_ptr con = m_client.get_connection(wsUrl, ec);
                
                if (ec) {
                    std::cerr << "Could not create connection: " << ec.message() << std::endl;
                    m_running = false;
                    return;
                }
                
                m_client.connect(con);
                m_client.run();
            } catch (const std::exception& e) {
                std::cerr << "Exception in WebSocket thread: " << e.what() << std::endl;
            }
            
            m_running = false;
            m_connected = false;
        });
    }
    
    // Disconnect from WebSocket server
    void disconnect() {
        if (!m_running) {
            return;
        }
        
        try {
            // Close the WebSocket connection
            if (m_connected) {
                m_client.close(m_hdl, websocketpp::close::status::normal, "Client disconnecting");
            }
            
            // Stop the WebSocket client
            m_client.stop();
            m_running = false;
            
            // Wait for the thread to finish
            if (m_thread.joinable()) {
                m_thread.join();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error during disconnect: " << e.what() << std::endl;
        }
    }
    
    // Subscribe to a channel
    bool subscribe(const std::string& channel) {
        if (!m_connected) {
            std::cerr << "Not connected to WebSocket server" << std::endl;
            return false;
        }
        
        try {
            json subscriptionRequest = {
                {"jsonrpc", "2.0"},
                {"id", 42},
                {"method", "public/subscribe"},
                {"params", {
                    {"channels", {channel}}
                }}
            };
            
            std::string message = subscriptionRequest.dump();
            m_client.send(m_hdl, message, websocketpp::frame::opcode::text);
            std::cout << "Subscription request sent for channel: " << channel << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error subscribing to channel: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Unsubscribe from a channel
    bool unsubscribe(const std::string& channel) {
        if (!m_connected) {
            std::cerr << "Not connected to WebSocket server" << std::endl;
            return false;
        }
        
        try {
            json unsubscribeRequest = {
                {"jsonrpc", "2.0"},
                {"id", 43},
                {"method", "public/unsubscribe"},
                {"params", {
                    {"channels", {channel}}
                }}
            };
            
            std::string message = unsubscribeRequest.dump();
            m_client.send(m_hdl, message, websocketpp::frame::opcode::text);
            
            // Remove the callback
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            m_callbacks.erase(channel);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error unsubscribing from channel: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Register a callback for a specific channel
    void registerCallback(const std::string& channel, std::function<void(const json&)> callback) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_callbacks[channel] = callback;
    }
    
    // Set connection callback
    void setConnectionCallback(std::function<void(bool)> callback) {
        m_connectionCallback = callback;
    }
    
    // Check if connected to WebSocket server
    bool isConnected() const {
        return m_connected;
    }
};

#endif // WEBSOCKET_CLIENT_H 