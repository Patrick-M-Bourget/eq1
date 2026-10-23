#!/usr/bin/env bash
# Builds the shipping configuration (Release, EQ1_LTO=ON, Universal), signs the VST3, AU and CLAP with a
# Developer ID, packages them in one installer, notarizes and staples it (#16; AAX waits for PACE).
#
#   scripts/release-mac.sh <label>            e.g. alpha.1: writes dist/eq1-<version>-<label>.pkg
#   scripts/release-mac.sh --dry-run <label>  signs ad hoc and neither signs the installer nor notarizes, to
#                                             try the build and packaging without the credentials
#
# The credentials live in the login keychain, never in this repo: a "Developer ID Application" and a
# "Developer ID Installer" identity (SIGN_APP and SIGN_INSTALLER override which), and a notarytool profile
# stored with `xcrun notarytool store-credentials` (NOTARY_PROFILE, default eq1-notary). Each bundle's .dSYM
# is zipped beside the installer, for symbolicating the testers' crash logs.
#
# BUILD_DIR (default build-release) and FETCHCONTENT_BASE_DIR (default .deps, or the main checkout's .deps in a
# linked worktree) can be overridden.
set -euo pipefail
cd "$(dirname "$0")/.."

dry_run=0
if [ "${1:-}" = --dry-run ]; then
    dry_run=1
    shift
fi
label=${1:?usage: scripts/release-mac.sh [--dry-run] <label>, e.g. alpha.1}
[ "$(uname -s)" = Darwin ] || { echo "release-mac.sh runs on macOS only" >&2; exit 1; }

BUILD_DIR=${BUILD_DIR:-build-release}
# A linked worktree shares the main checkout's dependencies, as scripts/check.sh does.
common_dir=$(git rev-parse --path-format=absolute --git-common-dir 2> /dev/null || true)
main_checkout=${common_dir%/.git}
if [ -z "${FETCHCONTENT_BASE_DIR:-}" ] && [ -n "$common_dir" ] && [ "$main_checkout" != "$PWD" ]; then
    DEPS=$main_checkout/.deps
else
    DEPS=${FETCHCONTENT_BASE_DIR:-$PWD/.deps}
fi
NOTARY_PROFILE=${NOTARY_PROFILE:-eq1-notary}
version=$(sed -n 's/^project(eq1 VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[ -n "$version" ] || { echo "no project version in CMakeLists.txt" >&2; exit 1; }
name=eq1-$version-$label
dist=$PWD/dist
work=$BUILD_DIR/package

step() { printf '\n== %s\n' "$*"; }

# The one identity in the keychain whose name starts with $1, unless $2 names it already.
identity() {
    local kind=$1 given=$2 found
    if [ -n "$given" ]; then
        printf '%s\n' "$given"
        return
    fi
    found=$(security find-identity -v | sed -n "s/^ *[0-9]*) [0-9A-F]* \"\($kind: .*\)\"\$/\1/p" | sort -u)
    case "$(printf '%s' "$found" | grep -c .)" in
        1) printf '%s\n' "$found" ;;
        0) echo "no \"$kind\" identity in the keychain (see the comment at the top of this script)" >&2; return 1 ;;
        *) echo "more than one \"$kind\" identity; choose one with the environment variable:" >&2
           printf '  %s\n' "$found" >&2; return 1 ;;
    esac
}

if [ $dry_run = 1 ]; then
    sign_app=-
    sign_installer=
else
    sign_app=$(identity "Developer ID Application" "${SIGN_APP:-}")
    sign_installer=$(identity "Developer ID Installer" "${SIGN_INSTALLER:-}")
    xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" > /dev/null ||
        { echo "notarytool profile $NOTARY_PROFILE doesn't sign in" >&2; exit 1; }
fi

step "Shipping build (Release, EQ1_LTO=ON, Universal) in $BUILD_DIR"
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
    -DEQ1_LTO=ON -DEQ1_BUILD_TESTS=OFF "-DFETCHCONTENT_BASE_DIR=$DEPS"
cmake --build "$BUILD_DIR" --config Release --parallel --target eq1_VST3 eq1_AU eq1_CLAP
artefacts=$BUILD_DIR/plugin/eq1_artefacts/Release

rm -rf "$work"
mkdir -p "$work" "$dist"
# Each format: its bundle, where it installs, and its package identifier's suffix.
formats=(
    "VST3/eq1.vst3 /Library/Audio/Plug-Ins/VST3 vst3"
    "AU/eq1.component /Library/Audio/Plug-Ins/Components au"
    "CLAP/eq1.clap /Library/Audio/Plug-Ins/CLAP clap"
)

step "Signing with ${sign_app/#-/ad hoc (dry run)}"
for entry in "${formats[@]}"; do
    read -r bundle location suffix <<< "$entry"
    root=$work/root-$suffix
    mkdir -p "$root"
    ditto "$artefacts/$bundle" "$root/$(basename "$bundle")"
    # The hardened runtime and a secure timestamp are what notarization asks of every executable.
    codesign --force --options runtime --timestamp$([ $dry_run = 1 ] && echo =none) --sign "$sign_app" \
        "$root/$(basename "$bundle")"
    codesign --verify --strict --verbose=2 "$root/$(basename "$bundle")"
done

step "Packaging $name.pkg"
choices= refs=
for entry in "${formats[@]}"; do
    read -r bundle location suffix <<< "$entry"
    root=$work/root-$suffix
    # Not relocatable: the installer otherwise "upgrades" a copy it finds elsewhere (a build folder, or
    # another format's bundle, which shares the bundle identifier) instead of installing where the host
    # looks. Not version checked: every alpha of one version installs over the last.
    pkgbuild --analyze --root "$root" "$work/$suffix.plist" > /dev/null
    plutil -replace 0.BundleIsRelocatable -bool NO "$work/$suffix.plist"
    plutil -replace 0.BundleIsVersionChecked -bool NO "$work/$suffix.plist"
    # pkgbuild prints "write: Permission denied" lines on macOS 27 even for a package of one file, and
    # writes the package regardless; they are dropped so a real error stands out.
    pkgbuild --root "$root" --component-plist "$work/$suffix.plist" --install-location "$location" \
        --identifier "com.pbourget.eq1.$suffix" --version "$version" "$work/eq1-$suffix.pkg" 2>&1 |
        { grep -v -e '^write: Permission denied$' -e '^pkgbuild: ' || true; }
    [ -f "$work/eq1-$suffix.pkg" ] || { echo "pkgbuild wrote no eq1-$suffix.pkg" >&2; exit 1; }
    choices+="    <choice id=\"$suffix\" title=\"$(dirname "$bundle")\"><pkg-ref id=\"com.pbourget.eq1.$suffix\"/></choice>"$'\n'
    refs+="    <pkg-ref id=\"com.pbourget.eq1.$suffix\" version=\"$version\">eq1-$suffix.pkg</pkg-ref>"$'\n'
done
cat > "$work/distribution.xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>eq1 $version ($label)</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <volume-check><allowed-os-versions><os-version min="11.0"/></allowed-os-versions></volume-check>
    <choices-outline>
$(for entry in "${formats[@]}"; do read -r _ _ suffix <<< "$entry"; echo "        <line choice=\"$suffix\"/>"; done)
    </choices-outline>
$choices$refs</installer-gui-script>
EOF
pkg=$dist/$name.pkg
rm -f "$pkg"
productbuild --distribution "$work/distribution.xml" --package-path "$work" \
    ${sign_installer:+--sign "$sign_installer" --timestamp} "$pkg"

step "Debug symbols: $name-dSYMs.zip"
rm -f "$dist/$name-dSYMs.zip"
(cd "$artefacts" && zip -qry "$dist/$name-dSYMs.zip" VST3/eq1.vst3.dSYM AU/eq1.component.dSYM CLAP/eq1.clap.dSYM)

if [ $dry_run = 1 ]; then
    step "Dry run: $pkg is signed ad hoc, its installer unsigned and not notarized; it installs on this Mac only"
    exit 0
fi

step "Notarizing (usually a few minutes)"
submission=$(xcrun notarytool submit "$pkg" --keychain-profile "$NOTARY_PROFILE" --wait --output-format plist)
status=$(plutil -extract status raw - <<< "$submission")
if [ "$status" != Accepted ]; then
    id=$(plutil -extract id raw - <<< "$submission")
    echo "notarization: $status; Apple's log:" >&2
    xcrun notarytool log "$id" --keychain-profile "$NOTARY_PROFILE" >&2
    exit 1
fi
xcrun stapler staple "$pkg"

step "Gatekeeper's verdict"
spctl --assess --type install --verbose=2 "$pkg"
pkgutil --check-signature "$pkg" | head -4
echo
echo "$pkg"
echo "$dist/$name-dSYMs.zip"
