# Contributing to RabbitBroker

Thank you for your interest in contributing to RabbitBroker! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Code Style Guide](#code-style-guide)
- [Commit Message Guidelines](#commit-message-guidelines)
- [Pull Request Process](#pull-request-process)
- [Testing Requirements](#testing-requirements)
- [Performance Guidelines](#performance-guidelines)
- [Documentation Standards](#documentation-standards)
- [Reporting Bugs](#reporting-bugs)
- [Suggesting Enhancements](#suggesting-enhancements)

## Code of Conduct

We are committed to providing a welcoming and inclusive environment for all contributors. Please:

- Be respectful and professional in all interactions
- Welcome diverse perspectives and experiences
- Focus on constructive feedback
- Report unacceptable behavior to the maintainers

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
    ```bash
    git clone https://github.com/Rnneb-Broker/core.git
    cd rabbit-broker
    ```
3. **Create a branch** for your work:
    ```bash
    git checkout -b feature/your-feature-name
    # or
    git checkout -b fix/your-bug-fix
    ```
4. **Make your changes** and commit regularly
5. **Push to your fork** and submit a Pull Request

## Development Setup

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15 or later
- Conan 2.0+
- Git

### Build for Development

```bash
cd rabbit-broker
mkdir -p build && cd build

# Install dependencies
conan install .. --build=missing

# Configure with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Build
make -j$(nproc)

# Run tests
ctest --verbose
```

### Development with IDE

**VSCode**:

```bash
# Install CMake Tools extension
# Open folder in VSCode - will auto-detect CMake project
# Use CMake: Configure from command palette
# Use CMake: Build for compilation
```

**CLion**:

```bash
# Open the folder as a CMake project
# CLion will auto-detect and configure
```

## Code Style Guide

### C++ Style

We follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with minor modifications:

#### Naming Conventions

- **Classes**: `PascalCase` (e.g., `StorageManager`, `Session`)
- **Functions**: `snake_case` (e.g., `append_message()`, `on_connect()`)
- **Variables**: `snake_case` (e.g., `buffer_size`, `is_connected`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_BUFFER_SIZE`, `DEFAULT_PORT`)
- **Private Members**: `snake_case_` suffix (e.g., `session_mutex_`, `running_`)
- **Namespaces**: `lowercase` (e.g., `namespace rabbit`)

#### Code Formatting

```cpp
// Header guards (prefer pragma once)
#pragma once

// Includes (grouped by: std lib, external libs, project)
#include <vector>
#include <memory>

#include <boost/asio.hpp>

#include "broker/broker.hpp"

namespace rabbit {

// Class declaration
class MyClass {
public:
    // Public methods
    void public_method();

    // Public members (rarely used)
    int public_member;

protected:
    // Protected methods
    void protected_method();

private:
    // Private members
    int private_member_;
    std::mutex mutex_;

    // Private methods
    void private_method();
};

}  // namespace rabbit
```

#### Formatting Rules

- **Indentation**: 2 spaces (no tabs)
- **Line Length**: Maximum 100 characters (aim for 80)
- **Braces**: Allman style for method definitions
    ```cpp
    void MyClass::method()
    {
        // body
    }
    ```
- **Comments**: Use `//` for single-line, `/* */` for multi-line
- **Documentation**: Use inline comments sparingly; prefer clear code

#### Modern C++ Guidelines

- Prefer `std::make_shared` and `std::make_unique`
- Use `auto` for cleaner code where type is obvious
- Prefer references over pointers when ownership is clear
- Use move semantics where appropriate
- Delete copy constructors when not needed:
    ```cpp
    MyClass(const MyClass&) = delete;
    MyClass& operator=(const MyClass&) = delete;
    ```
- Use `constexpr` for compile-time constants
- Prefer range-based for loops

#### Memory Safety

- **RAII**: Use smart pointers, avoid raw `new`/`delete`
- **Thread Safety**: Use `std::mutex`, `std::shared_mutex`, `std::atomic`
- **Exception Safety**: Mark functions that don't throw with `noexcept`
- **Resource Cleanup**: Use destructors and RAII classes

### Example Compliant Code

```cpp
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace rabbit {

class MessageBuffer {
public:
    explicit MessageBuffer(size_t capacity = 1024 * 1024);
    ~MessageBuffer();

    // Non-copyable
    MessageBuffer(const MessageBuffer&) = delete;
    MessageBuffer& operator=(const MessageBuffer&) = delete;

    // Movable
    MessageBuffer(MessageBuffer&&) noexcept = default;
    MessageBuffer& operator=(MessageBuffer&&) noexcept = default;

    bool append(const std::vector<uint8_t>& data);
    size_t size() const { return size_.load(); }
    bool is_full() const { return size_.load() >= capacity_; }

private:
    std::vector<uint8_t> buffer_;
    size_t capacity_;
    std::atomic<size_t> size_{0};
    mutable std::mutex mutex_;
};

}  // namespace rabbit
```

## Commit Message Guidelines

### Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Type

- `feat`: A new feature
- `fix`: A bug fix
- `docs`: Documentation only changes
- `style`: Changes that don't affect code meaning (formatting, etc.)
- `refactor`: Code change that neither fixes a bug nor adds a feature
- `perf`: Code change that improves performance
- `test`: Adding or updating tests
- `chore`: Changes to build system or dependencies

### Scope

The scope should specify what part of the codebase is affected:

- `broker`
- `client`
- `storage`
- `protocol`
- `cmake`
- etc.

### Subject

- Use imperative mood ("add" not "added" or "adds")
- Don't capitalize first letter
- No period (.) at the end
- Limit to 50 characters

### Body

- Explain **what** and **why**, not how
- Wrap at 72 characters
- Separate from subject with blank line
- Use bullet points for multiple changes

### Footer

- Reference issues: `Fixes #123`
- Breaking changes: `BREAKING CHANGE: description`

### Examples

```
feat(storage): add sparse index for message lookup

Implement sparse indexing with one entry per 1024 messages
to enable O(log N) message lookup during replay operations.
Reduces memory overhead while maintaining fast access.

Fixes #42
```

```
fix(client): prevent double connect on reconnect

The client could create duplicate connections when
reconnect was called before the initial connection
completed. Add state check before creating new socket.

Fixes #128
```

## Pull Request Process

### Before Submitting

1. **Update from main**: `git rebase origin/main`
2. **Run all tests**: `ctest --verbose`
3. **Check code style**: Verify formatting matches guidelines
4. **Update documentation**: If adding features, update README/docs
5. **Add tests**: New features must have corresponding tests
6. **Run static analysis**: Check for common issues

### Submitting a PR

1. **Clear title**: Summarize the change
    - Good: "Add sparse index for O(log N) message lookup"
    - Bad: "Update code", "Fix stuff"

2. **Detailed description**: Include
    - What problem does this solve?
    - How does the solution work?
    - Any breaking changes?
    - Related issues: `Fixes #123`, `Related to #456`

3. **Screenshots/examples**: For UI changes, show before/after

### PR Template

```markdown
## Description

Brief description of the changes.

## Related Issues

Fixes #(issue number)

## Type of Change

- [ ] Bug fix (non-breaking)
- [ ] New feature (non-breaking)
- [ ] Breaking change
- [ ] Documentation update

## Testing

Describe the tests you added/modified.

## Checklist

- [ ] Tests pass locally
- [ ] Code follows style guide
- [ ] Documentation is updated
- [ ] No new warnings generated
- [ ] Commits are well-described
```

### Review Process

- **At least 2 approvals** required for merge
- **All CI checks** must pass
- **No merge conflicts** allowed
- Address all feedback before re-requesting review

## Testing Requirements

### Unit Tests

All new code must include unit tests:

```cpp
#include <gtest/gtest.h>
#include "broker/storage_manager.hpp"

namespace rabbit {

class StorageManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

TEST_F(StorageManagerTest, AppendMessageSucceeds) {
    // Arrange
    StorageManager manager;
    std::vector<uint8_t> payload = {1, 2, 3};

    // Act
    bool result = manager.append_message(0, payload);

    // Assert
    EXPECT_TRUE(result);
}

}  // namespace rabbit
```

### Test Coverage

- Aim for >80% code coverage for new code
- Test both happy path and error cases
- Include edge cases and boundary conditions
- Test thread safety for concurrent code

### Running Tests

```bash
# Run all tests
ctest --verbose

# Run specific test
ctest -R StorageManagerTest --verbose

# Run with output on failure
ctest --output-on-failure

# Generate coverage report
cd build
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make coverage
# Open coverage report in build/coverage/index.html
```

## Performance Guidelines

### Benchmarking

For performance-critical code:

```cpp
#include <chrono>

// Measure execution time
auto start = std::chrono::high_resolution_clock::now();

// Code to benchmark
function_under_test();

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Time: " << duration.count() << " us\n";
```

### Performance Requirements

- **Append path**: Must remain O(1) with no dynamic allocation
- **Message lookup**: Should be O(log N) via sparse indexing
- **Subscriber notification**: Should complete in <1ms for <1000 subscribers
- **Write latency**: Sub-millisecond for 90th percentile

### Profiling

```bash
# Build with profiling support
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_PROFILING=ON ..
make

# Run with perf
perf record -g ./broker
perf report

# Or use flame graphs
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

## Documentation Standards

### Code Documentation

- **Public APIs**: Use Doxygen-style comments
- **Complex logic**: Explain the "why" not the "what"
- **Assumptions**: Document preconditions and postconditions

```cpp
/**
 * Appends a message to the buffer with CRC validation.
 *
 * @param offset     Message offset (monotonically increasing)
 * @param payload    Message data
 * @return true if message was appended, false if buffer full
 *
 * @note Thread-safe for single writer, multiple readers
 * @note O(1) operation with no memory allocation
 *
 * @throws std::invalid_argument if offset not monotonic
 */
bool append_message(uint64_t offset, const std::vector<uint8_t>& payload);
```

### File Documentation

Every source file should start with:

```cpp
/**
 * @file storage_manager.cpp
 * @brief Persistent message storage with double-buffering
 *
 * Implements segment-based append-only logs with configurable
 * durability and automatic retention cleanup.
 */
```

### Documentation Updates

- Update README.md for user-facing changes
- Update ARCHITECTURE.md for design changes
- Add CHANGELOG entry for notable changes
- Update API docs for public API changes

## Reporting Bugs

### Before Reporting

1. Check existing issues - your bug might already be reported
2. Reproduce consistently - note exact steps
3. Identify version - which commit/release?
4. Check environment - OS, compiler, dependencies

### Bug Report Template

```markdown
## Description

Clear, concise description of the bug.

## Environment

- OS: (e.g., Ubuntu 20.04)
- Compiler: (e.g., GCC 9.3)
- Branch: (e.g., main, develop)
- Commit: (e.g., abc1234)

## Reproduction Steps

1. Step one
2. Step two
3. ...

## Expected Behavior

What should happen?

## Actual Behavior

What actually happens?

## Logs/Error Messages

Relevant output, stack traces, etc.

## Minimal Reproducible Example

Simplest code that reproduces the issue.
```

## Suggesting Enhancements

### Enhancement Suggestion Template

```markdown
## Is your feature request related to a problem?

Describe the problem it solves.

## Describe the Solution

Clear description of what you want to happen.

## Describe Alternatives

Other solutions or workarounds you've considered.

## Additional Context

Any other context, mockups, or examples.

## Acceptance Criteria

How would you know this feature is complete?
```

## Questions?

- **Questions about contributing**: Open a discussion in GitHub Discussions
- **Need help getting started**: Comment on a good-first-issue
- **Direct questions**: Tag @maintainers in an issue

## Additional Resources

- [Modern C++ Guidelines](https://github.com/isocpp/CppCoreGuidelines)
- [Boost C++ Libraries](https://www.boost.org/doc/)
- [CMake Best Practices](https://cmake.org/cmake/help/latest/)
- [Git Workflow](https://git-scm.com/book/en/v2)

---

Thank you for contributing! Your work helps make RabbitBroker better for everyone. 🎉
