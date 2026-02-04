#!/bin/bash
set -e

BUILD_DIR="build"
APP_BUNDLE="$BUILD_DIR/ReferenceCapturer_artefacts/Release/Reference Capturer.app"

# Configure if needed
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Configuring CMake..."
    cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=$(uname -m)
fi

# Build
echo "Building..."
cmake --build "$BUILD_DIR"

# Copy webapp resources to bundle
echo "Copying webapp resources..."
rsync -a --delete Resources/webapp/ "$APP_BUNDLE/Contents/Resources/webapp/"

# Re-sign the app after modifying resources
echo "Re-signing app bundle..."
codesign --force --deep --sign - "$APP_BUNDLE"

echo "Build complete!"
echo "Application: $APP_BUNDLE"
