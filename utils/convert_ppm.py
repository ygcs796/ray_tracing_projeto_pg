from PIL import Image
import random
import time

nm = str(int(time.time()))

im = Image.open("out.ppm")
im.save(f"./renders/{nm}.jpg")
