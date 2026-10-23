# frames_to_gif.py - P-4: assemble PNG frames into a palette-optimized GIF.
# Usage: python frames_to_gif.py <framesDir[,framesDir2,...]> <out.gif> [width=1280] [colors=128] [duration=110] [skip=0]
import io, os, sys
from PIL import Image

def main():
    dirs = sys.argv[1].split(",")
    out = sys.argv[2]
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 1280
    colors = int(sys.argv[4]) if len(sys.argv) > 4 else 128
    duration = int(sys.argv[5]) if len(sys.argv) > 5 else 110
    skip = int(sys.argv[6]) if len(sys.argv) > 6 else 0

    frames = []
    for d in dirs:
        names = sorted(f for f in os.listdir(d) if f.endswith(".png"))
        for n in names:
            frames.append(os.path.join(d, n))
    frames = frames[skip:]
    if not frames:
        print("no frames"); sys.exit(1)

    imgs = []
    for p in frames:
        im = Image.open(p).convert("RGB")
        if im.width > width:
            im = im.resize((width, int(im.height * width / im.width)), Image.LANCZOS)
        imgs.append(im.convert("P", palette=Image.ADAPTIVE, colors=colors))
    imgs[0].save(out, save_all=True, append_images=imgs[1:], duration=duration,
                 loop=0, optimize=True)
    size_mb = os.path.getsize(out) / 1048576.0
    print(f"GIF {out}: {len(imgs)} frames, {size_mb:.2f} MB")
    if size_mb > 5.0:
        print("WARN: exceeds 5MB budget - consider width=960 / colors=96 / fewer frames")

if __name__ == "__main__":
    main()
