#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""video_gen.py -- 图像序列 / GIF / 视频 转换成 OLED 逐帧动画 C 代码 (RLE 压缩)

用法:
  python video_gen.py --demo                          生成内置示例动画
  python video_gen.py --gif anim.gif                  转换 GIF
  python video_gen.py --png frames/                   转换图片序列目录
  python video_gen.py --mp4 input.mp4                 转换视频(需要 ffmpeg)

风格选项 (单色屏上观感差别很大, 建议都试一遍):
  --style edge     边缘描线, 适合照片/卡通/彩色素材   ★推荐
  --style plain    直接二值化, 适合已经是黑底白线的素材
  --style dither   抖动保留灰度, 会有点噪点

其它常用参数:
  --out FILE       输出的 C 文件, 默认 Video_Data.c
  --fps N          播放帧率, 默认 10
  --threshold N    二值化阈值 (plain/dither 用), 默认 128
  --invert         反相 (白底素材需要加)
  --edge-thr N     边缘阈值 (edge 用), 默认 55
  --edge-gain N    边缘增益 (edge 用), 默认 2.2
  --blur N         高斯模糊半径 (edge 用), 默认 1.2
  --dilate         边缘加粗 1 像素
  --preview FILE   导出帧预览拼图 PNG
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageOps, ImageFilter, ImageSequence
except ImportError:
    sys.exit("缺少 Pillow, 请先执行: pip install pillow")

W, H = 128, 64
FRAME_BYTES = W * H // 8   # 1024


# ============ 图像 -> OLED 显存格式 (纵向8点一页, 低位在上) ============

def pack_frame(img, threshold=128):
    """把 128x64 灰度/二值图打包成 1024 字节的 OLED 显存格式"""
    if img.mode != "L":
        img = img.convert("L")
    if img.size != (W, H):
        img = img.resize((W, H), Image.LANCZOS)
    px = img.load()
    buf = bytearray(FRAME_BYTES)
    for page in range(H // 8):
        base = page * W
        for x in range(W):
            b = 0
            for bit in range(8):
                if px[x, page * 8 + bit] > threshold:
                    b |= 1 << bit
            buf[base + x] = b
    return bytes(buf)


# ============ RLE 编码 ============
# 每段 2 字节头:
#   count != 0  -> 重复段, 后跟 1 字节值, 重复 count 次 (count 1..255)
#   count == 0  -> 字面量段, 后跟 1 字节长度 len, 再跟 len 个原始字节
# 最坏情况膨胀不到 1.02 倍

def rle_encode(data):
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        v = data[i]
        run = 1
        while i + run < n and data[i + run] == v and run < 255:
            run += 1
        if run >= 3:
            out.append(run)
            out.append(v)
            i += run
            continue
        lit = bytearray()
        while i < n and len(lit) < 255:
            vv = data[i]
            r = 1
            while i + r < n and data[i + r] == vv and r < 255:
                r += 1
            if r >= 3:
                break
            lit.append(vv)
            i += 1
        out.append(0)
        out.append(len(lit))
        out.extend(lit)
    return bytes(out)


# ============ 画面风格处理 ============

STYLE_PLAIN = "plain"
STYLE_EDGE = "edge"
STYLE_DITHER = "dither"


def _content_bbox(images, dark_thr=200, skip=3):
    """求整段素材的统一内容包围盒, 保证所有帧裁剪一致不抖动"""
    try:
        import numpy as np
    except ImportError:
        return None
    x0 = y0 = 10 ** 9
    x1 = y1 = -1
    for im in images:
        a = np.array(im)
        h, w = a.shape
        if h <= 2 * skip or w <= 2 * skip:
            continue
        sub = a[skip:h - skip, skip:w - skip]
        mask = sub < dark_thr
        if not mask.any():
            continue
        ys, xs = np.nonzero(mask)
        x0 = min(x0, int(xs.min()) + skip)
        y0 = min(y0, int(ys.min()) + skip)
        x1 = max(x1, int(xs.max()) + skip + 1)
        y1 = max(y1, int(ys.max()) + skip + 1)
    if x1 < 0:
        return None
    return (x0, y0, x1, y1)


def _fit(img, bbox, pad=1):
    """按统一包围盒裁剪后等比缩放到 128x64, 白底居中"""
    if bbox:
        x0, y0, x1, y1 = bbox
        img = img.crop((max(0, x0 - pad), max(0, y0 - pad),
                        min(img.width, x1 + pad), min(img.height, y1 + pad)))
    w, h = img.size
    s = min(W / float(w), H / float(h))
    nw, nh = max(1, int(round(w * s))), max(1, int(round(h * s)))
    t = img.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("L", (W, H), 255)
    canvas.paste(t, ((W - nw) // 2, (H - nh) // 2))
    return canvas


def apply_style(images, style=STYLE_PLAIN, threshold=128, invert=False,
                blur=1.2, edge_threshold=55, gain=2.2, dilate=0):
    """把任意灰度的帧序列处理成 128x64 的位图序列 (亮点=像素点亮)"""
    bbox = None if style == STYLE_PLAIN else _content_bbox(images)

    if style == STYLE_EDGE:
        try:
            import numpy as np
        except ImportError:
            sys.exit("edge 风格需要 numpy, 请先执行: pip install numpy")

    out = []
    for im in images:
        g = _fit(im, bbox)

        if style == STYLE_EDGE:
            e = g.filter(ImageFilter.GaussianBlur(blur)).filter(ImageFilter.FIND_EDGES)
            arr = np.clip(np.array(e).astype(np.int16) * gain, 0, 255).astype(np.uint8)
            bw = Image.fromarray((arr > edge_threshold).astype(np.uint8) * 255, "L")
            if dilate:
                bw = bw.filter(ImageFilter.MaxFilter(3))
            out.append(bw)

        elif style == STYLE_DITHER:
            g2 = ImageOps.invert(g) if invert else g
            out.append(g2.convert("1"))

        else:
            g2 = ImageOps.invert(g) if invert else g
            out.append(g2.point(lambda v: 255 if v > threshold else 0))

    return out


# ============ 内置示例动画: 狗脸 眨眼 -> 左右看 -> 微笑 ============

def demo_frames():
    frames = []
    total = 24
    cx1, cx2, cy = 40, 88, 26

    for f in range(total):
        img = Image.new("L", (W, H), 0)
        d = ImageDraw.Draw(img)

        if f < 6:
            ratio = 0.15 + 0.85 * (f / 5.0)
            style = "open"
            pdx, pdy = 0, 0
        elif f < 16:
            ratio = 1.0
            style = "open"
            g = (f - 6) / 9.0
            pdx = int(-8 + 16 * g)
            pdy = 0
        else:
            ratio = 1.0
            style = "happy"
            pdx, pdy = 0, 0

        if style == "happy":
            for cx in (cx1, cx2):
                d.arc([cx - 15, cy - 4, cx + 15, cy + 20], start=200, end=340, fill=255, width=4)
            d.arc([44, 38, 84, 58], start=20, end=160, fill=255, width=3)
        else:
            ry = max(1, int(14 * ratio))
            for cx in (cx1, cx2):
                d.ellipse([cx - 15, cy - ry, cx + 15, cy + ry], outline=255, width=3)
                if ry >= 7:
                    pxx = cx + pdx
                    pyy = cy + pdy
                    d.ellipse([pxx - 6, pyy - 6, pxx + 6, pyy + 6], fill=255)

        frames.append(img)
    return frames


# ============ 读入各种来源 ============

def flatten(im):
    """把带透明通道的帧铺到白底上, 返回灰度图"""
    if im.mode in ("RGBA", "LA") or (im.mode == "P" and "transparency" in im.info):
        rgba = im.convert("RGBA")
        bg = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
        bg.alpha_composite(rgba)
        return bg.convert("L")
    return im.convert("L")


def frames_from_png(folder):
    names = sorted(n for n in os.listdir(folder)
                   if n.lower().endswith((".png", ".bmp", ".jpg", ".jpeg")))
    if not names:
        sys.exit("目录里没有图片: " + folder)
    return [flatten(Image.open(os.path.join(folder, n))) for n in names]


def frames_from_gif(path):
    im = Image.open(path)
    out = [flatten(fr) for fr in ImageSequence.Iterator(im)]
    im.close()
    return out


def frames_from_mp4(path, fps):
    import subprocess
    import tempfile
    tmpdir = tempfile.mkdtemp(prefix="oled_frames_")
    cmd = ["ffmpeg", "-y", "-i", path,
           "-vf", "fps={},scale={}:{}".format(fps, W, H),
           os.path.join(tmpdir, "f_%05d.png")]
    try:
        subprocess.run(cmd, check=True)
    except FileNotFoundError:
        sys.exit("找不到 ffmpeg, 请先安装并加入 PATH")
    return frames_from_png(tmpdir)


# ============ 生成 C 代码 ============

def emit_c(images, fps, out_c, title):
    offsets = []
    blob = bytearray()
    for img in images:
        offsets.append(len(blob))
        blob.extend(rle_encode(pack_frame(img, threshold=127)))

    out_h = os.path.splitext(out_c)[0] + ".h"
    guard = "__VIDEO_DATA_H"

    with open(out_c, "w", encoding="utf-8", newline="") as fp:
        print('/* 由 video_gen.py 自动生成, 请勿手工修改 */', file=fp)
        print('/* 源素材: {} */'.format(title), file=fp)
        print('/* 帧数: {}  原始: {} 字节  压缩后: {} 字节  '
              '平均 {:.0f} 字节/帧 */'.format(
                  len(images), len(images) * FRAME_BYTES, len(blob),
                  len(blob) / float(len(images))), file=fp)
        print('', file=fp)
        print('#include "Video_Data.h"', file=fp)
        print('', file=fp)
        print('const uint16_t Video_FrameCount = {};'.format(len(images)), file=fp)
        print('', file=fp)
        print('const uint32_t Video_Offset[] = {', file=fp)
        for i in range(0, len(offsets), 10):
            print('    ' + ', '.join(str(v) for v in offsets[i:i + 10]) + ',', file=fp)
        print('};', file=fp)
        print('', file=fp)
        print('const uint8_t Video_Data[] = {', file=fp)
        for i in range(0, len(blob), 16):
            print('    ' + ','.join('0x{:02X}'.format(b) for b in blob[i:i + 16]) + ',', file=fp)
        print('};', file=fp)

    with open(out_h, "w", encoding="utf-8", newline="") as fp:
        print('#ifndef {}'.format(guard), file=fp)
        print('#define {}'.format(guard), file=fp)
        print('', file=fp)
        print('#include <stdint.h>', file=fp)
        print('', file=fp)
        print('/* 建议播放帧率, 直接传给 Video_Init() */', file=fp)
        print('#define VIDEO_FPS   {}'.format(fps), file=fp)
        print('', file=fp)
        print('extern const uint16_t Video_FrameCount;', file=fp)
        print('extern const uint32_t Video_Offset[];', file=fp)
        print('extern const uint8_t  Video_Data[];', file=fp)
        print('', file=fp)
        print('#endif', file=fp)

    return len(blob), out_h


def emit_preview(images, path, cols=8, scale=2):
    rows = (len(images) + cols - 1) // cols
    cw, ch = W * scale + 4, H * scale + 4
    sheet = Image.new("L", (cw * cols, ch * rows), 40)
    for i, im in enumerate(images):
        big = im.convert("L").resize((W * scale, H * scale), Image.NEAREST)
        sheet.paste(big, ((i % cols) * cw + 2, (i // cols) * ch + 2))
    d = os.path.dirname(os.path.abspath(path))
    if d:
        os.makedirs(d, exist_ok=True)
    sheet.save(path)


def main():
    ap = argparse.ArgumentParser(description="OLED 逐帧动画代码生成器")
    ap.add_argument("--demo", action="store_true", help="生成内置示例动画")
    ap.add_argument("--png", help="PNG/JPG 序列目录")
    ap.add_argument("--gif", help="GIF 文件")
    ap.add_argument("--mp4", help="视频文件(需要 ffmpeg)")
    ap.add_argument("--style", default=STYLE_PLAIN,
                    choices=[STYLE_PLAIN, STYLE_EDGE, STYLE_DITHER],
                    help="画面风格, 默认 plain")
    ap.add_argument("--fps", type=int, default=10, help="播放帧率, 默认 10")
    ap.add_argument("--threshold", type=int, default=128, help="二值化阈值, 默认 128")
    ap.add_argument("--invert", action="store_true", help="反相")
    ap.add_argument("--edge-thr", type=int, default=55, help="边缘阈值, 默认 55")
    ap.add_argument("--edge-gain", type=float, default=2.2, help="边缘增益, 默认 2.2")
    ap.add_argument("--blur", type=float, default=1.2, help="边缘高斯模糊半径, 默认 1.2")
    ap.add_argument("--dilate", action="store_true", help="边缘加粗1像素")
    ap.add_argument("--max-frames", type=int, default=0, help="最多取前 N 帧, 0=全部")
    ap.add_argument("--sample", type=int, default=0,
                    help="均匀抽取 N 帧(保留完整故事线), 0=不抽样")
    ap.add_argument("--out", default="Video_Data.c", help="输出 C 文件")
    ap.add_argument("--preview", help="导出预览拼图 PNG")
    args = ap.parse_args()

    if args.demo:
        images = demo_frames()
        title = "demo_dog_face"
        pre_styled = True          # demo 帧已经是"亮=点亮"的位图
    elif args.png:
        images = frames_from_png(args.png)
        title = os.path.basename(os.path.abspath(args.png))
        pre_styled = False
    elif args.gif:
        images = frames_from_gif(args.gif)
        title = os.path.basename(args.gif)
        pre_styled = False
    elif args.mp4:
        images = frames_from_mp4(args.mp4, args.fps)
        title = os.path.basename(args.mp4)
        pre_styled = False
    else:
        ap.print_help()
        return

    if not pre_styled:
        # 先对全部帧统一处理(包围盒基于全部帧计算), 保证不同抽样数下画面一致
        images = apply_style(images,
                             style=args.style,
                             threshold=args.threshold,
                             invert=args.invert,
                             blur=args.blur,
                             edge_threshold=args.edge_thr,
                             gain=args.edge_gain,
                             dilate=1 if args.dilate else 0)

    # 再抽样/截断, 只减少帧数, 不改变每帧内容
    if args.max_frames and len(images) > args.max_frames:
        print("截断: %d 帧 -> %d 帧 (取前 %d 帧)" % (len(images), args.max_frames, args.max_frames))
        images = images[:args.max_frames]

    if args.sample and len(images) > args.sample:
        n_orig = len(images)
        step = n_orig / float(args.sample)
        idx = [min(int(i * step), n_orig - 1) for i in range(args.sample)]
        images = [images[i] for i in idx]
        print("均匀抽样: %d 帧 -> %d 帧 (步长 %.2f)" % (n_orig, len(images), step))

    blob_size, out_h = emit_c(images, args.fps, args.out, title)

    if args.preview:
        emit_preview(images, args.preview)

    total = len(images) * FRAME_BYTES
    fit_kb = 49.5
    print("=" * 56)
    print("帧数        : {}".format(len(images)))
    print("风格        : {}".format("demo(内置)" if pre_styled else args.style))
    print("原始体积    : {} 字节 ({:.1f} KB)".format(total, total / 1024.0))
    print("压缩后      : {} 字节 ({:.1f} KB)".format(blob_size, blob_size / 1024.0))
    print("压缩比      : {:.1f}%".format(blob_size * 100.0 / total))
    print("平均每帧    : {:.0f} 字节".format(blob_size / float(len(images))))
    dur = len(images) / float(args.fps)
    print("帧率        : {} fps  (时长 {:.1f} 秒)".format(args.fps, dur))
    if args.sample and len(images) < 69 and args.sample:
        pass
    print("Flash 余量  : {:.1f} KB 可用, 占 {:.1f}%".format(
        fit_kb, blob_size / 1024.0 / fit_kb * 100))
    print("输出        : {} + {}".format(args.out, out_h))
    print("=" * 56)


if __name__ == "__main__":
    main()
