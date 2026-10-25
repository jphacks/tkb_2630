"""開けたあとの気持ちを記録して、次の重さに返す。

ドアを開け切るたびに、GUI が「食べてどうだった？」と聞く（満足 / ふつう / 後悔）。
答えは、run-string.sh が記録したその回の開閉（引いた力・距離・時間）と一緒に
logs/feelings.csv に残す。途中でやめた回は、聞かずに「我慢」として残す。

次の重さへの返し方は、説明できるように単純な決まりにしてある。
時間帯（昼 / 夜）ごとに最近 RECENT 回の答えを見て、後悔が多ければ最大の力を強く、
満足が多ければ弱くする。倍率は SCALE_MIN〜SCALE_MAX、電流は MAX_A_CAP まで。

    python3 reflect.py                     # logs/feelings.csv を日ごとの表にする
    python3 reflect.py path/to/sample.csv  # 別の CSV（デモ用のサンプルなど）で同じ表を出す
"""

import csv
import json
import os
import sys
import time
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
FEELINGS_PATH = os.path.join(HERE, "logs", "feelings.csv")
MOUNT_PATH = os.path.join(HERE, "mounts", "string.json")

ANSWERS = ("満足", "ふつう", "後悔")   # GUI のボタン
HELD_BACK = "我慢"                    # 途中でやめた回（聞かずに記録）
HELD_BACK_MIN_COUNTS = 6              # これより短い引き（約1cm 未満）は触っただけとみなし、記録しない

RECENT = 5           # 時間帯ごとに、最近の何回の答えを見るか
MIN_ANSWERS = 2      # これより少なければ調整しない
SCALE_MIN, SCALE_MAX = 0.75, 1.25
MAX_A_CAP = 2.5      # [A] ファームの上限 3A より余裕を持たせる

FIELDS = ["wall_s", "local_time", "slot", "feeling", "level", "max_a", "energy_mj", "peak_iq",
          "max_dist", "duration_ms", "remaining_tasks"]


def slot_of(hour):
    """夜（21時〜翌5時）と昼で分ける。誘惑の強さは時間帯で違うので、別々に学ぶ。"""
    return "夜" if hour >= 21 or hour < 5 else "昼"


def base_max():
    """string.json の最大電流（学習で倍率をかける前の値）。"""
    try:
        with open(MOUNT_PATH, encoding="utf-8") as f:
            return float(json.load(f)["settings"]["max"])
    except (OSError, ValueError, KeyError):
        return 2.0


def record(feeling, event, level, max_a, remaining_tasks, path=None):
    """1回分の気持ちを、その回の開閉の記録と一緒に追記する。"""
    path = path or FEELINGS_PATH
    wall = float(event.get("wall_s") or time.time())
    new = not os.path.exists(path)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "a", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        if new:
            w.writerow(FIELDS)
        w.writerow([f"{wall:.3f}", time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(wall)),
                    slot_of(time.localtime(wall).tm_hour), feeling, f"{level:g}", f"{max_a:.2f}",
                    event.get("energy_mj", ""), event.get("peak_iq", ""), event.get("max_dist", ""),
                    event.get("duration_ms", ""), remaining_tasks])


def load(path=None):
    path = path or FEELINGS_PATH
    try:
        with open(path, newline="", encoding="utf-8") as f:
            return [r for r in csv.DictReader(f) if r.get("feeling")]
    except OSError:
        return []


def adapt(rows, now=None):
    """いまの時間帯の最近の答えから、最大電流の倍率と、その理由の一文を返す。"""
    now = time.time() if now is None else now
    slot = slot_of(time.localtime(now).tm_hour)
    recent = [r["feeling"] for r in rows if r["slot"] == slot and r["feeling"] in ANSWERS][-RECENT:]
    if len(recent) < MIN_ANSWERS:
        return 1.0, f"{slot}の記録がまだ少ないので、いつもの重さです"
    c = Counter(recent)
    regret, satisfied = c["後悔"] / len(recent), c["満足"] / len(recent)
    scale = min(max(1.0 + 0.4 * (regret - satisfied), SCALE_MIN), SCALE_MAX)
    if scale > 1.0:
        why = "後悔が多いので、少し重くしています"
    elif scale < 1.0:
        why = "満足が多いので、少し軽くしています"
    else:
        why = "いつもの重さです"
    return scale, f"{slot}の最近{len(recent)}回：後悔{c['後悔']}・満足{c['満足']}・ふつう{c['ふつう']} → {why}"


def max_for(rows, now=None):
    """学習を反映した最大電流 [A] と、その理由。"""
    scale, why = adapt(rows, now)
    return min(base_max() * scale, MAX_A_CAP), why


def summary(rows):
    """日ごとの表（README・スライド用）。"""
    days = defaultdict(list)
    for r in rows:
        days[r["local_time"][:10]].append(r)
    lines = ["| 日付 | 開けた | 我慢した | 満足 | ふつう | 後悔 | 後悔の割合 | 開けたときの力（中央値） |",
             "|---|---|---|---|---|---|---|---|"]
    for day, rs in sorted(days.items()):
        c = Counter(r["feeling"] for r in rs)
        answered = sum(c[a] for a in ANSWERS)
        energies = sorted(float(r["energy_mj"]) for r in rs
                          if r["feeling"] in ANSWERS and r.get("energy_mj"))
        median = f"{energies[len(energies) // 2]:.0f} mJ" if energies else "-"
        rate = f"{c['後悔'] / answered:.0%}" if answered else "-"
        lines.append(f"| {day} | {answered} | {c[HELD_BACK]} | {c['満足']} | {c['ふつう']} | {c['後悔']} | "
                     f"{rate} | {median} |")
    return "\n".join(lines)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else FEELINGS_PATH
    rows = load(path)
    if not rows:
        sys.exit(f"{path} に記録がありません")
    print(summary(rows))
    print()
    for hour, label in ((14, "昼"), (23, "夜")):
        t = time.mktime(time.localtime()[:3] + (hour, 0, 0, 0, 0, -1))
        max_a, why = max_for(rows, t)
        print(f"{label}の次の重さ: 最大 {max_a:.2f} A（{why}）")


if __name__ == "__main__":
    main()
