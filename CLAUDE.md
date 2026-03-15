# CLAUDE.md

This file provides guidance for AI assistants working in this repository.

## Project Overview

**Libppcompveri** is a C++ library project (the name suggests "lib" + "pp" (C++) + "comp" + "veri", likely a computation/verification library). The repository is in its initial state — source code, build system, and tests have not yet been added.

## Repository State

- **Language**: C++ (inferred from `.gitignore` patterns covering `.o`, `.so`, `.a`, `.dll`, `.gch`, `.pch`, etc.)
- **Current files**: `README.md`, `.gitignore`
- **Status**: Skeleton — awaiting source code, build system, and tests

## Expected Project Layout (C++ Library Convention)

When the project is developed, the recommended layout is:

```
Libppcompveri/
├── include/               # Public header files (.h / .hpp)
│   └── libppcompveri/
├── src/                   # Implementation files (.cpp)
├── tests/                 # Unit and integration tests
├── examples/              # Usage examples
├── docs/                  # Documentation
├── CMakeLists.txt         # Primary build file (or Makefile)
├── .gitignore
├── README.md
└── CLAUDE.md
```

## Build System

No build system is configured yet. For a C++ library, common choices are:

- **CMake** (recommended): `cmake -B build && cmake --build build`
- **Make**: `make` / `make all`
- **Meson**: `meson setup build && ninja -C build`

When a build system is added, update this file with the exact commands.

## Development Workflow

### Branching

- Main branch: `main`
- Feature branches follow the pattern: `claude/<description>-<id>` (e.g., `claude/add-claude-documentation-YfPSj`)
- Always develop on the designated feature branch; never push directly to `main`

### Making Changes

1. Ensure you are on the correct feature branch before editing
2. Write code following the conventions described below
3. Build and test before committing
4. Commit with clear, descriptive messages
5. Push with: `git push -u origin <branch-name>`

### Commit Messages

Use concise imperative-style messages:
- `Add matrix multiplication implementation`
- `Fix overflow in integer verification routine`
- `Refactor parser to reduce code duplication`

## C++ Conventions

Since no source code exists yet, follow these standard C++ best practices when code is added:

### Code Style
- Standard: **C++17** or later unless specified otherwise
- Indentation: **4 spaces** (no tabs)
- Naming:
  - Classes/structs: `PascalCase`
  - Functions/methods: `snake_case`
  - Member variables: `snake_case_` (trailing underscore for private members)
  - Constants/enums: `UPPER_SNAKE_CASE` or `kCamelCase`
  - Namespaces: `snake_case`
- Header guards: Use `#pragma once` (preferred over `#ifndef` guards)
- File extensions: `.cpp` for implementation, `.hpp` or `.h` for headers

### Code Quality
- Prefer RAII and smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw pointers
- Avoid undefined behavior; use sanitizers (`-fsanitize=address,undefined`) during development
- Mark functions `noexcept` where appropriate
- Use `const` correctness throughout
- Avoid global mutable state

### Testing
- Place tests under `tests/`
- Common frameworks: **Google Test (gtest)**, **Catch2**, or **doctest**
- When tests are added, the command to run them will be documented here

## .gitignore Coverage

The `.gitignore` already covers all standard C++ build artifacts:
- Object files: `*.o`, `*.obj`, `*.slo`, `*.lo`
- Libraries: `*.a`, `*.lib`, `*.so`, `*.dylib`, `*.dll`
- Precompiled headers: `*.gch`, `*.pch`
- Executables: `*.exe`, `*.out`, `*.app`
- Debug info: `*.pdb`, `*.dwo`
- Dependency files: `*.d`

Do not commit build artifacts or generated files.

## Updating This File

This file should be kept up to date as the project evolves. When any of the following change, update the relevant section:

- Build system commands are established
- Testing framework is chosen and configured
- Code style decisions are made explicit
- CI/CD pipelines are added
- New major directories or components are introduced
