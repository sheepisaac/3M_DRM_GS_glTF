# Static PLY-Only Scalable GS Layerizer

This tool gives a non-scalable INRIA-style GS PLY a static scalable structure:

- `base`: highest-scoring splats
- `e1`, `e2`, ...: disjoint enhancement layers from the remaining splats
- manifest for `ply2gltf_3m_multidrm --manifest`

The default profile is `shin-lod`. It follows the LoD budget pattern used in the SHIN reference code:

- base ratio: `0.25`, matching SHIN's default `keep_ratio_stageA`
- enhancement ratios: `0.03,0.02,0.01`, matching SHIN's default `layer_keep_ratios`
- enhancement modes: `lf,mix,hf`, matching SHIN's default `layer_modes`
- mode weights:
  - `lf`: low/mid/high = `1.0,0.35,0.10`
  - `mix`: low/mid/high = `0.45,1.0,0.35`
  - `hf`: low/mid/high = `0.10,0.55,1.0`

It is still not a full SHIN reproduction. Full SHIN residual scoring requires the original scene, cameras, GT images, and differentiable GS renderer. This tool substitutes PLY-local low/mid/high proxy scores built from opacity, scale, DC color, and SH-rest energy.

## Usage

```bash
python3 scripts/scalable_ply_static/layerize_static_gs.py \
  input.ply \
  /tmp/static_layers \
  --object-name object_a \
  --profile shin-lod \
  --base-fraction 0.25 \
  --layer-keep-ratios 0.03,0.02,0.01 \
  --layer-modes lf,mix,hf
```

To generate GLB in one command:

```bash
python3 scripts/scalable_ply_static/layerize_static_gs.py \
  input.ply \
  /tmp/static_layers \
  --object-name object_a \
  --profile shin-lod \
  --base-fraction 0.25 \
  --layer-keep-ratios 0.03,0.02,0.01 \
  --layer-modes lf,mix,hf \
  --drm-system widevine \
  --drm-key-id 0123456789abcdef0123456789abcdef \
  --drm-key widevine_key_a \
  --convert-glb
```

The generated manifest sets `drmPolicy: null` for the base layer so object-level DRM protects only enhancement layers by default.

For comparison experiments, `--profile heuristic-even` keeps the older behavior: one base split plus evenly divided enhancement layers.
