#!/usr/bin/env bash

set -eo pipefail

echo "============================================="
echo "Installing Undo - Bring Ctrl+Z to terminal"
echo "============================================="

# 1. Detect OS
OS=$(uname -s)
if [ "$OS" != "Linux" ] && [ "$OS" != "Darwin" ]; then
    echo "Error: Unsupported operating system: $OS"
    echo "Undo only supports Linux and macOS."
    exit 1
fi
echo "Detected OS: $OS"

# 2. Build from source
echo "Compiling binary from source..."
if ! command -v make >/dev/null; then
    echo "Error: 'make' utility is required to compile Undo. Please install it."
    exit 1
fi
if ! command -v clang >/dev/null && ! command -v gcc >/dev/null; then
    echo "Error: A C compiler (clang or gcc) is required. Please install one."
    exit 1
fi

make clean
make

# 3. Install binary
INSTALL_DIR="/usr/local/bin"
TARGET_BIN="$INSTALL_DIR/undo"

if [ ! -d "$INSTALL_DIR" ]; then
    echo "Creating directory $INSTALL_DIR..."
    if [ -w "$(dirname "$INSTALL_DIR")" ]; then
        mkdir -p "$INSTALL_DIR"
    else
        echo "Requires sudo permission to create $INSTALL_DIR..."
        sudo mkdir -p "$INSTALL_DIR"
    fi
fi

echo "Copying binary to $TARGET_BIN..."
if [ -w "$INSTALL_DIR" ]; then
    cp undo "$TARGET_BIN"
    chmod 755 "$TARGET_BIN"
else
    echo "Requires sudo permission to write to $INSTALL_DIR..."
    sudo cp undo "$TARGET_BIN"
    sudo chmod 755 "$TARGET_BIN"
fi

# Clean build artifacts
make clean >/dev/null

# 4. Optional shell alias setup
ENABLE_ALIAS="n"
if [ -t 0 ]; then
    # Interactive session
    read -p "Do you want to enable the shell alias 'rm=undo --rm'? [y/N] " -n 1 -r REPLY
    echo
    if [[ "$REPLY" =~ ^[Yy]$ ]]; then
        ENABLE_ALIAS="y"
    fi
else
    # Non-interactive session
    echo "Non-interactive shell detected. Skipping interactive alias configuration."
fi

if [ "$ENABLE_ALIAS" = "y" ]; then
    echo "Enabling shell alias..."
    "$TARGET_BIN" install
else
    echo "Skipped alias setup."
    echo "To enable it later, run: undo install"
fi

echo
echo "---------------------------------------------"
echo "Success! Undo has been installed successfully."
echo "Binary path: $TARGET_BIN"
echo "---------------------------------------------"
echo "Usage:"
echo "  rm thesis.pdf    # Delete a file (if alias is active)"
echo "  undo             # Revert the latest deletion"
echo "  undo history     # List deletions history"
echo "  undo --help      # Show more commands"
echo "---------------------------------------------"
