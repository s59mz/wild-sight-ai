import os
import json
import random
import subprocess
from tqdm import tqdm

# CONFIGURATION
REMOTE_NAME = "lila"
REMOTE_PATH = "public-datasets-lila/wcs-unzipped/animals"
DEST_DIR = "calibration_images"
NUM_IMAGES = 500

# Make sure destination directory exists
os.makedirs(DEST_DIR, exist_ok=True)

print("Gathering list of available image files...")

# List all files using rclone lsjson
cmd = ["rclone", "lsjson", "-R", f"{REMOTE_NAME}:{REMOTE_PATH}"]
result = subprocess.run(cmd, capture_output=True, text=True, check=True)
all_files = json.loads(result.stdout)

# Filter only .jpg files
jpg_files = [f for f in all_files if f['Path'].endswith(".jpg")]

print(f"Found {len(jpg_files)} JPG files in total.")

# Select random unique images
selected = random.sample(jpg_files, min(NUM_IMAGES, len(jpg_files)))

print(f"Downloading {len(selected)} images...")

# Download each selected image using rclone copyto
for f in tqdm(selected):
    remote_file = f"{REMOTE_PATH}/{f['Path']}"
    # Avoid overwriting by replacing slashes with underscores
    local_filename = f['Path'].replace("/", "_")
    local_path = os.path.join(DEST_DIR, local_filename)

    cmd = [
        "rclone", "copyto",
        f"{REMOTE_NAME}:{remote_file}", local_path,
        "--transfers=4", "--low-level-retries=2", "--retries=1"
    ]
    try:
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError:
        continue  # Skip any failures silently

print(f"Done! Images downloaded to: {DEST_DIR}")
