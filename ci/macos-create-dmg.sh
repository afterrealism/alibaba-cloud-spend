#!/bin/bash
set -euo pipefail

VERSION="${1:?Usage: macos-create-dmg.sh <version>}"
BINARY="${2:-alibaba-cloud-spend}"

APP_NAME="AlibabaCloudSpend"
APP_BUNDLE="${APP_NAME}.app"
DMG_OUTPUT="${APP_NAME}-${VERSION}-macos.dmg"

echo "Creating macOS .app bundle and DMG: ${DMG_OUTPUT}"

rm -rf "${APP_BUNDLE}"
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"

cp "${BINARY}" "${APP_BUNDLE}/Contents/MacOS/"
chmod +x "${APP_BUNDLE}/Contents/MacOS/${BINARY}"

if [ -f "assets/icon-512.png" ]; then
    echo "Generating ICNS from PNG..."
    ICONSET_DIR="${APP_NAME}.iconset"
    mkdir -p "${ICONSET_DIR}"
    
    sips -z 16 16     assets/icon-512.png --out "${ICONSET_DIR}/icon_16x16.png" > /dev/null
    sips -z 32 32     assets/icon-512.png --out "${ICONSET_DIR}/icon_16x16@2x.png" > /dev/null
    sips -z 32 32     assets/icon-512.png --out "${ICONSET_DIR}/icon_32x32.png" > /dev/null
    sips -z 64 64     assets/icon-512.png --out "${ICONSET_DIR}/icon_32x32@2x.png" > /dev/null
    sips -z 128 128   assets/icon-512.png --out "${ICONSET_DIR}/icon_128x128.png" > /dev/null
    sips -z 256 256   assets/icon-512.png --out "${ICONSET_DIR}/icon_128x128@2x.png" > /dev/null
    sips -z 256 256   assets/icon-512.png --out "${ICONSET_DIR}/icon_256x256.png" > /dev/null
    sips -z 512 512   assets/icon-512.png --out "${ICONSET_DIR}/icon_256x256@2x.png" > /dev/null
    sips -z 512 512   assets/icon-512.png --out "${ICONSET_DIR}/icon_512x512.png" > /dev/null
    sips -z 1024 1024 assets/icon-512.png --out "${ICONSET_DIR}/icon_512x512@2x.png" > /dev/null
    
    iconutil -c icns "${ICONSET_DIR}" -o "${APP_BUNDLE}/Contents/Resources/${APP_NAME}.icns"
    rm -rf "${ICONSET_DIR}"
    echo "  Created: ${APP_NAME}.icns"
fi

cat > "${APP_BUNDLE}/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>${BINARY}</string>
    <key>CFBundleIdentifier</key>
    <string>com.sheece.alicloudspend</string>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundleDisplayName</key>
    <string>Alibaba Cloud Spend</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleIconFile</key>
    <string>${APP_NAME}</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
</dict>
</plist>
EOF

echo "Bundling dylibs..."
FRAMEWORKS_DIR="${APP_BUNDLE}/Contents/Frameworks"
mkdir -p "${FRAMEWORKS_DIR}"

bundle_dylibs() {
    local binary="$1"
    local deps
    
    deps=$(otool -L "${binary}" 2>/dev/null | grep -E '^\s+/' | awk '{print $1}' || true)
    
    for dep in ${deps}; do
        if [[ "${dep}" == /usr/lib/* ]] || [[ "${dep}" == /System/* ]]; then
            continue
        fi
        
        local dep_name=$(basename "${dep}")
        local dest="${FRAMEWORKS_DIR}/${dep_name}"
        
        if [ ! -f "${dest}" ]; then
            echo "  Copying: ${dep_name}"
            cp -L "${dep}" "${dest}" 2>/dev/null || true
            chmod +w "${dest}" 2>/dev/null || true
            
            install_name_tool -id "@executable_path/../Frameworks/${dep_name}" "${dest}" 2>/dev/null || true
            
            bundle_dylibs "${dest}"
        fi
        
        install_name_tool -change "${dep}" "@executable_path/../Frameworks/${dep_name}" "${binary}" 2>/dev/null || true
    done
}

bundle_dylibs "${APP_BUNDLE}/Contents/MacOS/${BINARY}"

echo "Creating DMG..."
rm -f "${DMG_OUTPUT}"

if command -v create-dmg &> /dev/null; then
    create-dmg \
        --volname "Alibaba Cloud Spend" \
        --volicon "${APP_BUNDLE}/Contents/Resources/${APP_NAME}.icns" \
        --background "assets/dmg-background.png" \
        --window-pos 200 120 \
        --window-size 660 400 \
        --icon-size 100 \
        --icon "${APP_BUNDLE}" 150 200 \
        --hide-extension "${APP_BUNDLE}" \
        --app-drop-link 450 200 \
        --no-internet-enable \
        "${DMG_OUTPUT}" \
        "${APP_BUNDLE}"
else
    echo "Warning: create-dmg not found, using basic hdiutil..."
    hdiutil create -volname "Alibaba Cloud Spend" -srcfolder "${APP_BUNDLE}" -ov -format UDZO "${DMG_OUTPUT}"
fi

echo "Built: ${DMG_OUTPUT}"
echo "Size: $(du -h "${DMG_OUTPUT}" | cut -f1)"
