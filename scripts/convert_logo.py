from PIL import Image
import os

os.makedirs("bin", exist_ok=True)

img = Image.open("scripts/logo_asset/logo.png").convert("RGB")
img = img.resize((300, 100))

img.save("bin/splash.bmp", "BMP")
print(f"Successfully generated bin/splash.bmp ({img.size[0]}x{img.size[1]})!")