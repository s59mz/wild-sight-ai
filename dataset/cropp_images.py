import os
import json
import random
from PIL import Image
from tqdm import tqdm
from collections import defaultdict

# to get the annotation file do:
# wget https://storage.googleapis.com/public-datasets-lila/wcs/wcs_20220205_bboxes_with_classes.zip
# and unzip the file

# Configuration
ANNOTATION_FILE = "wcs_20220205_bboxes_with_classes.json"
INPUT_DIR = "calibration_images"
OUTPUT_DIR = "calibration_cropps"

# uncommend the line resized.save() to take effect
CROP_SIZE = (448, 256)

# Create output directory if it doesn't exist
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Load JSON annotation file
with open(ANNOTATION_FILE, "r") as f:
    data = json.load(f)

# Map file_name → image_id
file_name_to_id = {img["file_name"]: img["id"] for img in data["images"]}

# Map image_id → list of bboxes
image_id_to_bboxes = defaultdict(list)
for ann in data["annotations"]:
    if "bbox" in ann:
        image_id_to_bboxes[ann["image_id"]].append(ann["bbox"])

# Process all images in the input folder
input_files = [f for f in os.listdir(INPUT_DIR) if f.lower().endswith(".jpg")]

for image_file in tqdm(input_files, desc="Cropping and resizing"):
    # Build relative path in JSON format (animals/XXXX/filename.jpg)
    splitn = image_file.split("_")
    folder = splitn[0]
    filen = splitn[1]
    relative_path = f"animals/{folder}/{filen}"

    if relative_path not in file_name_to_id:
        continue

    image_id = file_name_to_id[relative_path]
    bboxes = image_id_to_bboxes.get(image_id, [])

    if not bboxes:
        continue

    full_path = os.path.join(INPUT_DIR, image_file)
    try:
        with Image.open(full_path) as img:
            for i, bbox in enumerate(bboxes):
                x, y, w, h = map(int, bbox)
                cropped = img.crop((x, y, x + w, y + h))
                resized = cropped.resize(CROP_SIZE)
                output_name = f"{os.path.splitext(image_file)[0]}_{i}.jpg"
                output_path = os.path.join(OUTPUT_DIR, output_name)
                cropped.save(output_path)
                # resized.save(output_path)
    except Exception as e:
        print(f"Error processing {image_file}: {e}")
