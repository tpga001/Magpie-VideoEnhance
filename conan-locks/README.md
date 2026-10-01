# Conan lock files

These lock files pin the recipe revisions used by the Release x64 MSVC build.
Refresh them intentionally when dependency updates are reviewed; normal builds
must not pass Conan's `--update` option.

The current locks target Release x64, MSVC 19.4, and the build flags declared
by `src/_ConanDeps/_ConanDeps.vcxproj`.
