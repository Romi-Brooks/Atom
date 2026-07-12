# Contributing to Atom Engine  

[English](CONTRIBUTING.md) | [中文](Docs/CONTRIBUTING-CN.md)

---
First off, thank you for considering contributing to Atom Engine! We welcome contributions of all forms — bug reports, feature suggestions, documentation improvements, and code changes.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How to Contribute](#how-to-contribute)
- [Coding Standards](#coding-standards)
- [Pull Request Process](#pull-request-process)
- [Commit Messages](#commit-messages)
- [Reporting Issues](#reporting-issues)

---

## Code of Conduct

This project is committed to providing a welcoming and inclusive environment for everyone. By participating, you agree to:

- Use welcoming and inclusive language
- Be respectful of differing viewpoints and experiences
- Accept constructive criticism gracefully
- Focus on what is best for the community

---

## How to Contribute

### Reporting Bugs

1. Check the [issue tracker](https://github.com/RomiBrooks/Atom/issues) to avoid duplicates
2. Provide a clear, descriptive title
3. Include the following details:
   - OS and compiler version
   - CMake configuration flags used
   - Steps to reproduce
   - Expected vs actual behavior
   - Relevant logs or error output

### Suggesting Features

1. Describe the feature and the problem it solves
2. Explain how it fits into the engine's architecture
3. If possible, sketch an API or usage example

### Documentation

Documentation improvements are always welcome — typos, unclear sections, missing translations, or new guides.

---

## Coding Standards

All code **must** conform to the project's coding standard. Key points:

- **Trailing return types**: `auto Func() -> ReturnType` (mandatory)
- **Naming**: PascalCase for classes/functions, `snake_case_` for members, `camelCase` for locals/params
- **Indentation**: Tabs, Allman brace style
- **Namespaces**: `snake_case`, top-level `atom`
- **Include guard**: `#ifndef ATOM_<NAME>_HPP`
- **Include order**: Standard → Third Party → Project (`<>`) → Self (`""`)

See the full standards:
- [English](CODING_STANDARD.md)
- [中文](Docs/CODING_STANDARD-CN.md)

### Pre-commit Checklist

- [ ] Code compiles with no warnings
- [ ] Follows trailing return type convention
- [ ] Member variables use `snake_case_` trailing underscore
- [ ] No `m_` prefix or leading underscore
- [ ] Include guards use `ATOM_<NAME>_HPP` format
- [ ] Includes are in the correct order
- [ ] Uses Allman brace style with tab indentation
- [ ] No absolute paths in `#include` directives

---

## Branch Strategy

- **`master`** — Stable beta releases. Updated only by merging from `dev`.
- **`dev`** — Active development. Feature branches fork from here.
- **`feature/*`** — New features, fork from `dev`, merge back to `dev`.

## Pull Request Process

All contributors use the fork workflow:

```bash
# 1. Fork the repository on GitHub

# 2. Clone your fork and switch to dev
git clone https://github.com/<your-username>/Atom.git
cd Atom
git checkout dev

# 3. Create a feature branch from dev
git checkout -b feature/<feature-name>

# 4. Make changes, follow the coding standard

# 5. Commit and push to your fork
git add -A
git commit -m "feat: brief description"
git push origin feature/<feature-name>

# 6. Open a Pull Request on GitHub
#    From: your fork's feature/<feature-name>
#    To:   upstream repository's dev branch
```

### Before Submitting

- Keep commits focused — each commit should represent a single logical change
- Rebase onto the latest `dev` before submitting:
  ```bash
  git fetch origin
  git rebase origin/dev
  ```
- PR must include: clear title, description of changes, and any related issue (e.g. `Closes #123`)
   - A clear title describing the change
   - A description explaining what was changed and why
   - Reference to any related issues (e.g., `Closes #123`)

7. **Address review feedback**. Maintainers may request changes before merging.

---

## Commit Messages

Follow the [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>: <short description>

<optional body>
```

**Types:**

| Type | Usage |
|---|---|
| `feat` | New feature |
| `fix` | Bug fix |
| `refactor` | Code restructuring |
| `docs` | Documentation only |
| `style` | Formatting, indentation, etc. |
| `build` | CMake or build system changes |
| `chore` | Maintenance tasks |

**Examples:**

```
feat: add Vec2 math type with basic arithmetic operators

fix: correct include path for Vec2 in RenderWindow.hpp

docs: add CONTRIBUTING.md with development guide

build: add engine_math library target to CMake
```

---

## Reporting Issues

When opening an issue, consider including the following information:

1. **Environment**: OS, compiler, CMake version
2. **Steps to reproduce**: Minimal, complete, and verifiable
3. **Expected behavior**: What you expected to happen
4. **Actual behavior**: What actually happened (include logs)
5. **Possible fix**: If you have an idea of what might be wrong

---

*Thank you for helping make Atom Engine better!*
