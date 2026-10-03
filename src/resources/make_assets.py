import os
import math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

FONT_REG = "C:\\Windows\\Fonts\\segoeui.ttf"
FONT_SEMI = "C:\\Windows\\Fonts\\seguisb.ttf"
FONT_BOLD = "C:\\Windows\\Fonts\\segoeuib.ttf"
FONT_MONO = "C:\\Windows\\Fonts\\consola.ttf"

def draw_keycap_right(draw, right_x, y, key, action, dark=True):
    font_hint = ImageFont.truetype(FONT_SEMI, 12)
    font_key = ImageFont.truetype(FONT_BOLD, 10)

    key_bg = (36, 42, 56) if dark else (241, 245, 249)
    key_border = (58, 66, 88) if dark else (203, 213, 225)
    key_text = (226, 232, 240) if dark else (30, 41, 59)
    label_text = (148, 163, 184) if dark else (71, 85, 105)

    bbox_k = font_key.getbbox(key)
    kw = (bbox_k[2] - bbox_k[0]) + 14
    kh = 18

    bbox_a = font_hint.getbbox(action)
    aw = bbox_a[2] - bbox_a[0]

    total_w = kw + 5 + aw
    start_x = right_x - total_w

    # Keycap pill
    draw.rounded_rectangle([start_x, y, start_x + kw, y + kh], radius=5, fill=key_bg, outline=key_border, width=1)
    draw.text((start_x + 7, y + 2), key, fill=key_text, font=font_key)

    # Label
    draw.text((start_x + kw + 5, y + 2), action, fill=label_text, font=font_hint)

    return start_x - 12

def render_launcher_card(query, badge, primary, secondary, dark=True):
    W = 680
    H = 148

    bg_col = (16, 18, 24) if dark else (238, 242, 246)
    border_col = (45, 52, 68) if dark else (203, 213, 225)
    chevron_col = (0, 225, 255) if dark else (0, 130, 220)

    pill_bg = (28, 32, 44) if dark else (255, 255, 255)
    pill_border = (55, 64, 85) if dark else (203, 213, 225)

    card_bg = (22, 25, 34) if dark else (255, 255, 255)
    card_border = (45, 52, 68) if dark else (203, 213, 225)

    primary_col = (255, 255, 255) if dark else (15, 23, 42)
    secondary_col = (148, 163, 184) if dark else (71, 85, 105)

    badge_bg = (12, 45, 72) if dark else (224, 242, 254)
    badge_border = (2, 132, 199) if dark else (125, 211, 252)
    badge_text = (56, 189, 248) if dark else (3, 105, 161)

    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Outer window
    draw.rounded_rectangle([0, 0, W - 1, H - 1], radius=18, fill=bg_col, outline=border_col, width=1)

    # Top search pill
    draw.rounded_rectangle([6, 6, W - 6, 50], radius=14, fill=pill_bg, outline=pill_border, width=1)

    # Chevron
    font_chev = ImageFont.truetype(FONT_BOLD, 22)
    draw.text((20, 12), ">", fill=chevron_col, font=font_chev)

    # Query text
    font_input = ImageFont.truetype(FONT_SEMI, 19)
    draw.text((46, 14), query, fill=primary_col, font=font_input)

    # Cursor
    bbox = font_input.getbbox(query)
    cx = 46 + (bbox[2] - bbox[0]) + 3
    draw.line([(cx, 16), (cx, 40)], fill=chevron_col if dark else (15, 23, 42), width=2)

    # Bottom result card
    draw.rounded_rectangle([6, 58, W - 6, H - 6], radius=14, fill=card_bg, outline=card_border, width=1)

    # Badge
    bx = 18
    by = 70
    font_badge = ImageFont.truetype(FONT_BOLD, 11)
    bbox_b = font_badge.getbbox(badge)
    bw = (bbox_b[2] - bbox_b[0]) + 18
    bh = 22
    draw.rounded_rectangle([bx, by, bx + bw, by + bh], radius=6, fill=badge_bg, outline=badge_border, width=1)
    draw.text((bx + 9, by + 3), badge, fill=badge_text, font=font_badge)

    # Primary text
    font_primary = ImageFont.truetype(FONT_BOLD, 18)
    draw.text((bx + bw + 12, 69), primary, fill=primary_col, font=font_primary)

    # Secondary text
    font_sec = ImageFont.truetype(FONT_SEMI, 13)
    draw.text((18, 110), secondary, fill=secondary_col, font=font_sec)

    # Keycaps on right (draw from right margin)
    rx = W - 20
    rx = draw_keycap_right(draw, rx, 110, "Esc", "Close", dark)
    rx = draw_keycap_right(draw, rx, 110, "Ctrl+Enter", "Admin", dark)
    draw_keycap_right(draw, rx, 110, "Enter", "Run", dark)

    return img

def create_window_mockup(query, badge, primary, secondary, dark=True):
    card = render_launcher_card(query, badge, primary, secondary, dark)
    cw, ch = card.size

    pad = 32
    canvas_w = cw + pad * 2
    canvas_h = ch + pad * 2
    canvas = Image.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))

    # Shadow
    shadow = Image.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))
    s_draw = ImageDraw.Draw(shadow)
    s_draw.rounded_rectangle([pad, pad + 12, pad + cw, pad + ch + 12], radius=18, fill=(0, 0, 0, 130))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=20))

    canvas = Image.alpha_composite(canvas, shadow)
    canvas.paste(card, (pad, pad), card)
    return canvas

def generate_banner():
    W = 1280
    H = 460
    banner = Image.new("RGBA", (W, H), (11, 14, 20, 255))
    draw = ImageDraw.Draw(banner)

    # Background gradient
    for y in range(H):
        t = y / H
        r = int(11 + t * 9)
        g = int(14 + t * 10)
        b = int(20 + t * 16)
        draw.line([(0, y), (W, y)], fill=(r, g, b, 255), width=1)

    # Ambient radial glows
    glow1 = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    g1_draw = ImageDraw.Draw(glow1)
    g1_draw.ellipse([100 - 300, 100 - 300, 100 + 300, 100 + 300], fill=(0, 220, 255, 28))
    glow1 = glow1.filter(ImageFilter.GaussianBlur(radius=100))
    banner = Image.alpha_composite(banner, glow1)

    glow2 = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    g2_draw = ImageDraw.Draw(glow2)
    g2_draw.ellipse([1150 - 350, 280 - 350, 1150 + 350, 280 + 350], fill=(139, 92, 246, 25))
    glow2 = glow2.filter(ImageFilter.GaussianBlur(radius=120))
    banner = Image.alpha_composite(banner, glow2)

    # Grid
    grid = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gr_draw = ImageDraw.Draw(grid)
    for x in range(0, W, 40):
        gr_draw.line([(x, 0), (x, H)], fill=(255, 255, 255, 6), width=1)
    for y in range(0, H, 40):
        gr_draw.line([(0, y), (W, y)], fill=(255, 255, 255, 6), width=1)
    banner = Image.alpha_composite(banner, grid)

    # Icon on left
    ico = Image.open("src/resources/app.ico")
    ico_img = ico.resize((140, 140), Image.Resampling.LANCZOS)
    banner.paste(ico_img, (60, 65), ico_img)

    # Typography on left
    t_draw = ImageDraw.Draw(banner)
    font_title = ImageFont.truetype(FONT_BOLD, 44)
    font_sub = ImageFont.truetype(FONT_SEMI, 18)
    font_desc = ImageFont.truetype(FONT_REG, 14)
    font_badge = ImageFont.truetype(FONT_BOLD, 11)

    t_draw.text((225, 70), "SuperC", fill=(255, 255, 255), font=font_title)

    # Version pill
    t_draw.rounded_rectangle([395, 82, 455, 106], radius=6, fill=(12, 45, 72), outline=(2, 132, 199), width=1)
    t_draw.text((406, 86), "v1.5.0", fill=(56, 189, 248), font=font_badge)

    t_draw.text((225, 126), "Ultra-Fast Windows Power Utility", fill=(0, 225, 255), font=font_sub)
    t_draw.text((225, 154), "Single-key Win+R runner & Ctrl+Space quick launcher.", fill=(148, 163, 184), font=font_desc)

    # Metric pills with colored dot bullets
    badges = [
        ("< 2 ms Hotkey", (16, 185, 129)),
        ("~4 MB RAM", (14, 165, 233)),
        ("Pure Native C++", (168, 85, 247)),
        ("Zero Dependencies", (245, 158, 11)),
    ]
    bx = 60
    by = 230
    for text, col in badges:
        bbox = font_badge.getbbox(text)
        bw = (bbox[2] - bbox[0]) + 26
        t_draw.rounded_rectangle([bx, by, bx + bw, by + 26], radius=6, fill=(20, 25, 36), outline=(45, 55, 75), width=1)
        # Dot bullet
        t_draw.ellipse([bx + 8, by + 9, bx + 15, by + 16], fill=col)
        t_draw.text((bx + 20, by + 5), text, fill=(226, 232, 240), font=font_badge)
        bx += bw + 10

    # Shortcut prompt pills
    t_draw.text((60, 285), "DUAL EXPERIENCE:", fill=(100, 116, 139), font=ImageFont.truetype(FONT_BOLD, 11))

    t_draw.rounded_rectangle([60, 310, 280, 352], radius=8, fill=(24, 28, 40), outline=(55, 65, 90), width=1)
    t_draw.text((72, 322), "Ctrl + Space", fill=(255, 255, 255), font=ImageFont.truetype(FONT_BOLD, 13))
    t_draw.text((165, 323), "Quick Launcher", fill=(148, 163, 184), font=ImageFont.truetype(FONT_SEMI, 12))

    t_draw.rounded_rectangle([295, 310, 520, 352], radius=8, fill=(24, 28, 40), outline=(55, 65, 90), width=1)
    t_draw.text((307, 322), "Win + R  >  c", fill=(255, 255, 255), font=ImageFont.truetype(FONT_BOLD, 13))
    t_draw.text((400, 323), "CLI Runner", fill=(148, 163, 184), font=ImageFont.truetype(FONT_SEMI, 12))

    # Right side: Render launcher card mockup
    launcher = render_launcher_card("sqrt(144) + 2^8", "Math", "= 268", "Copy result to clipboard", dark=True)
    lw, lh = launcher.size
    target_w = 640
    target_h = int(lh * (target_w / lw))
    launcher_scaled = launcher.resize((target_w, target_h), Image.Resampling.LANCZOS)

    # Shadow
    shadow_card = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sc_draw = ImageDraw.Draw(shadow_card)
    sc_x = 580
    sc_y = 100
    sc_draw.rounded_rectangle([sc_x, sc_y + 14, sc_x + target_w, sc_y + target_h + 14], radius=18, fill=(0, 0, 0, 170))
    shadow_card = shadow_card.filter(ImageFilter.GaussianBlur(radius=28))
    banner = Image.alpha_composite(banner, shadow_card)

    banner.paste(launcher_scaled, (sc_x, sc_y), launcher_scaled)

    # Second collapsed search bar preview underneath
    bar_w = target_w
    bar_collapsed = Image.new("RGBA", (bar_w, 54), (0, 0, 0, 0))
    bc_draw = ImageDraw.Draw(bar_collapsed)
    bc_draw.rounded_rectangle([0, 0, bar_w - 1, 53], radius=14, fill=(28, 32, 44), outline=(55, 64, 85), width=1)
    bc_draw.text((20, 14), ">", fill=(0, 225, 255), font=ImageFont.truetype(FONT_BOLD, 20))
    bc_draw.text((46, 16), "100c in f   ->   212 f (Unit Conversion)", fill=(148, 163, 184), font=ImageFont.truetype(FONT_SEMI, 15))

    sc2_x = 580
    sc2_y = 275
    banner.paste(bar_collapsed, (sc2_x, sc2_y), bar_collapsed)

    # Border
    b_draw = ImageDraw.Draw(banner)
    b_draw.rectangle([0, 0, W - 1, H - 1], outline=(40, 48, 65), width=1)

    banner.save("assets/banner.png")
    print("Saved assets/banner.png")

def generate_screenshots():
    # 1. Dark Theme Preview
    m_dark = create_window_mockup("notepad", "App", "Notepad", "Launch application", dark=True)
    m_dark.save("assets/preview-dark.png")
    print("Saved assets/preview-dark.png")

    # 2. Light Theme Preview
    m_light = create_window_mockup("notepad", "App", "Notepad", "Launch application", dark=False)
    m_light.save("assets/preview-light.png")
    print("Saved assets/preview-light.png")

    # 3. Math Expression Preview
    m_math = create_window_mockup("sqrt(144) + 2^8", "Math", "= 268", "Copy result to clipboard", dark=True)
    m_math.save("assets/preview-math.png")
    print("Saved assets/preview-math.png")

    # 4. Unit Conversion Preview
    m_unit = create_window_mockup("100c in f", "Unit", "212 f", "Copy result to clipboard", dark=True)
    m_unit.save("assets/preview-unit.png")
    print("Saved assets/preview-unit.png")

    # 5. Right-click context menu preview
    W = 740
    H = 340
    menu_img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    base_card = render_launcher_card("calc", "App", "Calculator", "Launch application", dark=True)
    menu_img.paste(base_card, (30, 30), base_card)

    # Render Popup Context Menu at x=440, y=65
    mx = 430
    my = 65
    mw = 250
    mh = 230

    m_shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ms_draw = ImageDraw.Draw(m_shadow)
    ms_draw.rounded_rectangle([mx, my + 8, mx + mw, my + mh + 8], radius=8, fill=(0, 0, 0, 160))
    m_shadow = m_shadow.filter(ImageFilter.GaussianBlur(radius=16))
    menu_img = Image.alpha_composite(menu_img, m_shadow)

    m_surf = Image.new("RGBA", (mw, mh), (30, 34, 46, 255))
    md = ImageDraw.Draw(m_surf)
    md.rounded_rectangle([0, 0, mw - 1, mh - 1], radius=8, fill=(30, 34, 46), outline=(60, 68, 88), width=1)

    font_item = ImageFont.truetype(FONT_SEMI, 12)
    font_hot = ImageFont.truetype(FONT_REG, 11)

    items = [
        ("Switch to Light Theme", "", False),
        ("---", "", False),
        ("Run as Administrator", "Ctrl+Enter", False),
        ("Execute / Copy Result", "Enter", False),
        ("---", "", False),
        ("Start with Windows", "", True),
        ("SuperC Documentation", "GitHub", False),
        ("---", "", False),
        ("Exit SuperC", "", False)
    ]

    cur_y = 10
    for text, hotkey, is_checked in items:
        if text == "---":
            md.line([(10, cur_y + 4), (mw - 10, cur_y + 4)], fill=(50, 56, 74), width=1)
            cur_y += 9
        else:
            col = (255, 255, 255) if not text.startswith("Exit") else (248, 113, 113)
            tx = 16
            if is_checked:
                # Draw checkmark
                md.line([(tx, cur_y + 7), (tx + 3, cur_y + 11)], fill=(56, 189, 248), width=2)
                md.line([(tx + 3, cur_y + 11), (tx + 9, cur_y + 3)], fill=(56, 189, 248), width=2)
                tx += 14
            md.text((tx, cur_y), text, fill=col, font=font_item)
            if hotkey:
                bbox_h = font_hot.getbbox(hotkey)
                hw = bbox_h[2] - bbox_h[0]
                md.text((mw - 16 - hw, cur_y + 1), hotkey, fill=(148, 163, 184), font=font_hot)
            cur_y += 22

    menu_img.paste(m_surf, (mx, my), m_surf)
    menu_img.save("assets/preview-context-menu.png")
    print("Saved assets/preview-context-menu.png")

if __name__ == "__main__":
    generate_banner()
    generate_screenshots()
