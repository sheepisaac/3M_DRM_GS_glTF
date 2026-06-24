# ply2gltf_3m_multidrm

Repository-local converter for the 3M DRM GS glTF project.

This tool is copied from the prior `Gaussian_glTF` multiDRM converter and maintained here so the research repository can build and run independently.

## What This Version Covers

- Single INRIA Gaussian Splat PLY to GLB/glTF.
- Multi-object GLB packaging with one glTF mesh per object.
- Per-object transform metadata.
- Per-object encrypted attribute selection.
- Container-level MultiDRM through per-object DRM selection.
- Optional commercial-style packaging with multiple DRM systems on one object by repeating `--object-drm`.
- Global MultiDRM metadata for single-object conversion.

## Vendored Dependencies

The minimal source needed for these dependencies is included under `external/`:

- RapidJSON headers
- tinyply source

CMake will use the vendored copies when present. If they are absent, it falls back to cloning the pinned upstream versions.

## Build

```bash
cd scripts/ply2gltf_3m_multidrm
./build.sh
```

The executable is produced at:

```bash
scripts/ply2gltf_3m_multidrm/build/bin/ply2gltf_3m_multidrm
```

## Multi-Object + MultiDRM Example

The v1 research interpretation of MultiDRM is: one glTF/GLB container can package multiple GS objects, and each object can choose its own DRM system.

```bash
./build/bin/ply2gltf_3m_multidrm --multi-object output.glb \
  --object object_a /path/to/object_a.ply \
  --transform 0 0 0 0 0 0 1 1 1 \
  --object-drm widevine \
  --object-drm-key-id 0123456789abcdef0123456789abcdef \
  --object-drm-key widevine_key_a \
  --object-drm-encrypted-attributes sh,scale \
  --object object_b /path/to/object_b.ply \
  --transform 1 0 0 0 0 0 1 1 1 \
  --object-drm playready \
  --object-drm-key-id 22222222222222222222222222222222 \
  --object-drm-key playready_key_b \
  --object-drm-encrypted-attributes sh,scale \
  --verbose
```

In this example, `object_a` uses Widevine metadata and `object_b` uses PlayReady metadata in the same glTF container.

## Advanced Per-Object MultiDRM

For commercial-service compatibility experiments, the converter also allows repeating `--object-drm` for the same object. In that mode, the first DRM system listed for an object is used for the test byte transform, and all listed DRM systems are written into `EXT_content_protection.systems` with the same encrypted accessor set. Renderer support for this advanced mode should be treated as a later milestone.

## Next 3M Extension Step

The converter now supports manifest-driven input for static multi-layer GS:

```bash
./build/bin/ply2gltf_3m_multidrm --manifest scene_3m.json output.glb
```

That mode adds object/layer records and produces root-level `EXT_gaussian_splat_layers` metadata. Static scalable layers are encoded as one glTF mesh per object and one primitive per layer.

See:

```bash
examples/static_multilayer_manifest.json
```
