# UV Cutout Tool

A lightweight tool for creating texture cutouts from Skyrim NIF mesh UV layouts.

## What It Does

Loads one or more NIF meshes and a matching texture, displays the UV layout, lets you select islands, and exports those texture regions as a transparent PNG or TGA.

Useful for isolating armor pieces, weapons, leather, trim, ornaments, or any set of Skyrim meshes that share one diffuse map.

## Multiple NIFs

Some Skyrim textures are shared by several NIF meshes. UV Cutout Tool can load multiple NIFs into the same workspace so you can cut from one shared texture while viewing all relevant UV layouts together.

When more than one NIF is loaded, the right-hand Shapes panel groups each shape under its source NIF file name. With only one NIF loaded, the panel stays simple and does not add an extra file-name category.

## Quick Start

1. Open the program.
2. Load one or more NIF meshes (or drag them in).
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
- Additional NIFs append to the current workspace until you return to the welcome screen.
- Only standard DDS BC1-BC7 compressions supported.
- Requires the included NiflyDLL.dll.

## Credits

- Qt 6 - UI framework
- DirectXTex - DDS decoding reference
- Nifly - NIF loading
