from PIL import Image
import os

os.makedirs("bin", exist_ok=True)

img = Image.open("scripts/logo_asset/logo.png").convert("RGB")

target_width = 350
w_percent = target_width / float(img.size[0])
target_height = int(float(img.size[1]) * w_percent)

img = img.resize((target_width, target_height), Image.Resampling.LANCZOS)
img.save("bin/splash.bmp", "BMP")
print(f"Successfully generated bin/splash.bmp ({target_width}x{target_height} proportional)!")