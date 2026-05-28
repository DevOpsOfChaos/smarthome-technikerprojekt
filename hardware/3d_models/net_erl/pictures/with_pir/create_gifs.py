
#!/usr/bin/env python3
"""
GIF Creator - Deckenlampe PIR
Benoetigt: pip install pillow
Ausfuehren: python create_gifs.py
"""
from PIL import Image
import os

FOLDER = os.path.dirname(os.path.abspath(__file__))
BG_COLOR = (0, 0, 0, 255)  # Schwarz

def make_gif(frame_prefix, output_name, duration_ms=80, loop=0):
    frames = sorted([f for f in os.listdir(FOLDER)
                     if f.startswith(frame_prefix) and f.endswith('.png')])
    if not frames:
        print(f'No frames found for prefix: {frame_prefix}')
        return
    images = []
    for f in frames:
        img = Image.open(os.path.join(FOLDER, f)).convert('RGBA')
        bg  = Image.new('RGBA', img.size, BG_COLOR)
        bg.paste(img, mask=img.split()[3])
        images.append(bg.convert('P', palette=Image.ADAPTIVE, colors=256))
    output_path = os.path.join(FOLDER, output_name)
    images[0].save(
        output_path,
        save_all=True,
        append_images=images[1:],
        duration=duration_ms,
        loop=loop,
        optimize=False
    )
    sz = os.path.getsize(output_path) // 1024
    print(f'Created: {output_name}  ({len(images)} frames @ {duration_ms}ms = {len(images)*duration_ms/1000:.1f}s  |  {sz} KB)')

if __name__ == '__main__':
    # GIF 1: 72 frames - cinematic 360 mit Elevation-Welle (~5.8s)
    make_gif('gif1_frame_', 'gif1_rotation_360.gif', duration_ms=80)

    # GIF 2: 60 frames - top to bottom sweep (~6.0s)
    make_gif('gif2_frame_', 'gif2_top_to_bottom.gif', duration_ms=100)

    # GIF 3: 120 frames - Explosion: oben, unten, 360 Grad (~10.8s)
    make_gif('gif3_frame_', 'gif3_explosion_rotation.gif', duration_ms=90)

    print('\nAll GIFs created!')
