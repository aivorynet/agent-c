# AIVory Monitor C/C++ Agent

Production debugging for C and C++ applications with automatic crash capture and AI-powered analysis.

## Overview

The AIVory Monitor C/C++ Agent captures crashes, segmentation faults, and custom errors in native applications, streams context to the AIVory backend, and enables AI-powered root cause analysis and fix generation. Unlike traditional debuggers, the agent runs in production and captures detailed context without halting execution.

## Requirements

- **CMake**: 3.16 or higher
- **C Compiler**: GCC 7+, Clang 10+, or MSVC 2019+
- **C Standard**: C11
- **Platform**: Linux, macOS, or Windows

### Dependencies

- **libwebsockets**: WebSocket client library
- **jansson**: JSON parsing and generation
- **libunwind** (optional): Improved stack trace capture on Linux/macOS
- **pthread**: POSIX threads (included on Unix-like systems)

#### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y cmake build-essential \
    libwebsockets-dev libjansson-dev libunwind-dev
```

**macOS (Homebrew):**
```bash
brew install cmake libwebsockets jansson libunwind
```

**Fedora/RHEL:**
```bash
sudo dnf install -y cmake gcc make \
    libwebsockets-devel jansson-devel libunwind-devel
```

**Windows (vcpkg):**
```powershell
vcpkg install libwebsockets jansson
```

## Installation

### Building from Source

```bash
# Clone the repository
git clone https://github.com/aivory/aivory-monitor.git
cd aivory-monitor/monitor-agents/agent-c

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Install (optional)
sudo cmake --install .
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `AIVORY_BUILD_SHARED` | ON | Build shared library (.so/.dylib/.dll) |
| `AIVORY_BUILD_STATIC` | ON | Build static library (.a/.lib) |
| `AIVORY_BUILD_EXAMPLES` | OFF | Build example programs |

**Example with options:**
```bash
cmake -DAIVORY_BUILD_SHARED=ON \
      -DAIVORY_BUILD_STATIC=OFF \
      -DAIVORY_BUILD_EXAMPLES=ON \
      ..
```

### Platform-Specific Builds

**Linux/macOS:**
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
```

**Windows (MSVC):**
```powershell
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
cmake --install . --config Release
```

## Usage

### Linking

**CMake:**
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(AIVORY REQUIRED aivory-monitor)

add_executable(myapp main.c)
target_link_libraries(myapp ${AIVORY_LIBRARIES})
target_include_directories(myapp PRIVATE ${AIVORY_INCLUDE_DIRS})
```

**gcc/clang (shared library):**
```bash
gcc -o myapp main.c -laivory-monitor -lwebsockets -ljansson -lpthread
```

**gcc/clang (static library):**
```bash
gcc -o myapp main.c \
    /usr/local/lib/libaivory-monitor.a \
    -lwebsockets -ljansson -lpthread -lunwind
```

### Basic Integration

```c
#include <aivory/monitor.h>
#include <stdio.h>

int main() {
    // Configure the agent
    aivory_config_t config = aivory_config_default();
    config.api_key = "your-api-key-here";
    config.environment = "production";
    config.sampling_rate = 1.0;  // Capture 100% of errors

    // Initialize the agent
    if (aivory_init(&config) != 0) {
        fprintf(stderr, "Failed to initialize AIVory Monitor\n");
        return 1;
    }

    printf("AIVory Monitor initialized\n");

    // Your application code here
    // Signal handlers are automatically installed
    // SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS will be captured

    // Manual error capture
    AIVORY_CAPTURE_ERROR("Something went wrong");

    // Cleanup
    aivory_shutdown();
    return 0;
}
```

### Advanced Usage

**Custom Context:**
```c
// Set user information
aivory_set_user("user-123", "user@example.com", "john_doe");

// Add custom context
aivory_set_context("{\"request_id\": \"req-456\", \"version\": \"1.2.3\"}");

// Capture error with additional context
const char *context = "{\"http_status\": 500, \"endpoint\": \"/api/users\"}";
aivory_capture_error_with_context(
    "Database connection failed",
    __FILE__,
    __LINE__,
    context
);

// Clear user data (e.g., on logout)
aivory_clear_user();
```

**Signal Handler Example:**
```c
#include <aivory/monitor.h>
#include <signal.h>

void dangerous_function() {
    // This will trigger SIGSEGV and be captured by AIVory
    int *null_ptr = NULL;
    *null_ptr = 42;  // Crash!
}

int main() {
    aivory_config_t config = aivory_config_default();
    config.api_key = getenv("AIVORY_API_KEY");
    config.capture_signals = true;  // Enable signal capture (default)

    aivory_init(&config);

    dangerous_function();  // Will be captured and reported

    aivory_shutdown();
    return 0;
}
```

## Configuration

### Configuration Options

| Field | Type | Description | Default |
|-------|------|-------------|---------|
| `api_key` | `const char*` | AIVory API key (required) | NULL |
| `backend_url` | `const char*` | Backend WebSocket URL | `wss://api.aivory.net/ws/agent` |
| `environment` | `const char*` | Environment name | `production` |
| `sampling_rate` | `double` | Error sampling rate (0.0-1.0) | `1.0` |
| `max_capture_depth` | `int` | Maximum variable capture depth | `3` |
| `max_string_length` | `int` | Maximum string length to capture | `1000` |
| `max_collection_size` | `int` | Maximum collection/array size | `100` |
| `debug` | `bool` | Enable debug logging | `false` |
| `capture_signals` | `bool` | Install signal handlers | `true` |

### Environment Variables

Configuration can also be set via environment variables:

| Variable | Description | Default |
|----------|-------------|---------|
| `AIVORY_API_KEY` | API key for authentication | Required |
| `AIVORY_BACKEND_URL` | Backend WebSocket endpoint | `wss://api.aivory.net/ws/agent` |
| `AIVORY_ENVIRONMENT` | Environment name | `production` |
| `AIVORY_SAMPLING_RATE` | Sampling rate (0.0-1.0) | `1.0` |
| `AIVORY_MAX_DEPTH` | Variable capture depth | `3` |
| `AIVORY_DEBUG` | Enable debug logging | `false` |

**Example:**
```bash
export AIVORY_API_KEY="your-key"
export AIVORY_ENVIRONMENT="staging"
export AIVORY_SAMPLING_RATE="0.5"
./myapp
```

## How It Works

### Signal-Based Capture

The agent installs signal handlers for common crash signals:

- **SIGSEGV**: Segmentation fault (null pointer, invalid memory access)
- **SIGABRT**: Abort signal (assertion failure, explicit abort)
- **SIGFPE**: Floating point exception (division by zero)
- **SIGILL**: Illegal instruction
- **SIGBUS**: Bus error (misaligned access)

When a signal is raised, the agent:

1. Captures the signal type and context
2. Generates a stack trace using `backtrace()` or `libunwind`
3. Captures local variables and memory state (if safe)
4. Computes a fingerprint for duplicate detection
5. Sends the exception to the backend via WebSocket
6. Optionally re-raises the signal for default handling

### Backtrace Capture

**Standard mode** (no libunwind):
- Uses `backtrace()` and `backtrace_symbols()` from `execinfo.h`
- Provides function addresses and symbol names
- Limited source file/line information

**Enhanced mode** (with libunwind):
- Uses `libunwind` for more accurate stack unwinding
- Provides function names, source files, and line numbers
- Better handling of optimized code and tail calls
- Enabled automatically when libunwind is detected at build time

### WebSocket Transport

The agent maintains a persistent WebSocket connection to the AIVory backend:

- Automatic reconnection with exponential backoff
- Message queuing during disconnection
- Heartbeat messages to keep connection alive
- Compression support for large payloads

### Thread Safety

All agent functions are thread-safe:

- Signal handlers use async-signal-safe functions only
- WebSocket communication runs in a separate thread
- Internal state is protected with mutexes

## API Reference

### Initialization

```c
aivory_config_t aivory_config_default(void);
int aivory_init(const aivory_config_t *config);
void aivory_shutdown(void);
bool aivory_is_initialized(void);
```

### Error Capture

```c
void aivory_capture_error(const char *message, const char *file, int line);

void aivory_capture_error_with_context(
    const char *message,
    const char *file,
    int line,
    const char *context_json
);

// Convenience macros
AIVORY_CAPTURE_ERROR(msg)
AIVORY_CAPTURE_ERROR_CTX(msg, ctx)
```

### Context Management

```c
void aivory_set_context(const char *context_json);
void aivory_set_user(const char *user_id, const char *email, const char *username);
void aivory_clear_user(void);
```

### Version

```c
#define AIVORY_VERSION_MAJOR 1
#define AIVORY_VERSION_MINOR 0
#define AIVORY_VERSION_PATCH 0
#define AIVORY_VERSION_STRING "1.0.0"
```

## Troubleshooting

### Agent fails to initialize

**Symptom:** `aivory_init()` returns -1

**Solutions:**
- Verify `api_key` is set and valid
- Check network connectivity to backend
- Enable debug logging: `config.debug = true`
- Check environment variables: `AIVORY_API_KEY`

### No crashes are captured

**Symptom:** Crashes occur but aren't sent to backend

**Solutions:**
- Verify signal capture is enabled: `config.capture_signals = true`
- Check that another signal handler isn't overriding AIVory's
- Ensure `aivory_init()` is called before the crash
- Check sampling rate: `config.sampling_rate = 1.0`

### Incomplete stack traces

**Symptom:** Stack traces are missing function names or line numbers

**Solutions:**
- Install libunwind: `sudo apt-get install libunwind-dev`
- Rebuild with libunwind: `cmake -DLIBUNWIND_FOUND=ON ..`
- Compile with debug symbols: `gcc -g ...`
- Disable optimizations for critical code: `gcc -O0 ...`
- Use frame pointers: `gcc -fno-omit-frame-pointer ...`

### WebSocket connection drops

**Symptom:** Agent disconnects frequently

**Solutions:**
- Check firewall rules for WebSocket traffic
- Verify backend URL is correct
- Check network stability
- Enable debug logging to see reconnection attempts

### Build errors

**Missing libwebsockets:**
```
CMake Error: Could not find libwebsockets
```
Solution: `sudo apt-get install libwebsockets-dev`

**Missing jansson:**
```
CMake Error: Could not find jansson
```
Solution: `sudo apt-get install libjansson-dev`

**Compiler too old:**
```
error: C11 required
```
Solution: Upgrade to GCC 7+ or Clang 10+

### Performance impact

**Symptom:** Application slowdown with agent enabled

**Solutions:**
- Reduce sampling rate: `config.sampling_rate = 0.1` (10%)
- Decrease capture depth: `config.max_capture_depth = 1`
- Limit string length: `config.max_string_length = 100`
- Disable signal capture in hot paths (use manual capture only)

## Examples

See the `examples/` directory for complete examples:

- `examples/basic.c` - Basic integration
- `examples/signals.c` - Signal capture demonstration
- `examples/context.c` - Custom context and user tracking
- `examples/cpp.cpp` - C++ integration

Build examples:
```bash
cmake -DAIVORY_BUILD_EXAMPLES=ON ..
make
./examples/basic
```

## License

Copyright (c) 2024 AIVory. All rights reserved.

## Support

- Documentation: https://aivory.net/monitor/
- Issues: https://github.com/aivory/aivory-monitor/issues
- Email: support@aivory.net
