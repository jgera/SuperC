import math
from PIL import Image, ImageDraw, ImageFont

def create_superc_icon(filename="app.ico"):
    base_size = 1024
    img = Image.new("RGBA", (base_size, base_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 1. Outer rounded squircle background
    pad = 48
    corner_radius = 240
    rect = [pad, pad, base_size - pad, base_size - pad]

    # Draw gradient or rich layered background
    # Dark modern surface: #12161f to #1a2232
    for i in range(12):
        r = [pad - i, pad - i, base_size - pad + i, base_size - pad + i]
        alpha = int(25 * (1 - i / 12))
        draw.rounded_rectangle(r, radius=corner_radius + i, fill=(0, 210, 255, alpha))

    # Base dark container
    draw.rounded_rectangle(rect, radius=corner_radius, fill=(18, 22, 32, 255), outline=(0, 180, 255, 230), width=18)

    # 2. Draw stylized "C" and Chevron ">"
    # Outer circle of C: center (512, 512), radius ~300
    cx, cy = 490, 512
    outer_r = 300
    inner_r = 180

    # Draw C as a thick arc from angle 45 to 315 degrees
    # In PIL, arc bbox is [left, top, right, bottom]
    # We can draw multiple pie slices or a smooth polygon
    points_outer = []
    points_inner = []
    
    start_deg = 48
    end_deg = 312
    steps = 100
    
    for i in range(steps + 1):
        deg = start_deg + (end_deg - start_deg) * (i / steps)
        rad = math.radians(deg)
        x = cx + outer_r * math.cos(rad)
        y = cy - outer_r * math.sin(rad)
        points_outer.append((x, y))

    for i in range(steps + 1):
        deg = end_deg - (end_deg - start_deg) * (i / steps)
        rad = math.radians(deg)
        x = cx + inner_r * math.cos(rad)
        y = cy - inner_r * math.sin(rad)
        points_inner.append((x, y))

    poly = points_outer + points_inner
    draw.polygon(poly, fill=(255, 255, 255, 255))

    # Rounded end caps for C
    # Cap 1 (top right)
    deg1 = math.radians(start_deg)
    mid_r = (outer_r + inner_r) / 2
    cap_r = (outer_r - inner_r) / 2
    cap1_x = cx + mid_r * math.cos(deg1)
    cap1_y = cy - mid_r * math.sin(deg1)
    draw.ellipse([cap1_x - cap_r, cap1_y - cap_r, cap1_x + cap_r, cap1_y + cap_r], fill=(255, 255, 255, 255))

    # Cap 2 (bottom right)
    deg2 = math.radians(end_deg)
    cap2_x = cx + mid_r * math.cos(deg2)
    cap2_y = cy - mid_r * math.sin(deg2)
    draw.ellipse([cap2_x - cap_r, cap2_y - cap_r, cap2_x + cap_r, cap2_y + cap_r], fill=(255, 255, 255, 255))

    # 3. Draw vibrant electric Chevron ">" inside the right opening
    chev_cx = 640
    chev_cy = 512
    w = 120
    h = 160
    thick = 48

    # Arrow points
    # (top_left, tip_outer, bottom_left, bottom_inner, tip_inner, top_inner)
    arrow = [
        (chev_cx - w, chev_cy - h),
        (chev_cx + w, chev_cy),
        (chev_cx - w, chev_cy + h),
        (chev_cx - w + thick, chev_cy + h),
        (chev_cx + w - thick, chev_cy),
        (chev_cx - w + thick, chev_cy - h)
    ]
    draw.polygon(arrow, fill=(0, 210, 255, 255))

    # Generate multi-resolution icons
    sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
    icon_images = [img.resize(s, Image.Resampling.LANCZOS) for s in sizes]

    icon_images[0].save(
        filename,
        format="ICO",
        sizes=sizes,
        append_images=icon_images[1:]
    )
    print(f"Saved {filename} with sizes {sizes}")

if __name__ == "__main__":
    create_superc_icon("src/resources/app.ico")
    create_superc_icon("app.ico")
