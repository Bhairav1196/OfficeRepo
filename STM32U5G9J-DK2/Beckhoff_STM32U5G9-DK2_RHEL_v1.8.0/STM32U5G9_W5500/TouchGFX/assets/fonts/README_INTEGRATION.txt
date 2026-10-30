TouchGFX source-font policy - v2.0.0-alpha.9.0

The release ZIP intentionally contains no TTF/OTF font binary.
Run Tools\PREPARE_TOUCHGFX_FONT.bat before TouchGFX Generate Code.

Required descriptor filenames:
- Roboto-Regular.ttf
- Roboto-Medium.ttf

If Roboto Medium is unavailable, the helper copies Roboto Regular to the
Medium descriptor filename so generation remains deterministic. Do not commit
or redistribute font binaries unless your organization has confirmed the
applicable license.
