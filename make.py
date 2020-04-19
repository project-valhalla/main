import os
os.system("make -j4 -C src install")
os.system("tesseract_unix")