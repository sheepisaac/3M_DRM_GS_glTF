# 3M DRM GS glTF Research Plan

## Working Title

3M DRM GS glTF: a glTF container for multi-object, multi-layer, and multi-DRM Gaussian Splat assets.

## Core Goal

Define and implement a glTF-based packaging format that can contain two or more Gaussian Splat objects, represent scalable Gaussian Splat layers for each object, and attach one or more DRM systems to selected objects, layers, or attributes.

## Existing Assets

- Multi-object GS glTF support: `/data3/isyang/Workspace/Gaussian_glTF/mpeg_ply2gltf/mpeg-ply2gltf_multiDRM`
- Multi-DRM metadata and encrypted accessor support: `/data3/isyang/Workspace/Gaussian_glTF/mpeg_ply2gltf/mpeg-ply2gltf_multiDRM`
- Renderer-side multi-DRM variants: `/data3/isyang/Workspace/Gaussian_glTF/mpeg_3d_renderer`
- Scalable GS reference outputs: `/data3/isyang/Workspace/3M_DRM_GS_glTF/reference/shin`
- SHIN scalable GS reference code: `/data3/isyang/Workspace/3M_DRM_GS_glTF/reference/code/SHIN`

## Main Research Claim

Existing GS glTF proposals mostly address single GS assets, progressive attribute delivery, dynamic timed buffers, or content protection independently. This work proposes a unified packaging model where:

1. A glTF scene can contain multiple GS objects as independent scene nodes or mesh primitives.
2. Each GS object can contain multiple scalable layers, such as base and enhancement layers.
3. Each object, layer, or attribute group can be protected by one or more DRM systems.
4. Receivers can render a valid baseline object before all enhancement layers or protected details are available.

## Terminology

- Multi-object: multiple independent GS assets packaged in one glTF/GLB scene.
- Multi-layer: scalable GS representation where an object is reconstructed from a base layer plus optional enhancement layers.
- Multi-DRM: multiple DRM systems, such as Widevine and PlayReady, associated with protected data ranges or accessors.
- Layer policy: metadata that describes layer order, dependency, quality level, and access requirements.
- Protection policy: metadata that maps DRM systems to encrypted accessors, bufferViews, layers, or objects.

## Proposed Milestones

### M1. Baseline Survey and Code Inventory

- Inspect existing multi-object and multi-DRM converter behavior.
- Identify reusable command-line flags and writer functions.
- Document current `EXT_gaussian_splats` and `EXT_content_protection` layout.
- Check how current renderer resolves encrypted accessors.

Deliverable: design notes and a minimal known-good command set.

### M2. Multi-Layer glTF Prototype

- Add an input model for one object with multiple layer PLY files.
- Represent each layer as a separate primitive or accessor group.
- Add layer metadata describing layer index, role, dependency, and quality.
- Support base-only rendering when enhancement layers are absent.

Deliverable: one-object, multi-layer GLB.

### M3. 3M Packaging Prototype

- Extend the multi-object path so each object can own multiple layers.
- Allow per-object transform metadata to apply consistently to all layers.
- Add per-layer and per-attribute DRM selection.
- Generate a single GLB with at least two objects and at least two layers per object.

Deliverable: multi-object, multi-layer, multi-DRM GLB.

### M4. Renderer and Validation

- Update renderer parsing for layer metadata.
- Implement toggles for base-only, base+enhancement, and DRM-unlocked rendering.
- Validate that encrypted layers do not break unprotected baseline rendering.
- Add structural validation scripts for accessors, layers, and protection mappings.

Deliverable: reproducible visual tests and JSON/GLB validation scripts.

### M5. Evaluation

- Compare file size and quality across base-only and base+enhancement configurations.
- Evaluate startup latency and incremental quality improvement.
- Compare object-level, layer-level, and attribute-level protection policies.
- Report security metadata overhead and compatibility behavior.

Deliverable: tables for rate-distortion, packaging overhead, and rendering availability.

## Recommended First Implementation Path

Start from `mpeg-ply2gltf_multiDRM`, because it already contains:

- single PLY to glTF conversion,
- sequence support,
- multi-object command-line parsing,
- `DRMConfig`,
- attribute selection for encrypted accessors,
- `EXT_content_protection` metadata.

Then add a new `--object-layer` or manifest-based input path. A manifest is preferable for the final paper because 3M scenes will quickly become hard to express cleanly as command-line flags.

## Manifest Sketch

```json
{
  "objects": [
    {
      "name": "object_0",
      "transform": {
        "translation": [0.0, 0.0, 0.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0]
      },
      "layers": [
        {
          "id": "base",
          "index": 0,
          "role": "base",
          "ply": "object_0/base.ply",
          "dependsOn": [],
          "drmPolicy": null
        },
        {
          "id": "e1",
          "index": 1,
          "role": "enhancement",
          "ply": "object_0/e1.ply",
          "dependsOn": ["base"],
          "drmPolicy": {
            "systems": ["widevine", "playready"],
            "attributes": ["sh", "scale"]
          }
        }
      ]
    }
  ]
}
```

## Paper Contribution Structure

1. A 3-axis packaging model for GS: object, layer, and DRM.
2. A glTF extension design for scalable GS layers.
3. A protection model that can target GS-specific attributes and layer dependencies.
4. A reference converter and renderer extension.
5. Experimental validation using base/enhancement rate-distortion behavior and protected rendering scenarios.

## Open Questions

- Should layers be represented as separate mesh primitives, separate nodes, or a GS-specific extension array?
- Should DRM metadata target accessors only, or should it also target layer IDs for easier policy reasoning?
- Should dynamic 4DGS be included in v1 or left as an extension-compatible future path?
- How should enhancement layers compose: additive residual splats, replacement splats, or decoder-specific fusion?
- How much of SHIN's scalable semantics should be standardized versus stored as producer metadata?
