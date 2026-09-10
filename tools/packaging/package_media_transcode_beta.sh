#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <release-build-dir> <package-dir>" >&2
  exit 2
fi

build_dir=$(realpath "$1")
package_dir=$2
# Match the public header to the actual CMake build, not another checkout.
source_dir=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$build_dir/CMakeCache.txt")
test -n "$source_dir"

for archive in libmedia_transcode_beta.a libmedia_transcode_realtime_application.a libmedia_transcode_core.a; do
  test -f "$build_dir/$archive"
done
test -f "$source_dir/include/media_transcode_beta/realtime.h"
test -f "$build_dir/media_transcode_realtime_video_cli"
test ! -e "$package_dir"

mkdir -p "$package_dir/lib" "$package_dir/include/media_transcode_beta" "$package_dir/bin" "$package_dir/internal" "$package_dir/examples"
cp "$source_dir/include/media_transcode_beta/realtime.h" "$package_dir/include/media_transcode_beta/realtime.h"
cp "$build_dir/media_transcode_realtime_video_cli" "$package_dir/bin/"
cp "$build_dir/libmedia_transcode_beta.a" "$package_dir/internal/libmedia_transcode_beta_facade.a"
cp "$build_dir/libmedia_transcode_realtime_application.a" "$package_dir/internal/"
cp "$build_dir/libmedia_transcode_core.a" "$package_dir/internal/"

mri_file=$(mktemp)
trap 'rm -f "$mri_file"' EXIT
cat > "$mri_file" <<MRI
CREATE $package_dir/lib/libmedia_transcode_beta.a
ADDLIB $build_dir/libmedia_transcode_beta.a
ADDLIB $build_dir/libmedia_transcode_realtime_application.a
ADDLIB $build_dir/libmedia_transcode_core.a
SAVE
END
MRI
ar -M < "$mri_file"
ranlib "$package_dir/lib/libmedia_transcode_beta.a"
(
  cd "$package_dir"
  find include lib bin internal -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)
printf '%s\n' "$package_dir"
