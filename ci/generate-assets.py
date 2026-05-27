#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
import os

ASSETS_DIR = os.path.join(os.path.dirname(__file__), '..', 'assets')
os.makedirs(ASSETS_DIR, exist_ok=True)

ALIBABA_ORANGE = (255, 106, 0)
DARK_BG = (30, 30, 35)
DARK_BG_LIGHTER = (45, 45, 52)
WHITE = (255, 255, 255)
LIGHT_GRAY = (200, 200, 210)

def draw_cloud(draw, cx, cy, scale, color):
    r = int(40 * scale)
    draw.ellipse([cx - r*2, cy - r*0.8, cx + r*0.5, cy + r*0.8], fill=color)
    draw.ellipse([cx - r*1.2, cy - r*1.4, cx + r*0.8, cy + r*0.2], fill=color)
    draw.ellipse([cx - r*0.3, cy - r*1.6, cx + r*1.5, cy + r*0.1], fill=color)
    draw.ellipse([cx + r*0.5, cy - r*1.2, cx + r*2.0, cy + r*0.5], fill=color)
    draw.rectangle([cx - r*1.8, cy - r*0.2, cx + r*1.8, cy + r*0.8], fill=color)

def draw_dollar_sign(draw, cx, cy, size, color):
    font_size = int(size)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", font_size)
    except:
        font = ImageFont.load_default()
    bbox = draw.textbbox((0, 0), "$", font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    draw.text((cx - tw//2, cy - th//2 - bbox[1]), "$", fill=color, font=font)

def generate_icon(size):
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    margin = int(size * 0.08)
    radius = int(size * 0.18)
    draw.rounded_rectangle(
        [margin, margin, size - margin, size - margin],
        radius=radius,
        fill=DARK_BG
    )

    border_width = max(2, int(size * 0.015))
    draw.rounded_rectangle(
        [margin, margin, size - margin, size - margin],
        radius=radius,
        outline=ALIBABA_ORANGE,
        width=border_width
    )

    cloud_scale = size / 512.0
    cloud_cx = size * 0.48
    cloud_cy = size * 0.42
    draw_cloud(draw, cloud_cx, cloud_cy, cloud_scale, ALIBABA_ORANGE)

    dollar_size = int(size * 0.35)
    dollar_cx = size * 0.48
    dollar_cy = size * 0.42
    draw_dollar_sign(draw, dollar_cx, dollar_cy, dollar_size, DARK_BG)

    bar_y = int(size * 0.72)
    bar_h = int(size * 0.06)
    bar_margin = int(size * 0.15)
    bars = [0.3, 0.55, 0.8, 0.65, 0.45]
    bar_w = int((size - 2 * bar_margin) / (len(bars) * 2 - 1))
    for i, h in enumerate(bars):
        bx = bar_margin + i * bar_w * 2
        bh = int(size * 0.12 * h)
        color = ALIBABA_ORANGE if i % 2 == 0 else LIGHT_GRAY
        draw.rounded_rectangle(
            [bx, bar_y + bar_h - bh, bx + bar_w, bar_y + bar_h],
            radius=max(1, bar_w // 4),
            fill=color
        )

    return img

def generate_dmg_background():
    width = 660
    height = 400
    img = Image.new('RGB', (width, height), DARK_BG)
    draw = ImageDraw.Draw(img)

    for i in range(0, width, 40):
        draw.line([(i, 0), (i, height)], fill=DARK_BG_LIGHTER, width=1)
    for i in range(0, height, 40):
        draw.line([(0, i), (width, i)], fill=DARK_BG_LIGHTER, width=1)

    try:
        font_large = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 28)
        font_small = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16)
    except:
        font_large = ImageFont.load_default()
        font_small = ImageFont.load_default()

    title = "Alibaba Cloud Spend"
    bbox = draw.textbbox((0, 0), title, font=font_large)
    tw = bbox[2] - bbox[0]
    draw.text(((width - tw) // 2, 30), title, fill=WHITE, font=font_large)

    subtitle = "Drag to Applications to install"
    bbox2 = draw.textbbox((0, 0), subtitle, font=font_small)
    tw2 = bbox2[2] - bbox2[0]
    draw.text(((width - tw2) // 2, 70), subtitle, fill=LIGHT_GRAY, font=font_small)

    app_box_x = 120
    app_box_y = 140
    box_size = 140
    draw.rounded_rectangle(
        [app_box_x, app_box_y, app_box_x + box_size, app_box_y + box_size],
        radius=20,
        outline=ALIBABA_ORANGE,
        width=3
    )
    icon_small = generate_icon(100)
    icon_small = icon_small.convert('RGBA')
    img.paste(icon_small, (app_box_x + 20, app_box_y + 20), icon_small)

    arrow_cx = width // 2
    arrow_cy = app_box_y + box_size // 2
    arrow_len = 80
    arrow_head = 20
    draw.line(
        [(arrow_cx - arrow_len//2, arrow_cy), (arrow_cx + arrow_len//2, arrow_cy)],
        fill=ALIBABA_ORANGE, width=4
    )
    draw.polygon([
        (arrow_cx + arrow_len//2, arrow_cy),
        (arrow_cx + arrow_len//2 - arrow_head, arrow_cy - arrow_head//2),
        (arrow_cx + arrow_len//2 - arrow_head, arrow_cy + arrow_head//2),
    ], fill=ALIBABA_ORANGE)

    apps_box_x = 400
    apps_box_y = 140
    draw.rounded_rectangle(
        [apps_box_x, apps_box_y, apps_box_x + box_size, apps_box_y + box_size],
        radius=20,
        outline=LIGHT_GRAY,
        width=3
    )
    try:
        font_apps = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 18)
    except:
        font_apps = ImageFont.load_default()
    apps_text = "Applications"
    bbox3 = draw.textbbox((0, 0), apps_text, font=font_apps)
    tw3 = bbox3[2] - bbox3[0]
    th3 = bbox3[3] - bbox3[1]
    draw.text(
        (apps_box_x + (box_size - tw3) // 2, apps_box_y + (box_size - th3) // 2),
        apps_text, fill=LIGHT_GRAY, font=font_apps
    )

    footer = "github.com/sheece/alibaba-cloud-spend"
    try:
        font_footer = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 12)
    except:
        font_footer = ImageFont.load_default()
    bbox4 = draw.textbbox((0, 0), footer, font=font_footer)
    tw4 = bbox4[2] - bbox4[0]
    draw.text(((width - tw4) // 2, height - 30), footer, fill=(100, 100, 110), font=font_footer)

    return img

print("Generating icon sizes...")
for size in [16, 32, 48, 64, 128, 256, 512, 1024]:
    icon = generate_icon(size)
    icon.save(os.path.join(ASSETS_DIR, f'icon-{size}.png'))
    print(f"  icon-{size}.png")

icon_512 = generate_icon(512)
icon_512.save(os.path.join(ASSETS_DIR, 'icon-512.png'))
icon_1024 = generate_icon(1024)
icon_1024.save(os.path.join(ASSETS_DIR, 'icon-1024.png'))

print("Generating DMG background...")
dmg_bg = generate_dmg_background()
dmg_bg.save(os.path.join(ASSETS_DIR, 'dmg-background.png'))
print("  dmg-background.png")

print("Generating Windows installer banner...")
banner = Image.new('RGB', (500, 60), DARK_BG)
banner_draw = ImageDraw.Draw(banner)
try:
    font_banner = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
except:
    font_banner = ImageFont.load_default()
banner_title = "Alibaba Cloud Spend"
bbox = banner_draw.textbbox((0, 0), banner_title, font=font_banner)
tw = bbox[2] - bbox[0]
th = bbox[3] - bbox[1]
banner_draw.text(((500 - tw) // 2, (60 - th) // 2), banner_title, fill=ALIBABA_ORANGE, font=font_banner)
banner.save(os.path.join(ASSETS_DIR, 'installer-banner.bmp'))
print("  installer-banner.bmp")

print("All assets generated successfully!")
