# 3M DRM GS glTF Format Design

## Design Principle

The format should keep ordinary glTF behavior valid while adding GS-specific semantics through extensions. A receiver that understands only basic `EXT_gaussian_splats` should still find renderable base primitives. A 3M-aware receiver can additionally resolve layer dependencies and DRM policies.

## Baseline GS Mapping

The inherited baseline follows the existing GS glTF mapping:

- `POSITION`: splat center
- `COLOR_0`: DC color and opacity
- `_GS_ORIENTATION`: quaternion
- `_GS_SCALE`: Gaussian scale
- `_GS_SH_COEFF_FIRST`: first-order SH coefficients
- `_GS_SH_COEFF_SECOND`: second-order SH coefficients
- `_GS_SH_COEFF_THIRD`: third-order SH coefficients

This is compatible with the current converter family in `Gaussian_glTF`.

## Proposed Extension Split

### `EXT_gaussian_splats`

Keep this as the low-level per-primitive GS attribute extension.

### `EXT_gaussian_splat_layers`

Add scalable layer semantics. This extension can live at mesh, primitive, or root level. For 3M packaging, root-level metadata with primitive references is easiest to validate.

Example:

```json
{
  "extensions": {
    "EXT_gaussian_splat_layers": {
      "objects": [
        {
          "id": "object_0",
          "name": "Cricket",
          "layers": [
            {
              "id": "object_0_base",
              "layerIndex": 0,
              "role": "base",
              "mesh": 0,
              "primitive": 0,
              "dependsOn": []
            },
            {
              "id": "object_0_e1",
              "layerIndex": 1,
              "role": "enhancement",
              "mesh": 0,
              "primitive": 1,
              "dependsOn": ["object_0_base"],
              "composition": "additive"
            }
          ]
        }
      ]
    }
  }
}
```

### `EXT_content_protection`

Reuse the existing protection model, but extend the metadata so policies can reference layers in addition to accessors.

Example:

```json
{
  "extensions": {
    "EXT_content_protection": {
      "systems": [
        {
          "schemeIdUri": "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd5121ed8",
          "keyId": "0123456789abcdef0123456789abcdef",
          "encryptedAccessors": [7, 8, 9],
          "encryptedLayers": ["object_0_e1"]
        },
        {
          "schemeIdUri": "urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95",
          "keyId": "fedcba9876543210fedcba9876543210",
          "encryptedAccessors": [7, 8, 9],
          "encryptedLayers": ["object_0_e1"]
        }
      ]
    }
  }
}
```

Accessor-level mapping is still the normative encryption target because the encrypted bytes live in buffers. Layer-level mapping is a policy convenience for packaging, renderer UI, and validation.

## Layer Representation Options

### Option A. One Primitive Per Layer

Each object is one mesh; each layer is a primitive within that mesh.

Pros:

- Simple to map layers to accessors.
- Natural for base+enhancement visibility toggles.
- Keeps object transform shared at the node level.

Cons:

- Standard renderers may render all primitives unless layer-aware filtering is implemented.

### Option B. One Node Per Layer

Each layer is its own node and mesh.

Pros:

- Easy visibility control using node enable/disable.
- Clear transform inheritance.

Cons:

- Object identity is more scattered.
- More verbose for many objects and layers.

### Option C. One Primitive With Layered Accessors

A single primitive references arrays of layer accessors through a custom extension.

Pros:

- Cleanest semantic model.
- Can avoid accidental rendering of enhancement-only data by legacy renderers.

Cons:

- Requires the most custom renderer logic.
- Less compatible with current converter code.

Recommendation for v1: Option A. Use one mesh per object and one primitive per layer. It is the best balance for implementation speed, validation, and paper clarity.

## PLY Format Decision

### Recommendation

Use pure INRIA GS PLY as the required v1 input format, and define an optional adapter profile for dynamic or 4DGS PLY.

### Rationale

INRIA PLY should be the normative baseline because:

- it matches the current converter implementation,
- it is the most reproducible input for static GS,
- it avoids mixing the paper's core contribution with dynamic-GS schema debates,
- scalable layers can be represented as multiple static GS PLY files.

4DGS should be considered an extension path because:

- dynamic attributes vary significantly across 4DGS implementations,
- the existing code already has MPEG timed accessor support for sequences,
- dynamic support can be described as compatible but not required for the first 3M proof.

### Proposed Profiles

- `profile: inria_static_v1`: required baseline.
- `profile: inria_layered_v1`: multiple INRIA PLY files representing base and enhancement layers.
- `profile: dynamic_sequence_v0`: multiple frame PLY files using existing MPEG timed accessors.
- `profile: fourdgs_adapter_v0`: future adapter profile for additional time/deformation fields.

## DRM Policy Granularity

Recommended policy order:

1. Attribute-level encryption for sensitive details such as SH and scale.
2. Layer-level policy metadata for access control and graceful fallback.
3. Object-level defaults for simple authoring.

The converter should resolve object defaults into concrete layer and accessor encryption decisions.

For the v1 research system, MultiDRM means that a single glTF/GLB container can include multiple GS objects where each object chooses one DRM system, such as object A using Widevine and object B using PlayReady. Packaging multiple DRM systems for the same object is useful for commercial service compatibility, but it should be treated as an advanced profile because renderer-side license selection and decryption policy become more involved.

## Minimal Valid 3M Asset

A minimal demonstration asset should contain:

- two GS objects,
- each object with one base layer and one enhancement layer,
- unencrypted base layers,
- protected enhancement layers,
- two DRM systems present in the same container through different protected objects or layers,
- renderer support for base-only and unlocked enhanced rendering.

## Current Static Multi-Layer Implementation

The repository-local converter supports static multi-layer packaging through a JSON manifest:

```bash
scripts/ply2gltf_3m_multidrm/build/bin/ply2gltf_3m_multidrm --manifest scene_3m.json output.glb
```

The current implementation maps each object to one glTF mesh/node and each static layer to one mesh primitive. It writes root-level `EXT_gaussian_splat_layers` metadata that records object IDs, layer IDs, layer roles, primitive references, dependency IDs, and composition mode.

For SHIN-style scalable GS references, each `base`, `e1`, `e2`, or `e3` layer is represented as a separate INRIA-style static GS PLY. This keeps the converter independent from SHIN training code while matching the generated layer artifact structure.

The repository also includes a PLY-only static layerizer:

```bash
python3 scripts/scalable_ply_static/layerize_static_gs.py input.ply out_dir --convert-glb
```

This layerizer gives a non-scalable GS PLY a scalable structure without requiring cameras or ground-truth images. Its default `shin-lod` profile mirrors SHIN's LoD budget pattern: base `keep_ratio_stageA=0.25`, enhancement `layer_keep_ratios=0.03,0.02,0.01`, and `layer_modes=lf,mix,hf` with SHIN's low/mid/high weights. Because PLY-only input has no rendered residual maps, it approximates SHIN's low/mid/high layer scores using PLY-local attributes: opacity, scale, DC color energy, and SH-rest energy. This is useful for format and packaging experiments, but it is not a full reproduction of SHIN's residual-driven layer induction. A full SHIN integration would require the original 3DGS training stack, differentiable rasterizer, camera set, and GT images.

DRM policy inheritance is explicit:

- Object-level `drmPolicy` is inherited by layers by default.
- Layer-level `drmPolicy` overrides the object policy.
- Layer-level `"drmPolicy": null` blocks inheritance, which enables unencrypted base layers with encrypted enhancement layers.

## Implementation Notes

- Use manifest input before adding many command-line flags.
- Reuse `DRMConfig` and `isAttributeEncrypted` from the current multiDRM code.
- Generalize object metadata so an object owns `vector<LayerInput>` rather than a single PLY path.
- Generate accessors per layer, then collect accessor IDs into `LayerRecord`.
- Write `EXT_gaussian_splat_layers` after accessors and primitives are finalized.
- Keep `encryptedAccessors` as the canonical byte-level protection list.

## Validation Checks

- Every layer references an existing mesh and primitive.
- Every non-base layer has a valid dependency chain.
- Every encrypted layer has at least one encrypted accessor.
- Every encrypted accessor belongs to at least one declared DRM system.
- Base layers can be rendered without encrypted data.
- Object transforms apply consistently to all layers.
