# 3M GS glTF Renderer

The first repository-local renderer is a browser-based inspection viewer:

```text
scripts/3m_gs_renderer.html
```

It targets the GLB structure produced by `ply2gltf_3m_multidrm`:

- root-level `EXT_gaussian_splat_layers`
- one mesh/node per GS object
- one primitive per static layer
- `EXT_gaussian_splats` primitive attributes
- optional `EXT_content_protection` on protected primitives

## Current Scope

The viewer renders each GS layer as a colored point cloud using `POSITION` and `COLOR_0`. It does not yet implement full Gaussian rasterization, SH view-dependent shading, or real DRM decryption. This makes it useful as a structural 3M renderer and verifier before implementing the final splat renderer.

Supported interactions:

- load a local GLB file
- inspect objects and layers
- toggle individual layers
- show base-only rendering
- show all unprotected layers
- mark DRM-protected layers as locked
- simulate DRM unlock for renderer-control testing

## Next Renderer Milestones

1. Replace point rendering with actual Gaussian splat rendering.
2. Decode `_GS_SCALE`, `_GS_ORIENTATION`, and SH attributes.
3. Sort splats or use an order-independent approximation.
4. Add real DRM-policy handling once encryption/decryption semantics are fixed.
5. Add multi-object scene controls and object transforms to match the paper experiments.
