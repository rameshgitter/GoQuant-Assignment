# GoQuant-Assignment

# High-Performance Order Execution and Management System

## Objective
Develop a **high-performance order execution and management system** in **C++** to trade on [Deribit Testnet](https://test.deribit.com/).

---

## Initial Setup
1. Create a new **Deribit Test** account.
2. Generate **API Keys** for authentication.

---

## Core Requirements

### Order Management Functions
- **Place Order**
- **Cancel Order**
- **Modify Order**
- **Get Orderbook**
- **View Current Positions**

### Real-Time Market Data Streaming
- Implement a **WebSocket server**:
  - Manage connections
  - Handle client subscriptions to symbols
  - Stream **continuous orderbook updates** for subscribed symbols

### Market Coverage
- **Instruments:** Spot, Futures, and Options
- **Scope:** All supported symbols on Deribit Testnet

---

## Technical Requirements
- Full implementation in **C++**
- Demonstrate **low-latency** performance
- Proper **error handling** and **logging**
- WebSocket server features:
  - Connection management
  - Subscription handling
  - Efficient message broadcasting

---

## Bonus Section (Recommended)

### Performance Analysis and Optimization

#### Latency Benchmarking
Measure and document:
- **Order Placement Latency**
- **Market Data Processing Latency**
- **WebSocket Message Propagation Delay**
- **End-to-End Trading Loop Latency**

#### Optimization Areas
Apply and justify optimization techniques for:
- **Memory Management**
- **Network Communication**
- **Data Structure Selection**
- **Thread Management**
- **CPU Optimization**

### Documentation Requirements for Bonus Section
- Detailed **analysis of bottlenecks**
- **Benchmarking methodology** explanation
- **Before/After** performance metrics
- Justification for **optimization choices**
- Discussion on **potential further improvements**

---

## Deliverables
- Complete **source code** with documentation
- **Video recording** demonstrating:
  - System functionality
  - Code review
  - Implementation explanation
- If bonus section completed:
  - **Performance analysis report**
  - **Benchmarking results**
  - **Optimization documentation**

---

## References
- [Deribit API Documentation](https://docs.deribit.com/)

---

