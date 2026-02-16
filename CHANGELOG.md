# Changelog

All notable changes to the AIVory Monitor C/C++ Agent will be documented in this file.

This project adheres to [Semantic Versioning](https://semver.org/).

## [0.1.0] - 2026-02-16

### Added
- Signal handlers for SIGSEGV, SIGABRT, SIGFPE, SIGBUS, and SIGILL
- Backtrace capture with optional libunwind integration
- Local variable capture at crash sites
- WebSocket transport via libwebsockets
- JSON serialization via jansson
- Shared and static library build targets
- CMake build system with cross-platform support
- Configurable sampling rate and capture depth
- Environment variable configuration
- C11 standard compliance
