# Dataset Preparation Tools for Wild-Sight-AI

This folder contains helper scripts for preparing datasets used in **quantization calibration** of the MegaDetector (YOLOv5-based) model.  

The goal is to create a directory of representative images (`calibration_images/`) that can be used during model quantization to produce the `.xmodel` file. Optionally, you can also crop images around annotated animals to create a dataset for calibrating an additional classifier.

---

## Prerequisites

- Python 3.8+  
- [rclone](https://rclone.org/) (for downloading images from WCS dataset)  
- `pip install pillow tqdm`  

---

## Step 1: Download random images from the WCS dataset

The script `get_random_images_from_wcs.py` downloads a random subset of wildlife images from the **WCS Dataset** hosted in the [LILA public dataset repository](https://lila.science/datasets).  

```bash
# edit NUM_IMAGES in the script if you want more/less images
python3 get_random_images_from_wcs.py
```

This will create a directory:

```
calibration_images/
    000123_abc.jpg
    000456_def.jpg
    ...
```

By default, it downloads **500 random JPEGs**.

---

## Step 2 (optional): Crop animals from images

To generate calibration images focused only on animals (useful for classifier training or cropped calibration), use the `crop_images.py` script.  

### 2.1 Download the annotation file
The WCS dataset provides bounding box annotations. Fetch them with:

```bash
wget https://storage.googleapis.com/public-datasets-lila/wcs/wcs_20220205_bboxes_with_classes.zip
unzip wcs_20220205_bboxes_with_classes.zip
```

This gives you `wcs_20220205_bboxes_with_classes.json`.

### 2.2 Run the cropping script
```bash
python3 crop_images.py
```

This will:
- Load the annotation JSON.  
- Look up bounding boxes for each downloaded calibration image.  
- Crop out each animal region, resize to `448x256`, and save into:

```
calibration_cropps/
    000123_abc_0.jpg
    000123_abc_1.jpg
    ...
```

These cropped images can optionally be used for fine-tuning or calibration of a **secondary classifier**.

---

## Step 3 (optional): Select a smaller subset

If you already have a large pool of images and want to randomly select a smaller number (e.g., 100 or 300) into a new folder, use the `select_random_images.py` script.  

Edit `NUM_IMAGES` inside the script, then run:

```bash
python3 select_random_images.py
```

This copies the requested number of images into:

```
selected_images/
```

---

## Typical workflow

1. Download ~500 images from WCS:  
   ```bash
   python3 get_random_images_from_wcs.py
   ```
   → Produces `calibration_images/`

2. (Optional) Crop animals using bounding boxes:  
   ```bash
   wget https://storage.googleapis.com/public-datasets-lila/wcs/wcs_20220205_bboxes_with_classes.zip
   unzip wcs_20220205_bboxes_with_classes.zip
   python3 crop_images.py
   ```
   → Produces `calibration_cropps/`

3. (Optional) Reduce dataset size:  
   ```bash
   python3 select_random_images.py
   ```
   → Produces `selected_images/`

The directory `calibration_images/` (or one of the subsets you created) can then be used directly in the quantization script (`yolo5s_quant.py`) to produce the `.xmodel`.

---

✅ With these scripts, you can quickly prepare **representative calibration datasets** for quantization or additional classifier training in the Wild-Sight-AI pipeline.  
