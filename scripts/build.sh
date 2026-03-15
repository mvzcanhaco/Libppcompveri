#!/usr/bin/env bash
# scripts/build.sh — Script de build da libppcomp
# Uso:
#   ./scripts/build.sh --test      Compila e roda testes unitários (x86_64, sem hardware)
#   ./scripts/build.sh --android   Build Android arm64-v8a (requer NDK)
#   ./scripts/build.sh --release   Build release (requer NDK + assinatura Verifone)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

: "${ANDROID_NDK_HOME:=${ANDROID_HOME:-}/ndk/26.2.11394342}"
NDK_CMAKE="${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake"

usage() {
    echo "Uso: $0 [--test|--android|--release]"
    exit 1
}

build_test() {
    echo "=== Build de testes (x86_64) ==="
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}/test" \
        -DBUILD_TESTS=ON \
        -DUSE_SDI_MOCK=ON \
        -DCMAKE_BUILD_TYPE=Debug
    cmake --build "${BUILD_DIR}/test" -- -j"$(nproc)"
    cd "${BUILD_DIR}/test" && ctest --output-on-failure
}

build_android() {
    echo "=== Build Android arm64-v8a ==="
    if [[ ! -f "${NDK_CMAKE}" ]]; then
        echo "ERRO: NDK não encontrado em ${ANDROID_NDK_HOME}"
        echo "Defina ANDROID_NDK_HOME ou instale o NDK r26b via Android SDK Manager."
        exit 1
    fi
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}/android" \
        -DCMAKE_TOOLCHAIN_FILE="${NDK_CMAKE}" \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-28 \
        -DANDROID_STL=c++_shared \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "${BUILD_DIR}/android" -- -j"$(nproc)"
    echo "=== Artefato gerado: ==="
    ls -lh "${BUILD_DIR}/android/libppcomp.so"
    echo "=== Símbolos exportados PP_*: ==="
    nm "${BUILD_DIR}/android/libppcomp.so" | grep " T PP_" || true
}

build_release() {
    echo "=== Build Release ==="
    build_android
    echo ""
    echo "ATENÇÃO: O binário de produção precisa ser assinado no Verifone Developer Central."
    echo "Acesse: https://developer.verifone.com (seção 'Sign Application')"
    echo "Faça upload de: ${BUILD_DIR}/android/libppcomp.so"
}

case "${1:-}" in
    --test)    build_test ;;
    --android) build_android ;;
    --release) build_release ;;
    *) usage ;;
esac
