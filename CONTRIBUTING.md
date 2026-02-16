# Contributing to AIVory Monitor C/C++ Agent

Thank you for your interest in contributing to the AIVory Monitor C/C++ Agent. Contributions of all kinds are welcome -- bug reports, feature requests, documentation improvements, and code changes.

## How to Contribute

- **Bug reports**: Open an issue at [GitHub Issues](https://github.com/aivorynet/agent-c/issues) with a clear description, steps to reproduce, and your environment details (compiler version, OS, architecture).
- **Feature requests**: Open an issue describing the use case and proposed behavior.
- **Pull requests**: See the Pull Request Process below.

## Development Setup

### Prerequisites

- GCC or Clang (C11 support required)
- CMake 3.16 or later
- libunwind (for stack unwinding)

### Build and Test

```bash
cd monitor-agents/agent-c
cmake -B build
cmake --build build
```

To run tests (if a test target is defined):

```bash
cmake --build build --target test
```

### Running the Agent

Link the agent library into your application or use `LD_PRELOAD` to attach at runtime. See the README for integration details.

## Coding Standards

- Follow the existing code style in the repository.
- Write tests for all new features and bug fixes.
- All code must be free of memory leaks -- use Valgrind or AddressSanitizer to verify.
- Keep signal handler and eBPF code minimal, async-signal-safe where required, and well-documented.
- Ensure cross-platform compatibility (Linux, macOS) where possible.

## Pull Request Process

1. Fork the repository and create a feature branch from `main`.
2. Make your changes and write tests.
3. Ensure the build succeeds on your platform (`cmake -B build && cmake --build build`).
4. Submit a pull request on [GitHub](https://github.com/aivorynet/agent-c) or GitLab.
5. All pull requests require at least one review before merge.

## Reporting Bugs

Use [GitHub Issues](https://github.com/aivorynet/agent-c/issues). Include:

- Compiler and version, OS, and architecture
- Agent version
- Crash logs, core dumps, or error output
- Minimal reproduction steps

## Security

Do not open public issues for security vulnerabilities. Report them to **security@aivory.net**. See [SECURITY.md](SECURITY.md) for details.

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
