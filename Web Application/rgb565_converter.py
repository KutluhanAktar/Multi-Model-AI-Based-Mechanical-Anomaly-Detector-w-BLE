import argparse
from glob import glob
import numpy as np
from PIL import Image, ImageDraw

# Obtain the passed bounding box measurements (query parameters) as Python Arguments.
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--x", required=True, help="bounding box [X]")
    parser.add_argument("--y", required=True, help="bounding box [Y]")
    parser.add_argument("--w", required=True, help="bounding box [Width]")
    parser.add_argument("--h", required=True, help="bounding box [Height]")
    args = parser.parse_args()
    x = int(args.x)
    y = int(args.y)
    w = int(args.w)
    h = int(args.h)

   # Obtain all RGB565 buffer arrays transferred by FireBeetle 2 ESP32-S3 as text (.txt) files.
    path = "C:\\Users\\kutlu\\New E\\xampp\\htdocs\\mechanical_anomaly_detector\\detections"
    images = glob(path + "/*.txt")

    # Convert each RGB565 buffer (TXT file) to a JPG image file and save the generated image files to the images folder.
    for img in images:
        loc = path + "/images/" + img.split("\\")[8].split(".")[0] + ".jpg"
        size = (240,240)
        # RGB565 (uint16_t) to RGB (3x8-bit pixels, true color)
        raw = np.fromfile(img).byteswap(True)
        file = Image.frombytes('RGB', size, raw, 'raw', 'BGR;16', 0, 1)
        # Modify the converted RGB buffer (image) to draw the received bounding box on the resulting image.
        offset = 50
        m_file = ImageDraw.Draw(file)
        m_file.rectangle([(x, y), (x+w+offset, y+h+offset)], outline=(225,255,255), width=3)
        file.save(loc)
        #print("Converted: " + loc)