# UV Cutout Tool

A lightweight tool for creating texture cutouts from Skyrim NIF mesh UV layouts.

## What It Does

Loads a NIF mesh and its texture, displays the UV layout, lets you select islands, and exports those texture regions as a transparent PNG or TGA.

Useful for isolating armor pieces, weapons, leather, trim, ornaments, or anything that shares one diffuse map.

## Quick Start

1. Open the program.
2. Load a NIF mesh (or drag it in).
3. Load the matching texture (or drag it in).
4. Click or drag-select the UV islands you want.
5. Export as PNG or TGA.

Selected areas export with texture. Everything else is transparent.

## Controls

- **Left click** - Toggle island selection
- **Left drag** - Box select islands
- **Mouse wheel** - Zoom
- **Middle drag / Space+drag** - Pan
- **Right drag** - Pan (when zoomed in)
- **Ctrl+Z** - Undo
- **Ctrl+Shift+Z** - Redo

## Supported Files

- **Meshes:** NIF
- **Textures:** DDS, PNG, TGA, JPG, BMP
- **Exports:** PNG, TGA

## Notes

- Export requires both a mesh and texture loaded.
- Only standard DDS BC1-BC7 compressions supported.
- Requires the included NiflyDLL.dll.

## Credits

- Qt 6 - UI framework
- DirectXTex - DDS decoding reference
- Nifly - NIF loading