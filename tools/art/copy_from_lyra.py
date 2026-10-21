"""Copies what a hero needs from the Lyra project into the game's Content.

Run with plain Python, after tools/art/lyra_hero.py has listed the packages:
    python tools/art/copy_from_lyra.py <Hero>
It reads the packages after "# packages" in the Lyra project's
Saved/lyra_hero_<Hero>.txt and copies each one's .uasset (and .ubulk or
.uexp beside it, if any) to the same path under this working copy's
Content/, leaving alone any that are already there (the mannequin's
animations and notifies, which the game has from Lyra already). It writes
what it copied to Saved/copy_from_lyra_<Hero>.txt. Lyra's animation
modifiers, which the retargeted animations name but the game can't load,
are left behind (tools/art/fit_hero.py strips them from the animations).
"""
import os
import shutil
import sys

LYRA = r"C:\Hamid\code\Lyra\LyraStarterGame"
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main():
    hero = sys.argv[1]
    listing = os.path.join(LYRA, "Saved", f"lyra_hero_{hero}.txt")
    with open(listing, encoding="utf-8") as f:
        lines = f.read().splitlines()
    packages = [line.split("\t")[0] for line in lines[lines.index("# packages") + 1:] if line.startswith("/Game/")]
    # Lyra's animation modifiers need Lyra's own C++; tools/art/fit_hero.py strips them from the animations.
    packages = [p for p in packages if "/AnimModifiers/" not in p]
    copied, kept, missing = [], [], []
    for package in packages:
        relative = package[len("/Game/"):].replace("/", os.sep)
        found = False
        for extension in (".uasset", ".umap", ".ubulk", ".uexp"):
            source = os.path.join(LYRA, "Content", relative + extension)
            if not os.path.exists(source):
                continue
            found = True
            target = os.path.join(ROOT, "Content", relative + extension)
            if os.path.exists(target):
                kept.append(package + extension)
                continue
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(source, target)
            copied.append(package + extension)
        if not found:
            missing.append(package)
    report = [f"copied {len(copied)}, already there {len(kept)}, missing {len(missing)}"]
    report += ["missing " + m for m in missing] + ["kept " + k for k in kept] + ["copied " + c for c in copied]
    os.makedirs(os.path.join(ROOT, "Saved"), exist_ok=True)
    with open(os.path.join(ROOT, "Saved", f"copy_from_lyra_{hero}.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    print(report[0])


if __name__ == "__main__":
    main()
