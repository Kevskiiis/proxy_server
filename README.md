# CS Guardian – Proxy Server

## Project Overview
CS Guardian is a proxy server designed to give users control over which websites can be accessed on their machine. It acts as a filtering and forwarding layer for HTTP/HTTPS traffic, while also attempting to mask the client’s IP address during requests.

The primary goal of this project is to explore low-level networking concepts and implement a functional proxy using the C programming language.

---

## Core Concepts and Themes

### Client-Server Architecture
- The proxy operates as an intermediary between the client and external servers.
- All requests are routed through the proxy before reaching their destination.

### Networking Fundamentals
- Implemented TCP connections using sockets.
- Managed communication between processes across a network.

### Abstraction vs. Control
- C provides fine-grained control over system resources.
- This comes at the cost of reduced abstraction and increased complexity.

### Delegation of Complexity
- Used `curl` to handle HTTP requests instead of implementing the full HTTP protocol manually.
- This allowed us to focus on proxy logic rather than protocol details.

### Modularity
- Separated responsibilities such as:
  - Connection handling
  - Request forwarding
- This improves readability and maintainability.

---

## Design Decisions and Trade-offs

### Design Decisions
- Built entirely in C (project requirement).
- Used low-level socket programming for networking.
- Leveraged `curl` for HTTP request handling.
- Focused on implementing core proxy functionality rather than a full-featured solution.

### Trade-offs

#### Pros
- Full control over networking behavior due to C.
- Simplified HTTP handling through `curl`.

#### Cons
- Mixing low-level socket code with external tools (curl) introduces inconsistency.
- Less flexibility compared to a fully custom HTTP implementation.
- HTTPS traffic cannot be fully inspected or modified due to lack of certificates for decryption.
  - As a result, the client’s IP address cannot be completely masked.

### Comparison to Other Languages
- **Python**: Provides high-level HTTP libraries (e.g., `requests`).
- **Node.js**: Includes built-in HTTP server capabilities.
- **C**: Offers maximum control but requires significantly more manual implementation.

---

## Challenges and Lessons Learned

### Socket Programming
- Learned how to create and manage sockets for inter-process communication over a network.
- Faced challenges with:
  - Handling byte sizes correctly
  - Determining when to open and close connections

### HTTP/HTTPS Handling
- Implemented request forwarding for HTTP and HTTPS traffic.
- Encountered limitations with HTTPS:
  - Unable to decrypt traffic without proper certificates
  - Could not fully remove or mask client IP information

### Working with C
- Building a client-server architecture in C introduced additional complexity.
- Required careful handling of low-level details such as:
  - Memory management
  - Data transfer sizes
  - Buffer control

---

## Summary
CS Guardian demonstrates the fundamentals of building a proxy server using low-level networking in C. While the implementation is simplified, it highlights key trade-offs between control and abstraction, and provides hands-on experience with real-world networking concepts.