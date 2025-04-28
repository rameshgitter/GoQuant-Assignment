#include <iostream>
#include <string>
#include <chrono>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <map>
#include <mutex>
#include "websocket_client.h"
#include <numeric>
#include <algorithm>
#include <iomanip>
#include "performance_monitor.h"

using json = nlohmann::json;
using namespace std::chrono;

// Create a global instance of PerformanceMonitor
PerformanceMonitor performanceMonitor;

// Add these color definitions at the top of your file, after the includes
#define RESET   "\033[0m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BOLD    "\033[1m"
#define UNDERLINE "\033[4m"
#define BG_BLACK "\033[40m"
#define BG_RED "\033[41m"
#define BG_GREEN "\033[42m"
#define BG_YELLOW "\033[43m"
#define BG_BLUE "\033[44m"
#define BG_MAGENTA "\033[45m"
#define BG_CYAN "\033[46m"
#define BG_WHITE "\033[47m"

class DeribitAPIClient {
private:
    static inline CURL* curlHandle = nullptr;
    static inline std::atomic<bool> initialized{false};

    // --------------------------------------------------------------------
    // 1. Configure your Deribit Test API credentials here
    //    (In production, store these securely or in environment variables)
    // --------------------------------------------------------------------
    std::string clientId = "37SSZh4R";
    std::string clientSecret = "GJbkkwiwElUtOPZKolJ5SXjQHgh4vuVxMOmrqC534Yw";
    std::string accessToken;  // Retrieved via 'public/auth'

    // Deribit Test endpoint
    std::string apiUrl = "https://test.deribit.com/api/v2";

    // Helper function to accumulate libcurl response
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
        data->append(ptr, size * nmemb);
        return size * nmemb;
    }

    // --------------------------------------------------------------------
    // 2. Deribit Authentication
    //    Authenticates via "public/auth" using client credentials.
    //    On success, stores the returned access_token.
    // --------------------------------------------------------------------
    void authenticate() {
        // Prepare JSON-RPC request for authentication
        json authRequest = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "public/auth"},
            {"params", {
                {"grant_type", "client_credentials"},
                {"client_id", clientId},
                {"client_secret", clientSecret}
            }}
        };

        // Send the auth request
        json response = sendRequest(authRequest, /*isPrivate*/ false);
        if (response.contains("result") && response["result"].contains("access_token")) {
            accessToken = response["result"]["access_token"].get<std::string>();
            std::cout << "Successfully authenticated. Access Token: " << accessToken << std::endl;
        } else {
            std::cerr << "Authentication failed. Response: " << response.dump() << std::endl;
        }
    }

public:
    DeribitAPIClient() {
        // Initialize curl only once, in a thread-safe manner
        if (!initialized.exchange(true)) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
            curlHandle = curl_easy_init();
            
            // Set some connection optimization options
            curl_easy_setopt(curlHandle, CURLOPT_TCP_KEEPALIVE, 1L);
            curl_easy_setopt(curlHandle, CURLOPT_TCP_KEEPIDLE, 120L);
            curl_easy_setopt(curlHandle, CURLOPT_TCP_KEEPINTVL, 60L);

            // Allow persistent connections
            curl_easy_setopt(curlHandle, CURLOPT_FORBID_REUSE, 0L);
            curl_easy_setopt(curlHandle, CURLOPT_DNS_CACHE_TIMEOUT, 60L);
        }
        
        // Perform initial authentication to retrieve the access token
        authenticate();
    }

    // --------------------------------------------------------------------
    // 3. Core JSON-RPC request function
    //    - Adds Authorization header if the method is private.
    //    - Measures round-trip latency in microseconds.
    // --------------------------------------------------------------------
    json sendRequest(const json& request, bool isPrivate = false) {
        std::string readBuffer;
        std::string requestStr = request.dump();

        // Build headers
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: application/json");

        // If it's a private method, include the bearer token
        if (isPrivate && !accessToken.empty()) {
            std::string authHeader = "Authorization: Bearer " + accessToken;
            headers = curl_slist_append(headers, authHeader.c_str());
        }

        // Reset and set options
        curl_easy_reset(curlHandle);
        curl_easy_setopt(curlHandle, CURLOPT_URL, apiUrl.c_str());
        curl_easy_setopt(curlHandle, CURLOPT_POST, 1L);
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, requestStr.c_str());
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE, requestStr.length());
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

        // Set timeouts (tweak as needed)
        curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT_MS, 5000L);
        curl_easy_setopt(curlHandle, CURLOPT_CONNECTTIMEOUT_MS, 2000L);

        // Measure latency
        auto start = high_resolution_clock::now();
        CURLcode res = curl_easy_perform(curlHandle);
        auto end = high_resolution_clock::now();
        
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
            return {};
        }

        auto latency = duration_cast<microseconds>(end - start).count();
        std::cout << "API Call Latency: " << latency << " microseconds" << std::endl;
        
        // Record latency for performance monitoring
        performanceMonitor.recordRestApiLatency(latency);

        try {
            return json::parse(readBuffer);
        } catch (const std::exception& e) {
            std::cerr << "JSON Parse Error: " << e.what() << std::endl;
            return {};
        }
    }

    ~DeribitAPIClient() {
        if (curlHandle) {
            curl_easy_cleanup(curlHandle);
            curl_global_cleanup();
        }
    }
};

// --------------------------------------------------------------------
// OrderManager uses the DeribitAPIClient for private and public calls
// --------------------------------------------------------------------
class OrderManager {
private:
    DeribitAPIClient apiClient;

public:
    // Place a limit buy order
    bool placeOrder(const std::string& instrument, double price, double amount) {
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 101},
            {"method", "private/buy"},
            {"params", {
                {"instrument_name", instrument},
                {"amount", amount},
                {"price", price},
                {"type", "limit"}
            }}
        };

        // private method => pass isPrivate = true
        json response = apiClient.sendRequest(request, /*isPrivate*/ true);
        if (response.contains("result")) {
            std::cout << "Order placed successfully. Full response:\n" 
                      << response.dump(2) << std::endl;
            return true;
        } else {
            std::cerr << "Order placement failed. Response:\n" 
                      << response.dump(2) << std::endl;
            return false;
        }
    }

    // Cancel an order
    bool cancelOrder(const std::string& orderId) {
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 102},
            {"method", "private/cancel"},
            {"params", {{"order_id", orderId}}}
        };

        json response = apiClient.sendRequest(request, true);
        if (response.contains("result")) {
            std::cout << "Order cancelled successfully. Full response:\n"
                      << response.dump(2) << std::endl;
            return true;
        } else {
            std::cerr << "Order cancellation failed. Response:\n"
                      << response.dump(2) << std::endl;
            return false;
        }
    }

    // Modify an existing order
    bool modifyOrder(const std::string& orderId, double newPrice, double newAmount) {
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 103},
            {"method", "private/edit"},
            {"params", {
                {"order_id", orderId},
                {"price", newPrice},
                {"amount", newAmount}
            }}
        };

        json response = apiClient.sendRequest(request, true);
        if (response.contains("result")) {
            std::cout << "Order modified successfully. Full response:\n"
                      << response.dump(2) << std::endl;
            return true;
        } else {
            std::cerr << "Order modification failed. Response:\n"
                      << response.dump(2) << std::endl;
            return false;
        }
    }

    // Public call: get order book for a specific instrument
    json getOrderBook(const std::string& instrument) {
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 104},
            {"method", "public/get_order_book"},
            {"params", {{"instrument_name", instrument}}}
        };
        return apiClient.sendRequest(request, /*isPrivate*/ false);
    }

    // Private call: get positions for a specific currency
    json getPositions(const std::string& currency) {
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 105},
            {"method", "private/get_positions"},
            {"params", {{"currency", currency}}}
        };
        return apiClient.sendRequest(request, true);
    }

    // Public call: get instruments for a given currency/kind
    json getInstruments(const std::string& currency, const std::string& kind = "future") {
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 106},
            {"method", "public/get_instruments"},
            {"params", {
                {"currency", currency},
                {"kind", kind},
                {"expired", false}
            }}
        };
        return apiClient.sendRequest(request, false);
    }

    // In the OrderManager class, add a method to measure end-to-end latency:
    bool placeOrderWithLatencyMeasurement(const std::string& instrument, double price, double amount) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Place the order
        bool result = placeOrder(instrument, price, amount);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::cout << "End-to-end trading loop latency: " << latency << " microseconds" << std::endl;
        
        // You could add a new category in PerformanceMonitor for this
        // performanceMonitor.recordTradingLoopLatency(latency);
        
        return result;
    }
};

// --------------------------------------------------------------------
// MarketDataManager uses the DeribitWebSocketClient for real-time data
// --------------------------------------------------------------------
class MarketDataManager {
private:
    DeribitWebSocketClient wsClient;
    std::map<std::string, json> latestOrderBooks;
    std::mutex orderBookMutex;

public:
    MarketDataManager(const std::string& clientId, const std::string& clientSecret, PerformanceMonitor& pm) 
        : wsClient(clientId, clientSecret, pm) {
        
        // Set connection callback
        wsClient.setConnectionCallback([this](bool connected) {
            if (connected) {
                std::cout << "WebSocket connected, ready for subscriptions" << std::endl;
            } else {
                std::cout << "WebSocket disconnected" << std::endl;
            }
        });
    }

    // Connect to WebSocket server
    void connect() {
        wsClient.connect();
    }

    // Disconnect from WebSocket server
    void disconnect() {
        wsClient.disconnect();
    }

    // Subscribe to order book updates for an instrument
    bool subscribeOrderBook(const std::string& instrument) {
        std::string channel = "book." + instrument + ".100ms";
        
        // Register callback for this channel
        wsClient.registerCallback(channel, [this, instrument](const json& data) {
            try {
                // Process and store order book update
                std::lock_guard<std::mutex> lock(orderBookMutex);
                latestOrderBooks[instrument] = data["params"]["data"];
                
                // Print some basic info about the update
                auto& book = data["params"]["data"];
                std::cout << "Order book update for " << instrument << ": "
                          << "Timestamp: " << book["timestamp"] << ", "
                          << "Best bid: " << (book["bids"].empty() ? "none" : book["bids"][0][0].dump()) << ", "
                          << "Best ask: " << (book["asks"].empty() ? "none" : book["asks"][0][0].dump()) << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error processing order book update: " << e.what() << std::endl;
            }
        });
        
        return wsClient.subscribe(channel);
    }

    // Subscribe to trades for an instrument
    bool subscribeTrades(const std::string& instrument) {
        std::string channel = "trades." + instrument + ".100ms";
        
        wsClient.registerCallback(channel, [instrument](const json& data) {
            // Process trade data
            for (const auto& trade : data["params"]["data"]) {
                std::cout << "Trade on " << instrument << ": "
                          << "Price: " << trade["price"] << ", "
                          << "Amount: " << trade["amount"] << ", "
                          << "Direction: " << trade["direction"] << std::endl;
            }
        });
        
        return wsClient.subscribe(channel);
    }

    // Get the latest order book for an instrument
    json getLatestOrderBook(const std::string& instrument) {
        std::lock_guard<std::mutex> lock(orderBookMutex);
        auto it = latestOrderBooks.find(instrument);
        if (it != latestOrderBooks.end()) {
            return it->second;
        }
        return json();
    }

    // Check if connected to WebSocket server
    bool isConnected() const {
        return wsClient.isConnected();
    }
};

// --------------------------------------------------------------------
// A simple command-line interface to demonstrate usage
// --------------------------------------------------------------------
class TradingCLI {
private:
    OrderManager orderManager;
    MarketDataManager marketDataManager;
    std::atomic<bool> running{true};
    std::thread marketDataThread;

    void clearScreen() {
        // Clear screen - works on most terminals
        std::cout << "\033[2J\033[1;1H";
    }

    void printHeader() {
        clearScreen();
        std::cout << BOLD << BG_BLUE << WHITE << "╔════════════════════════════════════════════════════════════╗" << RESET << std::endl;
        std::cout << BOLD << BG_BLUE << WHITE << "║                DERIBIT TRADING TERMINAL                    ║" << RESET << std::endl;
        std::cout << BOLD << BG_BLUE << WHITE << "╚════════════════════════════════════════════════════════════╝" << RESET << std::endl;
        std::cout << std::endl;
    }

    void printMenu() {
        printHeader();
        
        std::cout << BOLD << "┌─ " << CYAN << "ORDER MANAGEMENT" << RESET << BOLD << " ───────────────────────────────────┐" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "1" << RESET << ". Place Order                                            " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "2" << RESET << ". Cancel Order                                           " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "3" << RESET << ". Modify Order                                           " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "4" << RESET << ". Get Order Book (REST)                                  " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "5" << RESET << ". View Positions                                         " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "└────────────────────────────────────────────────────────┘" << RESET << std::endl;
        std::cout << std::endl;
        
        std::cout << BOLD << "┌─ " << CYAN << "MARKET DATA" << RESET << BOLD << " ──────────────────────────────────────┐" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "6" << RESET << ". View Instruments                                       " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "7" << RESET << ". Connect to WebSocket                                   " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "8" << RESET << ". Subscribe to Order Book (WebSocket)                    " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "9" << RESET << ". Subscribe to Trades (WebSocket)                        " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "└────────────────────────────────────────────────────────┘" << RESET << std::endl;
        std::cout << std::endl;
        
        std::cout << BOLD << "┌─ " << CYAN << "SYSTEM" << RESET << BOLD << " ────────────────────────────────────────┐" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "10" << RESET << ". View Performance Statistics                           " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "│" << RESET << " " << YELLOW << "11" << RESET << ". Exit                                                  " << BOLD << "│" << RESET << std::endl;
        std::cout << BOLD << "└────────────────────────────────────────────────────────┘" << RESET << std::endl;
        std::cout << std::endl;
        
        std::cout << BOLD << CYAN << "Enter your choice: " << RESET;
    }

    void printSectionHeader(const std::string& title) {
        std::cout << std::endl;
        std::cout << BOLD << BG_CYAN << BLACK << " " << title << " " << RESET << std::endl;
        std::cout << BOLD << "═════════════════════════════════════════════════════════" << RESET << std::endl;
    }

    void printSuccess(const std::string& message) {
        std::cout << GREEN << "✓ " << message << RESET << std::endl;
    }

    void printError(const std::string& message) {
        std::cout << RED << "✗ " << message << RESET << std::endl;
    }

    void printInfo(const std::string& message) {
        std::cout << BLUE << "ℹ " << message << RESET << std::endl;
    }

    void printWarning(const std::string& message) {
        std::cout << YELLOW << "⚠ " << message << RESET << std::endl;
    }

    void waitForKeyPress() {
        std::cout << std::endl << BOLD << "Press Enter to continue..." << RESET;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
    }

public:
    TradingCLI(PerformanceMonitor& pm) : marketDataManager("37SSZh4R", "GJbkkwiwElUtOPZKolJ5SXjQHgh4vuVxMOmrqC534Yw", pm) {
        // Initialize with the same credentials as REST API
    }

    ~TradingCLI() {
        // Ensure clean shutdown
        running = false;
        marketDataManager.disconnect(); // Disconnect WebSocket

        // Wait for the market data thread to finish
        if (marketDataThread.joinable()) {
            marketDataThread.join();
        }
    }

    void run() {
        while (running) {
            printMenu();
            int choice;
            std::cin >> choice;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            switch (choice) {
                case 1: handlePlaceOrder(); break;
                case 2: handleCancelOrder(); break;
                case 3: handleModifyOrder(); break;
                case 4: handleOrderBook(); break;
                case 5: handlePositions(); break;
                case 6: handleInstruments(); break;
                case 7: handleConnectWebSocket(); break;
                case 8: handleSubscribeOrderBook(); break;
                case 9: handleSubscribeTrades(); break;
                case 10: handleViewPerformance(); break;
                case 11: 
                    printInfo("Exiting...");
                    running = false;
                    return;
                default:
                    printError("Invalid choice. Try again.");
                    waitForKeyPress();
            }
        }
    }

    void handlePlaceOrder() {
        printSectionHeader("PLACE ORDER");
        
        std::string instrument;
        int quantity;
        std::string type;
        double price = 0.0;

        std::cout << BOLD << "Enter instrument (e.g., ETH-PERPETUAL): " << RESET;
        std::getline(std::cin, instrument);
        
        std::cout << BOLD << "Enter quantity: " << RESET;
        std::cin >> quantity;
        
        std::cout << BOLD << "Enter order type (market/limit): " << RESET;
        std::cin >> type;
        
        if (type == "limit") {
            std::cout << BOLD << "Enter price: " << RESET;
            std::cin >> price;
        }

        std::cout << std::endl << BOLD << "Placing order..." << RESET << std::endl;

        // Place the order using OrderManager
        bool result = orderManager.placeOrderWithLatencyMeasurement(instrument, price, quantity);
        
        if (result) {
            printSuccess("Order placed successfully.");
        } else {
            printError("Order placement failed.");
        }
        
        waitForKeyPress();
    }

    void handleCancelOrder() {
        printSectionHeader("CANCEL ORDER");
        
        std::string orderId;
        std::cout << BOLD << "Order ID to cancel: " << RESET;
        std::getline(std::cin, orderId);
        
        std::cout << std::endl << BOLD << "Cancelling order..." << RESET << std::endl;
        bool result = orderManager.cancelOrder(orderId);
        
        if (result) {
            printSuccess("Order cancelled successfully.");
        } else {
            printError("Order cancellation failed.");
        }
        
        waitForKeyPress();
    }

    void handleModifyOrder() {
        printSectionHeader("MODIFY ORDER");
        
        std::string orderId;
        double newPrice, newAmount;
        
        std::cout << BOLD << "Order ID to modify: " << RESET;
        std::getline(std::cin, orderId);
        
        std::cout << BOLD << "New price: " << RESET;
        std::cin >> newPrice;
        
        std::cout << BOLD << "New amount: " << RESET;
        std::cin >> newAmount;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        std::cout << std::endl << BOLD << "Modifying order..." << RESET << std::endl;
        bool result = orderManager.modifyOrder(orderId, newPrice, newAmount);
        
        if (result) {
            printSuccess("Order modified successfully.");
        } else {
            printError("Order modification failed.");
        }
        
        waitForKeyPress();
    }

    void handleOrderBook() {
        printSectionHeader("ORDER BOOK");
        
        std::string instrument;
        std::cout << BOLD << "Instrument to view order book: " << RESET;
        std::getline(std::cin, instrument);
        
        std::cout << std::endl << BOLD << "Fetching order book..." << RESET << std::endl;
        json orderBook = orderManager.getOrderBook(instrument);

        if (orderBook.contains("result")) {
            auto& res = orderBook["result"];
            
            std::cout << std::endl;
            std::cout << BOLD << "Order Book for " << CYAN << instrument << RESET << std::endl;
            std::cout << "───────────────────────────────────────────" << std::endl;
            
            // Print asks (in reverse order, highest to lowest)
            std::cout << BOLD << "ASKS:" << RESET << std::endl;
            if (res.contains("asks") && res["asks"].is_array()) {
                auto& asks = res["asks"];
                for (int i = std::min(5, (int)asks.size()) - 1; i >= 0; i--) {
                    std::cout << RED << std::fixed << std::setprecision(2) 
                              << std::setw(10) << asks[i][0].get<double>() << " | " 
                              << std::setw(10) << asks[i][1].get<double>() << RESET << std::endl;
                }
            }
            
            // Separator between asks and bids
            std::cout << "───────────────────────────────────────────" << std::endl;
            std::cout << BOLD << "Best Bid: " << GREEN << std::fixed << std::setprecision(2) 
                      << res["best_bid_price"].get<double>() << " x " << res["best_bid_amount"].get<double>() << RESET << std::endl;
            std::cout << BOLD << "Best Ask: " << RED << std::fixed << std::setprecision(2) 
                      << res["best_ask_price"].get<double>() << " x " << res["best_ask_amount"].get<double>() << RESET << std::endl;
            std::cout << "───────────────────────────────────────────" << std::endl;
            
            // Print bids (highest to lowest)
            std::cout << BOLD << "BIDS:" << RESET << std::endl;
            if (res.contains("bids") && res["bids"].is_array()) {
                auto& bids = res["bids"];
                for (int i = 0; i < std::min(5, (int)bids.size()); i++) {
                    std::cout << GREEN << std::fixed << std::setprecision(2) 
                              << std::setw(10) << bids[i][0].get<double>() << " | " 
                              << std::setw(10) << bids[i][1].get<double>() << RESET << std::endl;
                }
            }
        } else {
            printError("Failed to retrieve order book.");
            std::cerr << "Response: " << orderBook.dump(2) << std::endl;
        }
        
        waitForKeyPress();
    }

    void handlePositions() {
        printSectionHeader("POSITIONS");
        
        std::string currency;
        std::cout << BOLD << "Currency (BTC/ETH): " << RESET;
        std::getline(std::cin, currency);
        
        std::cout << std::endl << BOLD << "Fetching positions..." << RESET << std::endl;
        json positions = orderManager.getPositions(currency);
        
        if (positions.contains("result")) {
            auto& result = positions["result"];
            if (result.is_array() && !result.empty()) {
                std::cout << std::endl;
                std::cout << BOLD << "Current Positions for " << CYAN << currency << RESET << std::endl;
                std::cout << "═════════════════════════════════════════════════════════" << std::endl;
                
                for (auto& pos : result) {
                    std::string direction = pos["direction"];
                    std::string color = (direction == "buy") ? GREEN : RED;
                    
                    std::cout << BOLD << "Instrument: " << CYAN << pos["instrument_name"].get<std::string>() << RESET << std::endl;
                    std::cout << "Size: " << color << pos["size"].get<double>() << RESET << std::endl;
                    std::cout << "Direction: " << color << direction << RESET << std::endl;
                    std::cout << "Average Price: " << YELLOW << pos["average_price"].get<double>() << RESET << std::endl;
                    
                    double pnl = pos["floating_profit_loss"].get<double>();
                    std::string pnlColor = (pnl >= 0) ? GREEN : RED;
                    std::cout << "Floating P/L: " << pnlColor << pnl << RESET << std::endl;
                    std::cout << "───────────────────────────────────────────" << std::endl;
                }
            } else {
                printInfo("No positions found for " + currency);
            }
        } else {
            printError("Failed to retrieve positions.");
            std::cerr << "Response: " << positions.dump(2) << std::endl;
        }
        
        waitForKeyPress();
    }

    void handleInstruments() {
        printSectionHeader("INSTRUMENTS");
        
        std::string currency;
        std::cout << BOLD << "Currency (BTC/ETH): " << RESET;
        std::getline(std::cin, currency);
        
        std::cout << std::endl << BOLD << "Fetching instruments..." << RESET << std::endl;
        json instruments = orderManager.getInstruments(currency);
        
        if (instruments.contains("result")) {
            auto& result = instruments["result"];
            if (result.is_array() && !result.empty()) {
                std::cout << std::endl;
                std::cout << BOLD << "Available Instruments for " << CYAN << currency << RESET << std::endl;
                std::cout << "═════════════════════════════════════════════════════════" << std::endl;
                
                // Table header
                std::cout << BOLD << std::left 
                          << std::setw(25) << "Instrument" 
                          << std::setw(15) << "Min Trade" 
                          << std::setw(15) << "Tick Size" 
                          << std::setw(15) << "Contract Size" 
                          << RESET << std::endl;
                std::cout << "───────────────────────────────────────────────────────────────" << std::endl;
                
                // Table rows
                for (auto& instr : result) {
                    std::cout << std::left 
                              << CYAN << std::setw(25) << instr["instrument_name"].get<std::string>() << RESET
                              << std::setw(15) << instr["min_trade_amount"].get<double>()
                              << std::setw(15) << instr["tick_size"].get<double>()
                              << std::setw(15) << instr["contract_size"].get<double>()
                              << std::endl;
                }
            } else {
                printInfo("No instruments found for " + currency);
            }
        } else {
            printError("Failed to retrieve instruments.");
            std::cerr << "Response: " << instruments.dump(2) << std::endl;
        }
        
        waitForKeyPress();
    }

    void handleConnectWebSocket() {
        printSectionHeader("CONNECT TO WEBSOCKET");
        
        if (marketDataManager.isConnected()) {
            printInfo("Already connected to WebSocket server.");
            waitForKeyPress();
            return;
        }
        
        printInfo("Connecting to WebSocket server...");
        marketDataManager.connect();
        
        // Wait a bit for connection to establish
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        if (marketDataManager.isConnected()) {
            printSuccess("Successfully connected to WebSocket server.");
        } else {
            printError("Failed to connect to WebSocket server.");
        }
        
        waitForKeyPress();
    }

    void handleSubscribeOrderBook() {
        printSectionHeader("SUBSCRIBE TO ORDER BOOK");
        
        if (!marketDataManager.isConnected()) {
            printWarning("Not connected to WebSocket server. Please connect first.");
            waitForKeyPress();
            return;
        }
        
        std::string instrument;
        std::cout << BOLD << "Instrument to subscribe to order book (e.g., BTC-PERPETUAL): " << RESET;
        std::getline(std::cin, instrument);
        
        printInfo("Subscribing to order book updates...");
        bool success = marketDataManager.subscribeOrderBook(instrument);
        
        if (success) {
            printSuccess("Subscribed to order book updates.");
            printInfo("Order book updates will appear in the console.");
            printInfo("Press Enter to return to the main menu (updates will continue in background).");
        } else {
            printError("Failed to subscribe to order book updates.");
        }
        
        waitForKeyPress();
    }

    void handleSubscribeTrades() {
        printSectionHeader("SUBSCRIBE TO TRADES");
        
        if (!marketDataManager.isConnected()) {
            printWarning("Not connected to WebSocket server. Please connect first.");
            waitForKeyPress();
            return;
        }
        
        std::string instrument;
        std::cout << BOLD << "Instrument to subscribe to trades (e.g., BTC-PERPETUAL): " << RESET;
        std::getline(std::cin, instrument);
        
        printInfo("Subscribing to trade updates...");
        bool success = marketDataManager.subscribeTrades(instrument);
        
        if (success) {
            printSuccess("Subscribed to trade updates.");
            printInfo("Trade updates will appear in the console.");
            printInfo("Press Enter to return to the main menu (updates will continue in background).");
        } else {
            printError("Failed to subscribe to trade updates.");
        }
        
        waitForKeyPress();
    }

    void handleViewPerformance() {
        printSectionHeader("PERFORMANCE STATISTICS");
        performanceMonitor.printStatistics();
        waitForKeyPress();
    }
};

// Update the main function to include the new terminal styling
int main() {
    std::cout << BOLD << CYAN << "Starting Deribit Trading Client with performance monitoring..." << RESET << std::endl;
    
    // Start a thread to periodically check memory usage
    std::thread memoryMonitorThread([]() {
        while (true) {
            performanceMonitor.checkMemoryUsage();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    memoryMonitorThread.detach(); // Detach memory monitor thread
    
   
    // Simple CLI demonstration
    TradingCLI tradingCLI(performanceMonitor);
    tradingCLI.run();
    
    // Print performance statistics before exiting
    performanceMonitor.printStatistics();
    
    return 0;
}
