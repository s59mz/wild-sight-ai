import os
import random
import shutil

SOURCE_DIR = "dataset_images"
DEST_DIR = "selected_images"
NUM_IMAGES = 100  # number of random images to copy

# Create destination directory if it doesn't exist
os.makedirs(DEST_DIR, exist_ok=True)

# Get list of image files
image_extensions = [".jpg", ".jpeg", ".png", ".bmp", ".tiff"]
image_files = [f for f in os.listdir(SOURCE_DIR) if os.path.splitext(f)[1].lower() in image_extensions]

# Check we have enough images
if NUM_IMAGES > len(image_files):
    raise ValueError(f"Requested {NUM_IMAGES} images, but only found {len(image_files)} in {SOURCE_DIR}")

# Randomly select images
selected_files = random.sample(image_files, NUM_IMAGES)

# Copy selected files
for file in selected_files:
    src = os.path.join(SOURCE_DIR, file)
    dst = os.path.join(DEST_DIR, file)
    shutil.copy2(src, dst)

print(f"Copied {NUM_IMAGES} images to '{DEST_DIR}'")
