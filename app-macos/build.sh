#!/bin/bash
# Compila "Analisis de Clima.app" para macOS (universal: arm64 + x86_64).
# Requiere: Xcode Command Line Tools (xcode-select --install)
#
# Uso:
#   ./build.sh          -> compila Analisis de Clima.app en ./dist
#   ./build.sh --open   -> además lo abre con `open`

set -euo pipefail

APP_NAME="Analisis de Clima"
APP_BUNDLE="dist/Analisis de Clima.app"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)/Sources"
DEPLOY_TARGET="macosx13.0"

if ! command -v swiftc >/dev/null 2>&1; then
    echo "ERROR: swiftc no encontrado. Instala Command Line Tools con: xcode-select --install"
    exit 1
fi

echo "Compilando $APP_NAME (universal)..."

rm -rf "dist/Analisis de Clima.app"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"

ARCHS=()
if command -v xcrun >/dev/null 2>&1 && xcrun -sdk macosx --show-sdk-path >/dev/null 2>&1; then
    HOST_ARCH=$(uname -m)
    if [ "$HOST_ARCH" = "arm64" ]; then
        ARCHS=(arm64 x86_64)
    else
        ARCHS=(x86_64 arm64)
    fi
else
    ARCHS=("$(uname -m)")
fi

OBJS=()
for ARCH in "${ARCHS[@]}"; do
    echo "  -> compilando para $ARCH"
    swiftc -O \
        -target "$ARCH-apple-macosx$DEPLOY_TARGET" \
        -parse-as-library \
        -module-name AnalisisDeClima \
        -o "/tmp/analisis-clima-$ARCH" \
        "$SRC_DIR"/*.swift
    OBJS+=("/tmp/analisis-clima-$ARCH")
done

if [ "${#OBJS[@]}" -eq 2 ]; then
    lipo -create "${OBJS[@]}" -output "$APP_BUNDLE/Contents/MacOS/Analisis de Clima"
    rm -f "${OBJS[@]}"
else
    mv "${OBJS[0]}" "$APP_BUNDLE/Contents/MacOS/Analisis de Clima"
fi

chmod +x "$APP_BUNDLE/Contents/MacOS/Analisis de Clima"

cat > "$APP_BUNDLE/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>Analisis de Clima</string>
    <key>CFBundleDisplayName</key>
    <string>Analisis de Clima</string>
    <key>CFBundleIdentifier</key>
    <string>com.analisisdeclima.app</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundleExecutable</key>
    <string>Analisis de Clima</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSMinimumSystemVersion</key>
    <string>13.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.weather</string>
</dict>
</plist>
PLIST

echo ""
echo "Listo: $APP_BUNDLE"
echo "Para abrir: open '$APP_BUNDLE'"

if [ "${1:-}" = "--open" ]; then
    open "$APP_BUNDLE"
fi
