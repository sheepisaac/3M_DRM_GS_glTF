#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "Usage: $0 OBJECT_A.ply OBJECT_B.ply OUTPUT.glb"
  exit 1
fi

OBJECT_A="$1"
OBJECT_B="$2"
OUTPUT="$3"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONVERTER="$SCRIPT_DIR/../build/bin/ply2gltf_3m_multidrm"

"$CONVERTER" --multi-object "$OUTPUT" \
  --object object_a "$OBJECT_A" \
  --transform 0 0 0 0 0 0 1 1 1 \
  --object-drm widevine \
  --object-drm-key-id 0123456789abcdef0123456789abcdef \
  --object-drm-key widevine_key_a \
  --object-drm-encrypted-attributes sh,scale \
  --object object_b "$OBJECT_B" \
  --transform 1 0 0 0 0 0 1 1 1 \
  --object-drm playready \
  --object-drm-key-id 22222222222222222222222222222222 \
  --object-drm-key playready_key_b \
  --object-drm-encrypted-attributes sh,scale \
  --verbose
