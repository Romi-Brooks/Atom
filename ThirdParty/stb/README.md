# stb vendored headers

Atom vendors two unmodified single-header libraries from
[nothings/stb](https://github.com/nothings/stb):

| Header | Version | Atom usage | SHA-256 |
|---|---:|---|---|
| `stb_image.h` | 2.30 | `Media/Image/ImageDecoder.cpp` | `594C2FE35D49488B4382DBFAEC8F98366DEFCA819D916AC95BECF3E75F4200B3` |
| `stb_truetype.h` | 1.26 | `Render/Text/Font.cpp` | `ECD30B05E0DD4FEA3A13C26810DD9E1992DC379049482C393D5A19E6B5090AAB` |

Both headers contain their upstream dual-license text (MIT or public domain)
inside the file. Keep the headers unmodified so their version and license stay
auditable. The implementation macros are defined in exactly one Atom source
file per header; consumers link the `Atom_3rd_Stb` interface target and must not
define those macros themselves.

`stb_truetype` explicitly does not validate hostile font offsets. Atom's
current `Font` wrapper is therefore for trusted game assets only, not fonts
received from users or the network. Dependency updates must replace the whole
header, update the versions and hashes above, then rebuild the image,
Renderer2D and MusicCard examples.
