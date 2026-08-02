#!/bin/bash
#
# build-deb.sh - Build a .deb package for nexfetch
#
# Usage: ./scripts/build-deb.sh [output_dir]
#
set -euo pipefail

OUTPUT_DIR="${1:-dist}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_DIR"

# Ensure build dependencies are installed
echo "==> Checking build dependencies..."
if ! dpkg -l | grep -q "debhelper"; then
    sudo apt-get update
    sudo apt-get install -y build-essential debhelper dh-make devscripts gcc make
fi

# Clean previous build artifacts
echo "==> Cleaning previous build..."
make clean || true
rm -rf debian/nexfetch
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# Build the package using dpkg-buildpackage
echo "==> Building .deb package..."
dpkg-buildpackage -us -uc -b

# Move the generated .deb to the output directory
echo "==> Collecting built packages..."
mv ../nexfetch_*.deb "$OUTPUT_DIR/" 2>/dev/null || true
mv ../nexfetch_*.buildinfo "$OUTPUT_DIR/" 2>/dev/null || true
mv ../nexfetch_*.changes "$OUTPUT_DIR/" 2>/dev/null || true

echo ""
echo "==> Build complete! Packages in $OUTPUT_DIR/:"
ls -la "$OUTPUT_DIR/"

echo ""
echo "Install with: sudo apt install $OUTPUT_DIR/nexfetch_*.deb"
