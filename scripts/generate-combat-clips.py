"""Generate exact combat handles from an extracted AnimationID TSV (no game data shipped)."""
import argparse
import csv
import re
from pathlib import Path


def follows(name):
    n = name.lower()
    passive = bool(re.search(r"_(idle|strafe|lookat|walk|run)(_|$)", n)) and not re.search(
        r"flourish|combo|_dfn|directionalhit|_tr_|turn|knock|hurt", n)
    aim = bool(re.search(r"_aim", n)) and not re.search(
        r"_(throw|shoot|shot|fire|reload|lt|rt|tr|turn|start|end|deny)(_|$)", n)
    if re.match(r"xx_(freeaim|coveraim|coveraimmid|bomb)_", n):
        # These families include static blend-space poses; only authored actions
        # may own the view, never the pose used to aim a weapon continuously.
        return bool(re.search(r"_(throw|shoot|shot|fire|reload|recoil|turn|hit|hurt|start|end|lt|rt|tr)(_|$)", n)) and not passive
    return not (passive or aim)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalogue", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    with args.catalogue.open(encoding="utf-8-sig") as source:
        rows = [r for r in csv.DictReader(source, delimiter="\t") if re.match(
            r"xx_(Fight_|DirectionalHit_|FocusTurn_|HackingDodge_|Reflexshot_|Heavy_|Two_|Long_|FreeAim_|CoverAim_|CoverAimMid_|Bomb_|Wristcrossbow_|StunBomb_|tackle_|Incapacite_)", r["name"], re.I)]
    clips = {}
    for row in rows:
        handle = int(row["file_id"])
        if handle in clips and clips[handle] != row["name"]:
            raise ValueError(f"Conflicting handle {handle}")
        clips[handle] = row["name"]
    lines = ["#pragma once", "#include <cstdint>", "#include <iterator>", "namespace fp {",
             "// Generated from the local forge catalogue with scripts/generate-combat-clips.py.",
             "// Names establish candidate coverage, not gameplay verification. Only local-player",
             "// sampled clips qualify. Passive idle/locomotion/aim poses retain mouse control.",
             "struct CombatClip { std::uint64_t handle; bool follow; };",
             "inline constexpr CombatClip CombatClips[]={"]
    lines += [f" {{{h}ULL,{'true' if follows(n) else 'false'}}}, // {n}" for h,n in sorted(clips.items())]
    lines += ["};", "inline const CombatClip* FindCombatClip(std::uint64_t handle) {",
              " const auto* first=std::begin(CombatClips);const auto* last=std::end(CombatClips);",
              " while(first<last){const auto* middle=first+(last-first)/2;if(middle->handle<handle)first=middle+1;else last=middle;}",
              " return first!=std::end(CombatClips)&&first->handle==handle?first:nullptr;", "}", "}"]
    args.output.write_text("\n".join(lines)+"\n", encoding="utf-8")
    print(f"Mapped {len(clips)} combat clips; {sum(follows(n) for n in clips.values())} action candidates")

if __name__ == "__main__":
    main()
