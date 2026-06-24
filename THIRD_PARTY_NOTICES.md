# Third-Party Notices

This repository contains modified code and implementation ideas derived from
third-party projects. The notices below identify the upstream sources and the
license files retained in this repository.

## MPEG 3D Renderer

Parts of this repository, including Gaussian Splat rendering behavior used by
`scripts/3m_gs_renderer.html`, are derived from or based on the MPEG 3D
Renderer.

- Upstream source:
  `https://git.mpeg.expert/MPEG/Systems/SceneDescription/software/3dgs/mpeg-3d-renderer`
- Upstream commit:
  `d282da4cfd44a6e10ca05116c7d2672048917ce6`
- Copyright: 2016-2025 InterDigital
- License: BSD license preserved in
  `scripts/ply2gltf_3m_multidrm/LICENSE.upstream`

The upstream license notes that the software may be subject to InterDigital
and other third-party or contributor rights, including patent rights, and that
no patent rights are granted under that license.

## MPEG PLY-to-glTF

`scripts/ply2gltf_3m_multidrm` is derived from MPEG PLY-to-glTF.

- Upstream source:
  `https://git.mpeg.expert/MPEG/Systems/SceneDescription/software/3dgs/mpeg-ply2gltf`
- Upstream commit:
  `4ab00b68abca1aef9cd792b71a376e01e7466709`
- Upstream license statement: the same license as the MPEG 3D Renderer project
- License copy:
  `scripts/ply2gltf_3m_multidrm/LICENSE.upstream`

This repository modifies and extends the upstream converter with multi-object
packaging, static multi-layer Gaussian Splats, manifest-driven conversion,
per-layer and multi-system DRM metadata, and the proposed 3M GS glTF workflow.

## RapidJSON

RapidJSON is vendored under
`scripts/ply2gltf_3m_multidrm/external/rapidjson`.

Its copyright, MIT license, and included third-party notices are preserved in:

`scripts/ply2gltf_3m_multidrm/external/rapidjson/license.txt`

## TinyPLY

TinyPLY is vendored under
`scripts/ply2gltf_3m_multidrm/external/tinyply`.

Its public-domain dedication and alternative 2-clause BSD terms are documented
in:

`scripts/ply2gltf_3m_multidrm/external/tinyply/readme.md`

