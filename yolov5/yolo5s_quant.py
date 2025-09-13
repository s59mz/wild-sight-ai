#!/usr/bin/env python3
# yolo5s_quant.py
# Quantize YOLOv5-style detector (raw tensor heads) with Vitis-AI torch quantizer.
# Usage (same as before):
#   python yolo5s_quant.py --quant_mode calib --data_dir calibration_images --model_dir ./models/md_v5a.0.1.pt --subset_len 12
#   python yolo5s_quant.py --quant_mode test  --data_dir calibration_images --model_dir ./models/md_v5a.0.1.pt --subset_len 1 --batch_size=1 --deploy

import os
import sys
import argparse
from pathlib import Path
import random
from typing import Tuple, List

import torch
from torch.utils.data import Dataset, DataLoader
from torchvision import transforms
from PIL import Image, ImageOps

from pytorch_nndct.apis import torch_quantizer

# ===== YOLOv5 imports (repo must be next to this script as ../yolov5) =====
yolov5_root = Path('../yolov5')
sys.path.insert(0, str(yolov5_root.resolve()))
from models.common import DetectMultiBackend

# -------------------------
# Letterbox (PIL-only)
# -------------------------
def letterbox_pil(im: Image.Image, new_shape: int = 640, color=(114, 114, 114)) -> Image.Image:
    """Resize image to new_shape with aspect ratio kept and padding added (like YOLOv5)."""
    w0, h0 = im.size
    r = min(new_shape / w0, new_shape / h0)
    new_unpad = (int(round(w0 * r)), int(round(h0 * r)))  # (nw, nh)
    im_resized = im.resize(new_unpad, Image.BILINEAR)
    dw = new_shape - new_unpad[0]
    dh = new_shape - new_unpad[1]
    left = dw // 2
    top = dh // 2
    right = dw - left
    bottom = dh - top
    out = ImageOps.expand(im_resized, border=(left, top, right, bottom), fill=color)
    return out  # (new_shape, new_shape)

# -------------------------
# Calibration dataset
# -------------------------
class CalibImages(Dataset):
    def __init__(self, root: str, size: int = 640, subset_len: int = None):
        p = Path(root)
        exts = ('*.jpg', '*.jpeg', '*.png', '*.bmp', '*.tif', '*.tiff')
        paths: List[Path] = []
        for e in exts:
            paths += list(p.rglob(e))
        paths.sort()
        if subset_len is not None:
            subset_len = min(subset_len, len(paths))
            # deterministic subset (no aug) — OK for calibration
            paths = paths[:subset_len]
        self.paths = paths
        self.size = size
        self.to_tensor = transforms.ToTensor()  # 0..1, NO mean/std

    def __len__(self) -> int:
        return len(self.paths)

    def __getitem__(self, i: int) -> Tuple[torch.Tensor, int]:
        # label is unused; return 0
        im = Image.open(self.paths[i]).convert('RGB')
        im = letterbox_pil(im, self.size)
        return self.to_tensor(im), 0

def make_loader(root: str, batch_size: int, subset_len: int = None) -> DataLoader:
    ds = CalibImages(root, 640, subset_len)
    return DataLoader(ds, batch_size=batch_size, shuffle=False, num_workers=2, pin_memory=True, drop_last=False)

# -------------------------
# CLI
# -------------------------
parser = argparse.ArgumentParser()
parser.add_argument('--data_dir', default="calibration_images",
                    help='Directory of calibration/eval images (unlabeled).')
parser.add_argument('--model_dir', default="./models/md_v5a.0.1.pt",
                    help='Path to trained .pt weights file (YOLOv5).')
parser.add_argument('--config_file', default=None, help='Quantization config YAML (optional).')
parser.add_argument('--subset_len', default=None, type=int,
                    help='Limit number of images processed (recommended: 100–300 for calib).')
parser.add_argument('--batch_size', default=8, type=int, help='Batch size.')
parser.add_argument('--quant_mode', default='calib', choices=['float', 'calib', 'test'],
                    help='float: no quant; calib: collect stats; test: finalize/export.')
parser.add_argument('--fast_finetune', dest='fast_finetune', action='store_true',
                    help='(Not used here) fast finetune before calibration.')
parser.add_argument('--deploy', dest='deploy', action='store_true',
                    help='Export TorchScript/ONNX/XModel in test mode.')
parser.add_argument('--inspect', dest='inspect', action='store_true', help='Run inspector (optional).')
parser.add_argument('--target', dest='target', nargs="?", const="", help='Target DPU name (optional).')
args, _ = parser.parse_known_args()

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

# -------------------------
# Load YOLOv5 model
# -------------------------
def load_model(weights_path: str, device: torch.device):
    # DetectMultiBackend handles various formats; returns an object with .model
    backend = DetectMultiBackend(weights_path, device=device, dnn=False, data=None, fp16=False)
    model = backend.model
    model.eval()  # IMPORTANT for stable graph & proper BN folding
    return model

# -------------------------
# (Optional) inspector
# -------------------------
def run_inspector(model: torch.nn.Module, target: str):
    if not target:
        raise RuntimeError("Inspector requires --target (e.g., DPUCZDX8G).")
    from pytorch_nndct.apis import Inspector
    inspector = Inspector(target)
    dummy = torch.randn(1, 3, 640, 640)
    inspector.inspect(model.to('cpu'), (dummy,), device='cpu')

# -------------------------
# Main quant/eval/export
# -------------------------
def main():
    # Load model
    model = load_model(args.model_dir, device)

    # Dummy input for graph building
    dummy = torch.randn(args.batch_size, 3, 640, 640)

    # Float-only path (rarely used here)
    if args.quant_mode == 'float':
        if args.inspect:
            run_inspector(model, args.target)
            print("Inspector finished.")
            return
        # Drive a pass (not strictly needed)
        model.to(device)
        with torch.no_grad():
            _ = model(dummy.to(device))
        print("loss: 0")
        print("top-1 / top-5 accuracy: 0 / 0")
        return

    # Build quantizer (calib or test)
    quantizer = torch_quantizer(
        args.quant_mode,
        model,
        (dummy,),                   # shape definition only; real images come next
        device=device,
        quant_config_file=args.config_file,
        target=args.target
    )
    quant_model = quantizer.quant_model
    quant_model.eval()

    # Prepare data
    # NOTE: Use representative, augmentation-free, letterboxed images.
    loader = make_loader(args.data_dir, args.batch_size, args.subset_len)

    # Optional: fast finetune is not recommended for detection here; skip
    # if args.fast_finetune and args.quant_mode == 'calib':
    #     quantizer.fast_finetune(lambda qm, ldr, _: _drive(qm, ldr), (quant_model, loader, None))

    # Drive images through the quantized model
    quant_model.to(device)
    with torch.no_grad():
        for images, _ in loader:
            images = images.to(device)
            _ = quant_model(images)  # collect stats (calib) or finalize (test)

    # Print stub metrics (we’re not computing mAP here)
    print('loss: 0')
    print('top-1 / top-5 accuracy: 0 / 0')

    # Save intermediate config after calib (required before test)
    if args.quant_mode == 'calib':
        quantizer.export_quant_config()

    # Final export in test mode
    if args.deploy:
        # Vitis-AI export requires bs=1 and 1 iteration — enforce if needed
        if not (args.batch_size == 1 and (args.subset_len == 1 or args.subset_len is None)):
            print("Warning: --deploy expects --batch_size=1 and --subset_len=1. Proceeding anyway.")
        quantizer.export_torch_script()
        quantizer.export_onnx_model()
        quantizer.export_xmodel()

if __name__ == '__main__':
    print("-------- Start quantization/evaluation --------")
    main()
    print("-------- End of test --------")
