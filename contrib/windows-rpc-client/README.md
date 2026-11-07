# Windows RPC directory client: source prerequisites

## Scope and verification status

These are opt-in inputs for a disposable client-only cross-build. The normal root CMake and all submodule gitlinks remain unchanged. No binary, compiler output, private build report or credential is included. The FC patches remove Windows system-library `.lib` suffixes and use genuine `CaptureStackBackTrace` with honest address-only output on Windows. POSIX stacktrace code and signal-handler installation/reset/log/exit behavior remain intact. There are no scheduler, crypto, TLS, secp implementation or consensus substitutions.

This addition was checked by applying both patches to exact pinned FC Git blobs and checking before/after hashes; no source build or helper execution was performed for this preparation. The manual commands below reconstruct recorded legacy build settings, but the entire recipe has **NOT RUN from a clean public checkout**. This is not a hermetic build, bit-identical rebuild promise, historical-input attestation, or security approval. Historical native acceptance of an earlier executable is separate evidence and does not validate a newly built executable.

## Public source pins

- Core: `132b7b53511a42cdf885a7575ff122fdb947e9fe` (PR 252 baseline).
- FC: `94d96aaff4fddcb49195d165751b174a729b78d6`, `https://github.com/golos-blockchain/chain-fc`.
- Nested FC revisions: `fc-gitlinks.txt`; URLs come from pinned FC `.gitmodules`. Do not advance or replace gitlinks or silently follow a fork. Public remote availability was not retested here.
- Boost: 1.65.1. `boost-official-metadata.json` preserves official metadata values, including literal `file: boost_1_65_1_rc2.tar.bz2` despite the metadata URL naming 1.65.1. Metadata SHA256 is not a hash of locally verified archive bytes. No Boost archive was downloaded or hashed here, and this does not prove which archive supplied historical unpacked sources.
- OpenSSL: 1.1.1w; URL and recorded expected archive SHA256 are in `openssl-provenance.json`. Verify acquired bytes before use; not re-downloaded here.
- Legacy Ubuntu Bionic CMake 3.10.2, MinGW GCC/G++ 7.3.0, headers 5.0.3, binutils 2.30: `ubuntu-packages.json` records selected package versions, pool paths and expected SHA256 values. Authenticate signed Ubuntu Release/Packages metadata before acquiring/installing packages. This selection is not a complete host/transitive dependency lock.

Boost 1.65.1, OpenSSL 1.1.1w and this toolchain are legacy/EOL compatibility inputs, not production-security recommendations. A genuine compatible b2 must be prepared; the recorded engine reported 2015.07. Its exact bootstrap provenance is not locked here. Do not silently claim a newer engine reproduces that input. Host make/autoconf/automake/libtool/perl and transitive dependencies also require independent preparation.

## Manual legacy recipe (NOT RUN as a clean sequence)

Use a disposable legacy Linux build environment, without relying on private images or historical objects. Start from a public core checkout at the fixed commit, disable autocrlf and initialize pinned recursive submodules. Place it at `/work/source`; expose this directory as `/work/inputs` before replacing the disposable root CMake. Verify FC HEAD and every nested gitlink against the pins. Do not edit the normal development checkout.

Before applying patches, verify each raw FC file hash against either exact `before_lf_sha256` or exact `before_crlf_sha256` in `fc-before-after.json`; abort on any other bytes. Only then normalize these two files to LF. In `/work/source/thirdparty/fc`, run `git apply --check` on both patch files, then `git apply` on both; verify exact `after_lf_sha256`. These steps intentionally dirty only a disposable fixed-pin FC copy. Do not accept fuzzy/context-free patching or unrelated FC changes.

Copy `client-only-root.CMakeLists.txt` to `/work/source/CMakeLists.txt`, `toolchain.cmake` to `/work/toolchain.cmake`, and `user-config.jam` to `/work/user-config.jam`. If importing a Windows export, normalize only CRLF-containing secp `.sh`, `.ac`, `.am` build scripts; do not modify C/header implementations. Clean LF public clones need no such normalization.

After authenticating/installing the pinned cross packages, unpack independently verified Boost 1.65.1 at `/boost-source`, supply a genuine compatible `/work/boost-b2`, and verify/unpack OpenSSL at `/work/openssl-1.1.1w`. The expected archive hashes are inputs to check, not proof of acquisition. Use empty output directories.

```sh
mkdir -p /work/boost-build /work/prefix
cd /boost-source
/work/boost-b2 --user-config=/work/user-config.jam --build-dir=/work/boost-build --prefix=/work/prefix toolset=gcc-mingw target-os=windows address-model=64 architecture=x86 binary-format=pe abi=ms threading=multi threadapi=win32 link=static runtime-link=static variant=release --layout=system -j1 --with-thread --with-date_time --with-system --with-filesystem --with-program_options --with-signals --with-serialization --with-chrono --with-test --with-context --with-locale --with-iostreams --with-coroutine --with-regex -sNO_BZIP2=1 -sNO_ZLIB=1 install
cd /work/openssl-1.1.1w
./Configure mingw64 --cross-compile-prefix=x86_64-w64-mingw32- --prefix=/work/prefix --openssldir=/work/prefix/ssl no-shared
make -j1
make install_sw install_ssldirs
cp /work/prefix/ssl/openssl.cnf /work/prefix/openssl.cnf
mkdir -p /work/build
cd /work/build
export BOOST_ROOT=/work/prefix OPENSSL_ROOT_DIR=/work/prefix
cmake /work/source -DCMAKE_TOOLCHAIN_FILE=/work/toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBoost_NO_SYSTEM_PATHS=ON \
  -DBoost_USE_STATIC_RUNTIME=ON -DBOOST_LIBRARYDIR=/work/prefix/lib \
  -DOPENSSL_USE_STATIC_LIBS=ON \
  -DBoost_THREAD_LIBRARY_RELEASE=/work/prefix/lib/libboost_thread_win32.a \
  -DBoost_THREAD_LIBRARY_DEBUG=/work/prefix/lib/libboost_thread_win32.a \
  -DBUILD_RPC_DIRECTORY_LOOPBACK_TEST=ON
make -j1 VERBOSE=1 golos-rpc-directory-client rpc_directory_loopback_test
x86_64-w64-mingw32-ar t /work/prefix/lib/libboost_context.a
x86_64-w64-mingw32-nm /work/prefix/lib/libboost_context.a
x86_64-w64-mingw32-objdump -p programs/rpc_directory_client/golos-rpc-directory-client.exe
sha256sum programs/rpc_directory_client/*.exe
```

`abi=ms` is essential for genuine x86_64 Microsoft-ABI PE GAS Context assembly; check actual assembly objects and defined `make_fcontext`/`jump_fcontext`, not just unresolved references or a stack_traits-only archive. Never relax unresolved-symbol checks or introduce stubs. Preserve source/dependency hashes, tool versions and build logs for a future clean-build review. Inspect linked PE imports, manifest and unexpected runtime DLL dependencies. The existing UTF-8 activeCodePage manifest requires Windows 10 1903+.

## Required future acceptance and licensing

A new PE requires fresh native Windows minimal-PATH execution, encrypted P2P acceptance, and Cyrillic/Chinese/spaces atomic-cache acceptance. Cross-link success and old artifact acceptance do not replace these checks. No native acceptance result is asserted for this source-only addition. Applications must reference the eventual published prerequisite revision, not assume the earlier PR baseline already contains this overlay; publication remains conditional on separate application/feed checks and review.

`FC-NOTICES.md` records the actual pinned per-file inspection, including missing root FC LICENSE and absent notices in the two patched files. Do not infer a global FC license. Core, nested vendors, Boost, OpenSSL and MinGW runtime notices/exceptions must be reviewed and preserved independently before binary distribution; this source package is not a complete licensing clearance.
