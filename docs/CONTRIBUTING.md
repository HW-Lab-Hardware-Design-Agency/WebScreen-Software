# Contributing to WebScreen

Thank you for considering contributing to WebScreen! Whether you’re a seasoned developer or new to embedded development on the ESP32, your contributions help make this project better for everyone. This document will guide you through the various ways you can contribute, explain the pull request process, and detail our commit message guidelines.

## Introduction

WebScreen is an open-source ESP32-S3 based platform that runs dynamic JavaScript applications using the Elk engine and LVGL for rich user interfaces. The system features a modular architecture with robust SD card support, WiFi connectivity, MQTT integration, and a comprehensive JavaScript API for building embedded applications.

Your contributions—whether they’re bug fixes, new features, documentation improvements, or testing—are highly welcome.

## Ways to Contribute

Here are some of the ways you can get involved:

- **Spread the Word:** Share your WebScreen experiences on social media, blogs, or community forums.
- **Report Issues:** If you find bugs or unexpected behavior, please open an issue in our GitHub repository.
- **Suggest Features:** Have an idea? Open an issue or join our discussion forums to share your ideas.
- **Improve Documentation:** Help us enhance the user guides, API reference, and examples.
- **Submit Code:** Whether it’s fixing a small bug or implementing a new feature, code contributions are always welcome.
- **Review Pull Requests:** Offer feedback on others’ contributions to help ensure quality and consistency.

## Pull Request Process

All contributions are merged via pull requests (PRs). Here’s a brief overview of the process:

### From GitHub (for small changes)

1. Navigate to the file you want to edit.
2. Click the **Edit** button.
3. Make your changes and add a clear commit message.
4. Click **Propose changes** to open a pull request.

### From the Command Line (for larger contributions)

1. **Fork** the WebScreen repository on GitHub.
2. **Clone** your fork locally.
3. Create a new branch for your changes:
   ```bash
   git checkout -b my-feature-branch
   ```
4. Make your changes and commit them (see commit message guidelines below).
5. Push your branch to your fork:
   ```bash
   git push origin my-feature-branch
   ```
6. Open a pull request from your fork’s branch to the master branch of the main repository.
7. Provide a clear description of your changes and reference any related issues.
8. If further changes are required, simply add more commits to your branch—they will automatically appear in the PR.

## Commit Message Guidelines

We follow the [Conventional Commits v1.0.0](https://www.conventionalcommits.org/en/v1.0.0/) specification for commit messages. Each commit message should have the following structure:

```
<type>(<scope>): <subject>

<body>

<footer>
```

- **Type:** A short string indicating the nature of the commit, for example:
  - `feat` — A new feature
  - `fix` — A bug fix
  - `docs` — Documentation only changes
  - `style` — Changes that do not affect the meaning of the code (formatting, etc.)
  - `refactor` — A code change that neither fixes a bug nor adds a feature
  - `test` — Adding or updating tests
  - `chore` — Minor changes, build process updates, etc.
  
- **Scope:** _(Optional)_  
  A short description of the section or module that was affected (e.g., `wifi`, `ui`, `mqtt`).

- **Subject:**  
  A brief, imperative summary of the changes (e.g., “add support for custom app selection”).

- **Body:** _(Optional)_  
  A more detailed explanation of the commit. This section should be separated by a blank line.

- **Footer:** _(Optional)_  
  Can include information about breaking changes (prefixed with `BREAKING CHANGE:`) or references to related issues (e.g., `Closes #123`).

**Examples:**

- `feat(wifi): add fallback to secure HTTPS when no CA certificate is loaded`
- `fix(ui): resolve label alignment issue on landscape mode`
- `docs: update contributing guidelines based on Conventional Commits`

## Developer Certification of Origin (DCO)

By contributing to WebScreen, you agree that your contributions are made under the terms of our license and that you have the right to submit them. When you submit a pull request, please include a statement in your commit messages such as:

```
Signed-off-by: Your Name <youremail@example.com>
```

For more information, please read the [Developer Certificate of Origin](https://developercertificate.org/).

## Getting Started

If you’re new to the project:

1. **Set Up Your Development Environment:**  
   Follow the instructions in the [Quick Start](#quick-start) section of the README.md.
2. **Review the Code:**  
   Familiarize yourself with the core modules (e.g., dynamic JavaScript execution, fallback UI, SD card file handling).
3. **Join the Community:**  
   If you have questions or need guidance, open an issue or join our discussions.
4. **Start Contributing:**  
   Look for issues labeled `Simple` or `PR needed` to find a good starting point.

Thank you for contributing to WebScreen—your efforts help make this project better for everyone!
