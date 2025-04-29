# GoQuant-Assignment
## C++ Backend Developer Intern at GoQuant

**Stipend:** ₹20,000 to ₹35,000 per month

### 📅 Timeline:

- **Application to view:** ~1 week  
- **Assignment received:** ~3 week after application  
- **Assignment deadline:** 1 week to complete  
- **Response and interview scheduling:** Took ~3 weeks  

---

### 🧠 Interview Process:

#### 🔹 Round 1 – Technical + Resume-Based Interview (30 minutes)

- **Interviewer 1 (Finance-oriented):**
  - What is Deribit?
  - What is a dividend?
  - Other finance-related concepts
  - Questions related to my resume

- **Interviewer 2 (General/Behavioral):**
  - What is the most difficult course you encountered in your college curriculum, and how did you handle it?
  - If you had unlimited resources and time, what would you build?

- **Interviewer 3 (Technical – DS & Assignment Focused):**
  1. For an order book, which data structure would you prefer: `unordered_map` or a sorted array? Why?
  2. What is the time complexity of `unordered_map`? How and why can it increase from O(1) to O(n)?
  3. What was the most difficult part of the assignment?
  4. What was the most bug-prone part of the assignment?
  5. What are the drawbacks of the `chrono` library you used?

- **Outcome:** ❌ Rejected after this round  
  - However, they encouraged me to consider joining their bootcamp.
  - They sent a recorded video of the google meet interview using Spinach ai after the interview ended.

---
_____________________________________________________________________________________
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

