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

echo "[JARVIS] Downloading streaming keyword-spotting model for the Jarvis wake phrase"
KWS_URL="https://github.com/k2-fsa/sherpa-onnx/releases/download/kws-models/sherpa-onnx-kws-zipformer-gigaspeech-3.3M-2024-01-01-mobile.tar.bz2"
curl --fail --location --retry 3 "$KWS_URL" -o "$TMP_DIR/jarvis-kws-mobile.tar.bz2"
mkdir -p "$TMP_DIR/jarvis-kws"
tar -xjf "$TMP_DIR/jarvis-kws-mobile.tar.bz2" -C "$TMP_DIR/jarvis-kws"

KWS_ENCODER="$(find "$TMP_DIR/jarvis-kws" -type f -name 'encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx' -print -quit)"
KWS_DECODER="$(find "$TMP_DIR/jarvis-kws" -type f -name 'decoder-epoch-12-avg-2-chunk-16-left-64.onnx' -print -quit)"
KWS_JOINER="$(find "$TMP_DIR/jarvis-kws" -type f -name 'joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx' -print -quit)"
KWS_TOKENS="$(find "$TMP_DIR/jarvis-kws" -type f -name 'tokens.txt' -print -quit)"
KWS_BPE="$(find "$TMP_DIR/jarvis-kws" -type f -name 'bpe.model' -print -quit)"
if [[ -z "$KWS_ENCODER" || -z "$KWS_DECODER" || -z "$KWS_JOINER" || -z "$KWS_TOKENS" || -z "$KWS_BPE" ]]; then
  echo "The wake-word model archive is missing an expected Zipformer/BPE file."
  find "$TMP_DIR/jarvis-kws" -type f | head -100
  exit 1
fi

mkdir -p "$ROOT/app/src/main/assets/jarvis-kws"
cp "$KWS_ENCODER" "$ROOT/app/src/main/assets/jarvis-kws/encoder.int8.onnx"
cp "$KWS_DECODER" "$ROOT/app/src/main/assets/jarvis-kws/decoder.onnx"
cp "$KWS_JOINER" "$ROOT/app/src/main/assets/jarvis-kws/joiner.int8.onnx"
cp "$KWS_TOKENS" "$ROOT/app/src/main/assets/jarvis-kws/tokens.txt"
cp "$KWS_BPE" "$ROOT/app/src/main/assets/jarvis-kws/bpe.model"

echo "[JARVIS] Creating BPE-tokenized open-vocabulary wake phrase"
python3 -m pip install --disable-pip-version-check --quiet "sherpa-onnx==2.0.3" sentencepiece pypinyin
python3 - "$ROOT/app/src/main/assets/jarvis-kws/tokens.txt" \
  "$ROOT/app/src/main/assets/jarvis-kws/bpe.model" \
  "$ROOT/app/src/main/assets/jarvis-kws/keywords.txt" <<'PY'
import sys
from sherpa_onnx import text2token

tokens_path, bpe_path, output_path = sys.argv[1:4]
phrases = ["JARVIS", "HEY JARVIS"]
encoded = text2token(phrases, tokens_path, tokens_type="bpe", bpe_model=bpe_path)
with open(output_path, "w", encoding="utf-8") as output:
    for phrase, pieces in zip(phrases, encoded):
        output.write(" ".join(pieces) + " :2.0 #0.25 @" + phrase + "\n")
print("Generated wake phrases:", phrases)
PY

echo "[JARVIS] Downloading native C++ Piper phonemizer for Hindi speech output"
PIPER_URL="https://github.com/csukuangfj/piper-phonemize/releases/download/v1.6.1/piper-phonemize-v1.6.1-android.tar.bz2"
curl --fail --location --retry 3 "$PIPER_URL" -o "$TMP_DIR/piper-phonemize-android.tar.bz2"
mkdir -p "$TMP_DIR/piper"
tar -xjf "$TMP_DIR/piper-phonemize-android.tar.bz2" -C "$TMP_DIR/piper"

PIPER_CXX_LIB="$(find "$TMP_DIR/piper" -type f -name 'libpiper_phonemize_cxx_api.so' -print -quit)"
if [[ -z "$PIPER_CXX_LIB" ]]; then
  echo "Piper C++ phonemizer Android archive did not contain libpiper_phonemize_cxx_api.so."
  find "$TMP_DIR/piper" -type f | head -100
  exit 1
fi
PIPER_LIB_DIR="$(dirname "$PIPER_CXX_LIB")"
cp "$PIPER_LIB_DIR"/*.so "$ROOT/app/src/main/jniLibs/arm64-v8a/"

PIPER_HEADER_DIR="$ROOT/app/src/main/cpp/vendor/piper/include/piper-phonemize"
mkdir -p "$PIPER_HEADER_DIR"
curl --fail --location --retry 3 \
  "https://raw.githubusercontent.com/csukuangfj/piper-phonemize/v1.6.1/src/cxx-api.h" \
  -o "$PIPER_HEADER_DIR/cxx-api.h"
curl --fail --location --retry 3 \
  "https://raw.githubusercontent.com/csukuangfj/piper-phonemize/v1.6.1/src/c-api.h" \
  -o "$PIPER_HEADER_DIR/c-api.h"

echo "[JARVIS] Downloading Hindi male Piper voice (Rohan, int8)"
HINDI_TTS_URL="https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/vits-piper-hi_IN-rohan-medium-int8.tar.bz2"
curl --fail --location --retry 3 "$HINDI_TTS_URL" -o "$TMP_DIR/hindi-tts.tar.bz2"
mkdir -p "$TMP_DIR/hindi-tts"
tar -xjf "$TMP_DIR/hindi-tts.tar.bz2" -C "$TMP_DIR/hindi-tts"

HINDI_TTS_MODEL="$(find "$TMP_DIR/hindi-tts" -type f -name 'hi_IN-rohan-medium-int8.onnx' -print -quit)"
HINDI_TTS_TOKENS="$(find "$TMP_DIR/hindi-tts" -type f -name 'tokens.txt' -print -quit)"
if [[ -z "$HINDI_TTS_MODEL" || -z "$HINDI_TTS_TOKENS" ]]; then
  echo "Hindi Piper TTS package did not contain the expected model and tokens."
  find "$TMP_DIR/hindi-tts" -type f | head -100
  exit 1
fi
mkdir -p "$ROOT/app/src/main/assets/jarvis-tts"
cp "$HINDI_TTS_MODEL" "$ROOT/app/src/main/assets/jarvis-tts/model.int8.onnx"
cp "$HINDI_TTS_TOKENS" "$ROOT/app/src/main/assets/jarvis-tts/tokens.txt"

echo "[JARVIS] Downloading bundled espeak-ng phoneme data"
ESPEAK_URL="https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/espeak-ng-data.tar.bz2"
curl --fail --location --retry 3 "$ESPEAK_URL" -o "$TMP_DIR/espeak-ng-data.tar.bz2"
mkdir -p "$TMP_DIR/espeak"
tar -xjf "$TMP_DIR/espeak-ng-data.tar.bz2" -C "$TMP_DIR/espeak"
ESPEAK_PHONTAB="$(find "$TMP_DIR/espeak" -type f -name phontab -print -quit)"
if [[ -z "$ESPEAK_PHONTAB" ]]; then
  echo "espeak-ng-data archive does not contain phontab."
  find "$TMP_DIR/espeak" -type f | head -100
  exit 1
fi
ESPEAK_DATA_DIR="$(dirname "$ESPEAK_PHONTAB")"
cp -R "$ESPEAK_DATA_DIR" "$ROOT/app/src/main/assets/espeak-ng-data"

echo "[JARVIS] Native dependency preparation complete."
du -h "$ROOT/app/src/main/jniLibs/arm64-v8a/"*.so \
  "$ROOT/app/src/main/assets/sherpa-whisper-tiny/"* "$ROOT/app/src/main/assets/jarvis-kws/"* "$ROOT/app/src/main/assets/jarvis-tts/"*
