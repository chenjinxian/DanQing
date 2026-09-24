KhronosGroup/glTF-Sample-Assets (https://github.com/KhronosGroup/glTF-Sample-Assets) —
selected models vendored for DanQing glTF import verification (spec §7.3).
BoxTextured: Cesium logo texture; per-upstream license/credit — see upstream repo
Models/BoxTextured/README.md (CC-BY 4.0 per upstream). Vendor date: 2026-09-15.

Vendored files (upstream names, Models/BoxTextured/glTF/): BoxTextured.gltf,
BoxTextured0.bin (external buffer, 840 bytes), CesiumLogoFlat.png (baseColorTexture,
PNG magic 89 50 4E 47). The .gltf references both companion files by relative URI —
do not rename them. Upstream filenames differ from the names briefly assumed during
task planning (BoxTextured.bin/BoxTextured.png): those 404; the vendored set above
is the real upstream glTF/ directory listing.

BoxTexturedDots (DanQing-authored, NOT upstream): BoxTextured with the texture
replaced by a white-F + blue-dot(top-left) + red-dot(top-right) marker image
(256x256 RGB PNG) for the depth-inversion pixel regression
(GltfStandardView.TopViewRendersTopFaceNotBottom). The dots are unambiguous
face-orientation markers: under the correct (reference) projection the Top view
shows the +Z face, whose UV assignment places the blue dot at the face's
screen-right-bottom; under depth inversion (m22>0 + LEQUAL = farthest surface
wins, fixed 2026-09-16) the -Z face shows instead and the dots swap sides.
Authored per CLAUDE.md 5(g) — window/render behavior regression; no reference
test exists in itwinjs-core for viewport depth ordering of decorations.
