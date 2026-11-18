"""v34 CIA banner (3DMicroFlight): the v29 artwork with the bottom caption updated.
python -P work/cia-v34/make_banner.py  (from the repository root)"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import re

here = Path(__file__).resolve().parent
img = Image.open(here.parent / 'cia-v29' / 'banner.png').convert('RGBA')   # the v29 base artwork
d = ImageDraw.Draw(img)
w, h = img.size
bg = img.getpixel((4, h - 4))
d.rectangle([0, 100, w, h], fill=bg)  # old caption strip
font = lambda size, bold=False: ImageFont.truetype('C:/Windows/Fonts/' + ('segoeuib.ttf' if bold else 'segoeui.ttf'), size)
# v34.03c: renamed CirrusFlight -> 3DMicroFlight (title, underline, subtitle)
d.rectangle([30, 8, 172, 44], fill=img.getpixel((4, 4)))
tw = d.textlength('3DMicroFlight', font=font(14))
d.text((35, 7), '3DMicroFlight', fill=(232, 235, 236, 255), font=font(14))
d.rectangle([36, 34, 36 + int(tw) - 2, 35], fill=(51, 178, 197, 255))
d.text((36, 37), '5 AIRCRAFT  /  FLIGHT SIM', fill=(40, 150, 170, 255), font=font(6, True))
# v35.23: scenery covers the contiguous US, not just the West
d.text((12, 101), '737 / A321 / F-16  +  ALL 48 STATES', fill=(235, 238, 240, 255), font=font(9, True))
d.text((12, 114), 'NEW NINTENDO 3DS XL', fill=(62, 221, 240, 255), font=font(7))
# v35.04: version from AUTOTEST_BUILD, as in the Makefile's HOME menu title
version = re.search(r'AUTOTEST_BUILD "([^"]+)"', (here.parent.parent / 'outputs' / 'CanyonFlight' / 'include' / 'autotest.h').read_text()).group(1)
label = version + '  |  DAY & NIGHT'
d.text((w - 12 - d.textlength(label, font=font(7)), 114), label, fill=(235, 238, 240, 255), font=font(7))
img.save(here / 'banner.png')
print(img.size)
