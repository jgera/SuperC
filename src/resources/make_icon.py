import math
from PIL import Image, ImageDraw, ImageFilter

def create_superc_icon(filename="app.ico"):
    S = 2048
    base = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    pad = 140
    size = S - 2 * pad
    radius = 440

    # 1. Squircle ambient drop shadow
    shadow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    s_draw = ImageDraw.Draw(shadow)
    s_draw.rounded_rectangle([pad, pad + 60, S - pad, S - pad + 60], radius=radius, fill=(0, 0, 0, 190))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=80))
    base = Image.alpha_composite(base, shadow)

    # 2. Squircle dark body (deep obsidian #10141d to #181d28)
    body = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    b_draw = ImageDraw.Draw(body)
    for y in range(pad, S - pad):
        f = (y - pad) / size
        r = int(24 - f * 12)
        g = int(28 - f * 14)
        b = int(40 - f * 20)
        b_draw.line([(pad, y), (S - pad, y)], fill=(r, g, b, 255), width=1)

    body_mask = Image.new('L', (S, S), 0)
    bm_draw = ImageDraw.Draw(body_mask)
    bm_draw.rounded_rectangle([pad, pad, S - pad, S - pad], radius=radius, fill=255)

    squircle = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    squircle.paste(body, (0, 0), body_mask)
    base = Image.alpha_composite(base, squircle)

    # 3. Outer border: Smooth luminous neon cyan rim
    border_layer = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    bd_draw = ImageDraw.Draw(border_layer)
    bd_draw.rounded_rectangle([pad, pad, S - pad, S - pad], radius=radius, outline=(0, 225, 255, 220), width=16)
    base = Image.alpha_composite(base, border_layer)

    # 4. Ambient Top Glow
    glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    g_draw = ImageDraw.Draw(glow)
    g_draw.ellipse([S//2 - 600, pad - 200, S//2 + 600, pad + 800], fill=(0, 210, 255, 45))
    glow = glow.filter(ImageFilter.GaussianBlur(radius=150))
    glow_masked = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    glow_masked.paste(glow, (0, 0), body_mask)
    base = Image.alpha_composite(base, glow_masked)

    # 5. Centered Supersonic 'C' Lettermark
    cx, cy = 1024, 1024
    outer_r = 580
    inner_r = 300

    start_deg = 36
    end_deg = 324
    steps = 200

    outer_pts = []
    inner_pts = []
    for i in range(steps + 1):
        deg = start_deg + (end_deg - start_deg) * (i / steps)
        rad = math.radians(deg)
        ox = cx + outer_r * math.cos(rad)
        oy = cy - outer_r * math.sin(rad)
        ix = cx + inner_r * math.cos(rad)
        iy = cy - inner_r * math.sin(rad)
        outer_pts.append((ox, oy))
        inner_pts.append((ix, iy))

    c_mask = Image.new('L', (S, S), 0)
    cm_draw = ImageDraw.Draw(c_mask)
    cm_draw.polygon(outer_pts + inner_pts[::-1], fill=255)

    # Rounded aerodynamic terminal caps
    cap_r = (outer_r - inner_r) / 2
    mid_r = (outer_r + inner_r) / 2
    for deg in [start_deg, end_deg]:
        rad = math.radians(deg)
        cpx = cx + mid_r * math.cos(rad)
        cpy = cy - mid_r * math.sin(rad)
        cm_draw.ellipse([cpx - cap_r, cpy - cap_r, cpx + cap_r, cpy + cap_r], fill=255)

    # Fill 'C' with gradient: Neon Cyan -> Azure -> Indigo/Violet
    c_surf = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    cs_draw = ImageDraw.Draw(c_surf)
    for y in range(int(cy - outer_r - 20), int(cy + outer_r + 20)):
        t = (y - (cy - outer_r)) / (2 * outer_r)
        t = max(0.0, min(1.0, t))
        if t < 0.5:
            k = t / 0.5
            cr = 0
            cg = int(242 - k * 98)
            cb = 255
        else:
            k = (t - 0.5) / 0.5
            cr = int(0 + k * 121)
            cg = int(144 - k * 104)
            cb = int(255 - k * 53)
        cs_draw.line([(0, y), (S, y)], fill=(cr, cg, cb, 255), width=1)

    c_final = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    c_final.paste(c_surf, (0, 0), c_mask)

    # Drop shadow for glyph
    g_shadow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    gs_draw = ImageDraw.Draw(g_shadow)
    gs_draw.bitmap((0, 36), c_mask, fill=(0, 0, 0, 190))
    g_shadow = g_shadow.filter(ImageFilter.GaussianBlur(radius=40))

    base = Image.alpha_composite(base, g_shadow)
    base = Image.alpha_composite(base, c_final)

    # Multi-resolution ICO export
    sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
    icon_images = [base.resize(s, Image.Resampling.LANCZOS) for s in sizes]

    icon_images[0].save(
        filename,
        format="ICO",
        sizes=sizes,
        append_images=icon_images[1:]
    )
    print(f"Successfully generated {filename} with sizes {sizes}")

if __name__ == "__main__":
    create_superc_icon("src/resources/app.ico")
    create_superc_icon("app.ico")
