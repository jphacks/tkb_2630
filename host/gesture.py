"""PSOC Edge E84 AI Kit のレーダーで、手の動き（Push / Circle）を受け取る。

E84 には edge-fw/（Infineon の DEEPCRAFT radar gesture のコード例）を書き込んである。
E84 は1秒に7回ほど推論して、KitProg3 の USB シリアルに次の形で書き出す。

    label: unlabelled: score: 0.57
    label: Circle    : score: 0.43
    label: Push      : score: 0.00
    Output: unlabelled

誰も手を出していなくても単発で Push / Circle が出ることがあるので、
スコアが THRESHOLDS 以上の推論が HITS 回続いたときだけ「その動きをした」とみなす。
Circle は誰もいなくても 0.4〜0.8 を行き来して誤検知が多いので、Push だけを使う。

    python gesture.py watch     # スコアを表示して、手の動きが認識されるか試す
"""

import argparse
import re
import sys
import time

import serial
from serial.tools import list_ports

BAUD = 115200
THRESHOLDS = {"Push": 0.6}  # この score 以上を「その動きらしい」とみなす。無いラベルは使わない
HITS = 2          # 何回続いたら動きとして確定するか（1回 ≒ 0.15 秒）
COOLDOWN_S = 2.0  # 一度確定したら、しばらく同じ動きを数えない

LABEL_RE = re.compile(r"label:\s*(\w+)\s*:\s*score:\s*(-?[\d.]+)")


def find_port():
    """E84 の KitProg3 のシリアルポート。無ければ None（J-Link のポートは選ばない）。"""
    for p in list_ports.comports():
        if "KitProg3" in (p.description or "") or "KitProg3" in (p.product or ""):
            return p.device
    return None


def frames(port):
    """推論1回ぶんのスコアを {"unlabelled": .., "Circle": .., "Push": ..} で返し続ける。"""
    s = serial.Serial(port, BAUD, timeout=0.5)
    scores, line = {}, b""
    while True:
        line += s.read(256)
        *done, line = line.split(b"\n")
        for raw in done:
            text = raw.decode(errors="replace")
            m = LABEL_RE.search(text)
            if m:
                scores[m.group(1)] = float(m.group(2))
            elif "Output:" in text and scores:
                yield scores
                scores = {}


class Detector:
    """スコアの列から「Push した」「Circle した」を確定する。"""

    def __init__(self, thresholds=THRESHOLDS, hits=HITS, cooldown_s=COOLDOWN_S):
        self.thresholds, self.hits, self.cooldown_s = thresholds, hits, cooldown_s
        self.streak = {}
        self.last = float("-inf")

    def feed(self, scores, now=None):
        now = time.time() if now is None else now
        found = None
        for label, score in scores.items():
            if label not in self.thresholds:
                continue
            self.streak[label] = self.streak.get(label, 0) + 1 if score >= self.thresholds[label] else 0
            if self.streak[label] >= self.hits and now - self.last > self.cooldown_s:
                found = label
        if found:
            self.last = now
            self.streak = {}
        return found


def describe(scores, found=None):
    """推論1回ぶんを1行に: Push のスコアを棒で、確定したら <<< Push >>>。"""
    push = scores.get("Push", 0.0)
    return f"Push {push:4.2f} {'#' * int(push * 10):10s}" + (f"   <<< {found} >>>" if found else "")


def cmd_watch(args):
    port = args.port or find_port()
    if not port:
        sys.exit("E84 のシリアルポートが見つかりません。KitProg3 側の USB をつないでください")
    print(f"{port} を読んでいます。60cm くらい離れて手を動かしてください。Ctrl+C で終了\n")
    det = Detector({"Push": args.threshold}, args.hits)
    try:
        for scores in frames(port):
            print(describe(scores, det.feed(scores)), flush=True)
    except KeyboardInterrupt:
        pass


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", help="省略すると KitProg3 のポートを自動で探す")
    p.add_argument("--threshold", type=float, default=THRESHOLDS["Push"], help="Push とみなす score")
    p.add_argument("--hits", type=int, default=HITS)
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("watch").set_defaults(func=cmd_watch)
    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
