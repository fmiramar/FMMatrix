# Build, verification and packaging

This directory is designed to become a standalone repository. Its workflow
builds from that repository's root, not from the parent multi-project workspace.
There is no dependency on other port projects or on private SDK locations.

## CI

The native matrix uses SuperCollider 3.14.1 headers, explicit macOS x86_64 and
arm64 architectures, Linux x64 and Windows x64, in Release and Debug. Both
scsynth and Supernova modules are built. A separate Linux job installs the
distribution's runtime and checks out its matching source tag before running
language, NRT audio, SCDoc and live server checks against a dummy JACK device.
ASan/UBSan cover standalone DSP and the real adapter using a mock RT allocator.

Runner labels are explicit, including `macos-15-intel`; changing the macOS
default architecture cannot silently replace the x64 package. The workflow
uploads build artifacts only and does not publish GitHub releases automatically.
CI configuration must still run on GitHub before claiming those platform jobs
passed. Local macOS testing does not establish Windows or Linux verification.

## Local archive

After tests pass, stage and package the completed extension:

```sh
cmake --install build --config Release --prefix stage/Extensions
python3 tools/package.py --stage stage --output dist --owner GITHUB_OWNER \
  --version 0.1.0 --platform macos --architecture x64
```

Archives contain `Extensions/FMMatrixUGens`, a small build-info file and a
separate SHA-256 checksum. The name is derived only from the explicit account,
project, version, platform and architecture. Verify the actual binary architecture
before uploading an archive. If hosted macOS x64 CI is unavailable, this same
procedure can assemble a tested local Intel build for later publication.

Recompile the class library and restart the server after installing updated
classes or binaries. Do not run two different copies of the extension in the
SuperCollider search path.

## Verification from a source checkout

After installation, run the language/audio/help suite and the private live servers:

```sh
python3 tools/run_sc_tests.py --live --supernova
```

The example test mutes its private server and executes the shipped examples,
including their actual buffer setup. To repeat only changed checks, use for example
`--live --only examples_smoke`. The optional `--fm7 --only fm7_comparison`
requires sc3-plugins FM7 and regenerates its comparison CSV. Set `--sclang` when
the executable is not on PATH. Logs and rendered audio stay in ignored `build/`.

Native sanitizer checks use a separate Debug build:

```sh
cmake -S . -B build-sanitize -DSC_PATH=/path/to/supercollider \
  -DSCSYNTH=OFF -DSUPERNOVA=OFF -DFM_MATRIX_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize --parallel 2
ctest --test-dir build-sanitize --output-on-failure
```

The verified local target is macOS x64 with SuperCollider 3.14.1, for both
scsynth and Supernova. Windows x64 has also cross-compiled, but its runtime,
Linux, and macOS arm64 still need native/hosted validation. Keep those claims
separate when preparing release notes. Never promote a successful cross-build
into a claim that platform audio tests ran.
