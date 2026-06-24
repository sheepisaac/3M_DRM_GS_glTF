#!/usr/bin/env bash
set -euo pipefail

ROOT="/data3/isyang/Workspace/3M_DRM_GS_glTF"
CONVERTER="$ROOT/scripts/ply2gltf_3m_multidrm/build/bin/ply2gltf_3m_multidrm"
LAYERIZER="$ROOT/scripts/scalable_ply_static/layerize_static_gs.py"
INPUT_PLY="$ROOT/data/gs/tango_duo.ply"
LAYER_DIR="$ROOT/output/layered_play"
GLTF_DIR="$ROOT/output/gltf"
MANIFEST="$ROOT/scripts/ply2gltf_3m_multidrm/examples/tango_duo_layer_multidrm_manifest.json"

mkdir -p "$LAYER_DIR" "$GLTF_DIR"

python3 "$LAYERIZER" "$INPUT_PLY" "$LAYER_DIR" \
  --object-name tango_duo \
  --profile shin-lod \
  --enhancement-layers 3

"$CONVERTER" --manifest "$MANIFEST" \
  "$GLTF_DIR/tango_duo_3m_layer_multidrm.glb"

echo "Wrote $GLTF_DIR/tango_duo_3m_layer_multidrm.glb"
