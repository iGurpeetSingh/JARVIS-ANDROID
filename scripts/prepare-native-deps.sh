#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="${RUNNER_TEMP:-/tmp}/jarvis-native-deps"
SHERPA_URL="https://github.com/k2-fsa/sherpa-onnx/releases/download/v2.0.3/sherpa-onnx-v2.0.3-android.tar.bz2"
SHERPA_SHA256="2a30b222abae6d5cf544df4a1c27944279449c25dbb9c755097189cb45db1df0"
MODEL_URL="https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-tiny.tar.bz2"

mkdir -p "$TMP_DIR" \
  "$ROOT/app/src/main/cpp/vendor/sherpa/include" \
  "$ROOT/app/src/main/jniLibs/arm64-v8a" \
  "$ROOT/app/src/main/assets/sherpa-whisper-tiny"

echo "[JARVIS] Downloading sherpa-onnx 2.0.3 native Android libraries"
curl --fail --location --retry 3 "$SHERPA_URL" -o "$TMP_DIR/sherpa-android.tar.bz2"
echo "$SHERPA_SHA256  $TMP_DIR/sherpa-android.tar.bz2" | sha256sum --check
mkdir -p "$TMP_DIR/sherpa"
tar -xjf "$TMP_DIR/sherpa-android.tar.bz2" -C "$TMP_DIR/sherpa"

HEADER_DIR="$ROOT/app/src/main/cpp/vendor/sherpa/include/sherpa-onnx/c-api"
mkdir -p "$HEADER_DIR"
curl --fail --location --retry 3 \
  "https://raw.githubusercontent.com/k2-fsa/sherpa-onnx/v2.0.3/sherpa-onnx/c-api/cxx-api.h" \
  -o "$HEADER_DIR/cxx-api.h"
curl --fail --location --retry 3 \
  "https://raw.githubusercontent.com/k2-fsa/sherpa-onnx/v2.0.3/sherpa-onnx/c-api/c-api.h" \
  -o "$HEADER_DIR/c-api.h"

LIB_FILE="$(find "$TMP_DIR/sherpa" -type f -name 'libsherpa-onnx-cxx-api.so' -path '*arm64-v8a*' -print -quit)"
ONNX_FILE="$(find "$TMP_DIR/sherpa" -type f -name 'libonnxruntime.so' -path '*arm64-v8a*' -print -quit)"
if [[ -z "$LIB_FILE" || -z "$ONNX_FILE" ]]; then
  echo "The official sherpa-onnx package is missing arm64-v8a C++ API or ONNX Runtime libraries."
  find "$TMP_DIR/sherpa" -type f -name '*.so' | head -100
  exit 1
fi
LIB_DIR="$(dirname "$LIB_FILE")"
cp "$LIB_DIR"/*.so "$ROOT/app/src/main/jniLibs/arm64-v8a/"

echo "[JARVIS] Downloading multilingual Whisper Tiny model (offline ASR)"
curl --fail --location --retry 3 "$MODEL_URL" -o "$TMP_DIR/whisper-tiny.tar.bz2"
mkdir -p "$TMP_DIR/whisper-tiny"
tar -xjf "$TMP_DIR/whisper-tiny.tar.bz2" -C "$TMP_DIR/whisper-tiny"

ENCODER="$(find "$TMP_DIR/whisper-tiny" -type f -name 'tiny-encoder.int8.onnx' -print -quit)"
DECODER="$(find "$TMP_DIR/whisper-tiny" -type f -name 'tiny-decoder.int8.onnx' -print -quit)"
TOKENS="$(find "$TMP_DIR/whisper-tiny" -type f -name 'tiny-tokens.txt' -print -quit)"
if [[ -z "$ENCODER" || -z "$DECODER" || -z "$TOKENS" ]]; then
  echo "The Whisper Tiny model package is missing one or more required int8 files."
  find "$TMP_DIR/whisper-tiny" -type f | head -100
  exit 1
fi
cp "$ENCODER" "$ROOT/app/src/main/assets/sherpa-whisper-tiny/encoder.int8.onnx"
cp "$DECODER" "$ROOT/app/src/main/assets/sherpa-whisper-tiny/decoder.int8.onnx"
cp "$TOKENS" "$ROOT/app/src/main/assets/sherpa-whisper-tiny/tokens.txt"

echo "[JARVIS] Native dependency preparation complete."
du -h "$ROOT/app/src/main/jniLibs/arm64-v8a/"*.so \
  "$ROOT/app/src/main/assets/sherpa-whisper-tiny/"*
