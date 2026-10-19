from pathlib import Path
import json
import shutil

SOURCE = Path(r"F:\Launcher\SteamLibrary\steamapps\common\MMORPG Tycoon 2\MT2_Multiplayer")
ROOT = Path(__file__).resolve().parents[1]

for name in ("src", "proxy", "assets", "test", "tools"):
    source = SOURCE / name
    target = ROOT / name
    shutil.copytree(source, target, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns(".git", "build", "__pycache__"))
shutil.copytree(SOURCE / "include" / "mt2mp", ROOT / "include" / "mt2mp")
for name in ("imgui", "minhook", "steam"):
    shutil.copytree(SOURCE / "include" / "vendor" / name,
                    ROOT / "include" / "vendor" / name,
                    ignore=shutil.ignore_patterns(".git", "build", "examples"))
shutil.copy2(SOURCE / "CMakeLists.txt", ROOT / "CMakeLists.txt")
for name in ("_drafts", "artifacts"):
    (ROOT / name).mkdir(exist_ok=True)
print(json.dumps({"operation": "import-prototype", "source": str(SOURCE),
                  "target": str(ROOT), "files": sum(f.is_file() for f in ROOT.rglob("*"))}))
