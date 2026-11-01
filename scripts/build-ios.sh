#!/usr/bin/env bash
# Build only: never uploads to a store, modifies provisioning, or installs on a device.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-simulator}"
if ! developer_dir="$(xcode-select -p 2>/dev/null)" || [[ ! -d "$developer_dir/Platforms/iPhoneOS.platform" ]]; then
  echo 'A full Xcode installation with the iOS SDK must be selected before this build.' >&2
  exit 2
fi
case "$mode" in
  simulator)
    sdk=iphonesimulator
    arch="${AUDIOWIRE_IOS_ARCH:-$(uname -m)}"
    signing=(-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO)
    build_signing=(CODE_SIGNING_ALLOWED=NO)
    ;;
  device)
    if [[ $# -ne 2 || -z "$2" ]]; then
      echo 'Usage: scripts/build-ios.sh device APPLE_DEVELOPMENT_TEAM_ID' >&2
      exit 2
    fi
    sdk=iphoneos
    arch=arm64
    signing=("-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=$2")
    build_signing=()
    ;;
  *) echo 'Usage: scripts/build-ios.sh [simulator | device APPLE_DEVELOPMENT_TEAM_ID]' >&2; exit 2 ;;
esac
build="$root/build/ios-$mode"
xcodebuild -version
cmake -S "$root" -B "$build" -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 "-DCMAKE_OSX_SYSROOT=$sdk" \
  "-DCMAKE_OSX_ARCHITECTURES=$arch" -DAUDIOWIRE_BUILD_TESTS=OFF "${signing[@]}"
cmake --build "$build" --config Release --target AudioWire -- -sdk "$sdk" "${build_signing[@]}"
