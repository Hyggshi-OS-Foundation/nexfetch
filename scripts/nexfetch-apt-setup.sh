#!/bin/bash
#
# nexfetch-apt-setup.sh — Add the nexfetch APT repository to your system
#
# Usage:
#   curl -sL https://raw.githubusercontent.com/Hyggshi-OS-Foundation/nexfetch/main/scripts/nexfetch-apt-setup.sh | sudo bash
#
# Or:
#   wget -qO- https://raw.githubusercontent.com/Hyggshi-OS-Foundation/nexfetch/main/scripts/nexfetch-apt-setup.sh | sudo bash
#
set -euo pipefail

REPO_OWNER="Hyggshi-OS-Foundation"
REPO_NAME="nexfetch"
REPO_BRANCH="main"
KEYRING_DIR="/etc/apt/keyrings"
KEYRING_FILE="${KEYRING_DIR}/nexfetch-archive-keyring.gpg"
SOURCES_FILE="/etc/apt/sources.list.d/nexfetch.list"

echo "==> Setting up nexfetch APT repository..."

# Install prerequisites
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq ca-certificates curl gnupg lsb-release

# Create keyring directory
mkdir -p "$KEYRING_DIR"

# Download and install the GPG public key
echo "==> Downloading GPG public key..."
curl -fsSL "https://raw.githubusercontent.com/${REPO_OWNER}/${REPO_NAME}/${REPO_BRANCH}/apt/repo-key.gpg" \
    | gpg --dearmor -o "$KEYRING_FILE"
chmod 644 "$KEYRING_FILE"

# Add the apt repository (using GitHub Pages as the apt repo host)
echo "==> Adding APT source list..."
ARCH=$(dpkg --print-architecture)
CODENAME=$(lsb_release -cs 2>/dev/null || echo "stable")

cat > "$SOURCES_FILE" << EOF
deb [arch=${ARCH} signed-by=${KEYRING_FILE}] https://${REPO_OWNER}.github.io/${REPO_NAME}/apt ${CODENAME} main
EOF

# Update apt and install nexfetch
echo "==> Updating package lists..."
apt-get update -qq

echo "==> Installing nexfetch..."
apt-get install -y nexfetch

echo ""
echo "==> nexfetch installed successfully!"
echo "    Run 'nexfetch' to see your system information."
echo ""
echo "    To update in the future: sudo apt update && sudo apt upgrade nexfetch"
echo "    To uninstall: sudo apt remove nexfetch"
