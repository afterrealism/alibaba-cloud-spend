#!/bin/bash
set -euo pipefail

VERSION="${1:?Usage: linux-build-deb.sh <version>}"
BINARY="${2:-alibaba-cloud-spend}"

PKG_NAME="alibaba-cloud-spend"
PKG_DIR="deb-build"
DEB_OUTPUT="${PKG_NAME}-${VERSION}-amd64.deb"

echo "Building .deb package: ${DEB_OUTPUT}"

rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}/DEBIAN"
mkdir -p "${PKG_DIR}/usr/local/bin"
mkdir -p "${PKG_DIR}/usr/share/applications"
mkdir -p "${PKG_DIR}/usr/share/icons/hicolor/256x256/apps"

cp "${BINARY}" "${PKG_DIR}/usr/local/bin/"
chmod 755 "${PKG_DIR}/usr/local/bin/${PKG_NAME}"

cat > "${PKG_DIR}/DEBIAN/control" << EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Depends: libgtk-4-1 (>= 4.0), libcurl4 (>= 7.0), libjson-c5 (>= 0.15), libssl3 (>= 3.0)
Maintainer: Sheece Gardezi <noreply@example.com>
Description: Alibaba Cloud Spend Dashboard
 A dark-themed GTK4 desktop dashboard for monitoring Alibaba Cloud billing.
 Features outstanding balances, monthly spend by service, AI/LLM model costs
 broken down by region, and 6-month historical data with auto-refresh.
Homepage: https://github.com/sheece/alibaba-cloud-spend
EOF

cat > "${PKG_DIR}/usr/share/applications/${PKG_NAME}.desktop" << EOF
[Desktop Entry]
Name=Alibaba Cloud Spend
Comment=Monitor Alibaba Cloud billing and spend
Exec=/usr/local/bin/${PKG_NAME}
Icon=${PKG_NAME}
Terminal=false
Type=Application
Categories=Utility;Finance;
Keywords=alibaba;cloud;billing;spend;dashboard;
EOF

if [ -f "assets/icon-256.png" ]; then
    cp "assets/icon-256.png" "${PKG_DIR}/usr/share/icons/hicolor/256x256/apps/${PKG_NAME}.png"
fi

dpkg-deb --build --root-owner-group "${PKG_DIR}" "${DEB_OUTPUT}"

echo "Built: ${DEB_OUTPUT}"
echo "Package info:"
dpkg-deb --info "${DEB_OUTPUT}"
