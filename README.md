# 3M DRM GS glTF: Gaussian Splats glTF Format for Multi-object Multi-layer Multi-DRM

Research and implementation workspace for a glTF packaging format that supports:

- multi-object Gaussian Splat scenes,
- multi-layer scalable Gaussian Splat objects,
- multi-DRM protection over selected objects, layers, or attributes.

## Current Inputs

- Existing multi-object and multi-DRM implementation: `/data3/isyang/Workspace/Gaussian_glTF`
- Scalable GS reference outputs and SHIN code notes: `reference/`

## Repository-Local Code

- `scripts/ply2gltf_3m_multidrm`: standalone multi-object and multi-DRM GS PLY to glTF/GLB converter.
- `scripts/scalable_ply_static`: PLY-only static layerizer that splits one non-scalable GS PLY into base and enhancement PLY layers, then can call the converter.
- `scripts/3m_gs_renderer.html`: browser-based 3M GS glTF inspection renderer for object/layer/DRM metadata.

Build it with:

```bash
cd scripts/ply2gltf_3m_multidrm
./build.sh
```

The executable is:

```bash
scripts/ply2gltf_3m_multidrm/build/bin/ply2gltf_3m_multidrm
```

## Design Docs

- `docs/research_plan.md`: research claim, milestones, implementation path, and paper framing.
- `docs/format_design.md`: proposed layer extension, DRM mapping, PLY profile decision, and validation rules.
- `docs/renderer_design.md`: current renderer scope and next renderer milestones.

## Initial Direction

The recommended v1 path is:

1. Use INRIA static GS PLY as the required baseline input.
2. Represent scalable GS as multiple INRIA PLY files: base, e1, e2, e3, and so on.
3. Package each GS object as a mesh and each scalable layer as a primitive.
4. Keep accessor-level DRM as the canonical encrypted-byte mapping.
5. Add layer-level DRM metadata for authoring, validation, and renderer behavior.
6. Treat dynamic or 4DGS PLY as a future adapter profile rather than a v1 dependency.

## Next Implementation Target

The current converter covers multi-object, static multi-layer, and multi-DRM packaging. Static scalable layers are provided through a manifest:

```bash
ply2gltf_3m_multidrm --manifest scene_3m.json output.glb
```

The manifest describes objects, transforms, layer PLY paths, layer dependencies, and DRM policies. See `scripts/ply2gltf_3m_multidrm/examples/static_multilayer_manifest.json`.

For a single non-scalable GS PLY, generate static scalable layers and GLB with:

```bash
python3 scripts/scalable_ply_static/layerize_static_gs.py input.ply /tmp/static_layers \
  --object-name object_a \
  --base-fraction 0.25 \
  --enhancement-layers 3 \
  --drm-system widevine \
  --drm-key-id 0123456789abcdef0123456789abcdef \
  --drm-key widevine_key_a \
  --convert-glb
```

## Licensing

This repository contains modified code and implementation work derived from
MPEG 3D Renderer and MPEG PLY-to-glTF. See
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and
`scripts/ply2gltf_3m_multidrm/LICENSE.upstream` for upstream attribution,
license terms, and bundled dependency notices.
