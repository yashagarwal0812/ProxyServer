# C++ HTTP Proxy Server

A multi-threaded HTTP proxy server implementation in C++ with caching capabilities. This proxy server acts as an intermediary between clients and web servers, handling HTTP GET requests while providing caching functionality to improve response times.

## Features

- Multi-threaded request handling
- LRU (Least Recently Used) caching system
- Support for HTTP/1.0 and HTTP/1.1 GET requests
- Thread-safe cache implementation using mutex locks
- Configurable cache size and client limits
- RAII-compliant resource management
- Error handling and logging

## Architecture

### 1. Core Components

#### ProxyServer Class
- Manages the main server operations
- Handles client connections
- Creates worker threads for request processing
- Maintains the server socket and connection queue

#### ProxyCache Class
- Implements the caching system
- Uses LRU (Least Recently Used) replacement policy
- Thread-safe operations using mutex locks
- Manages cache size limits

#### CacheElement Class
- Represents individual cache entries
- Stores URL, response data, and metadata
- Tracks access times for LRU implementation

### 2. Threading Model

The server uses a thread-per-connection model:
- Main thread accepts incoming connections
- Each client request is handled by a separate worker thread
- Semaphore controls the maximum number of concurrent connections
- Thread-safe cache access using mutex locks

## Configuration

Key configurable parameters (defined as macros):

```cpp
#define MAX_BYTES 4096          // Maximum request/response size
#define MAX_CLIENTS 400         // Maximum concurrent connections
#define MAX_SIZE 200*(1<<20)    // Cache size (200MB)
#define MAX_ELEMENT_SIZE 10*(1<<20)  // Maximum cache entry size (10MB)
```

## Working Mechanism

### 1. Request Processing Flow

1. **Connection Acceptance**
   - Server listens on specified port
   - Accepts incoming client connections
   - Creates new thread for each connection

2. **Request Parsing**
   - Reads HTTP request from client
   - Parses request headers and URL
   - Validates HTTP method (only GET supported)

3. **Cache Checking**
   - Checks if requested URL exists in cache
   - If found, returns cached response
   - Updates LRU tracking information

4. **Remote Server Communication**
   - If not in cache, connects to remote server
   - Forwards client request
   - Receives server response

5. **Response Handling**
   - Caches new responses (if applicable)
   - Sends response back to client
   - Closes connection

### 2. Caching Mechanism

The cache implements LRU (Least Recently Used) policy:
- New entries are added to the front of the cache
- When cache is full, least recently used entries are removed
- Each access updates the entry's timestamp
- Thread-safe operations ensure cache consistency

### 3. Error Handling

The server handles various error conditions:
- Invalid requests (400 Bad Request)
- Server errors (500 Internal Server Error)
- Connection failures
- Memory allocation failures
- Invalid HTTP versions (505 HTTP Version Not Supported)

## Usage

1. **Compilation**
   ```bash
   g++ -o with_cache with_cache.cpp -pthread
   ```

2. **Running the Server**
   ```bash
   ./with_cache <port_number>
   ```

3. **Configure Browser**
   - Set proxy settings in your browser:
     - Host: localhost
     - Port: <specified_port_number>

## Performance Considerations

- **Threading**: Each connection spawns a new thread, suitable for moderate traffic
- **Cache Size**: Configurable based on available memory
- **Connection Limits**: Prevents resource exhaustion
- **Mutex Locks**: Minimizes lock contention while ensuring thread safety

## Limitations

- Only supports HTTP GET requests
- No HTTPS support
- Basic caching strategy (LRU only)
- No compression support
- No authentication mechanism

## Future Improvements

1. Add HTTPS support
2. Implement more HTTP methods (POST, PUT, etc.)
3. Add support for compressed responses
4. Implement more sophisticated caching strategies
5. Add configuration file support
6. Implement connection pooling
7. Add support for proxy authentication
8. Improve logging and monitoring capabilities

## Dependencies

- POSIX Threads (pthread)
- Standard C++ Library
- POSIX networking APIs

## System Requirements

- POSIX-compliant operating system (Linux, Unix, macOS)
- C++11 or higher compiler
- pthread library

## License

This project is open source and available under the MIT License.