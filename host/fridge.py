"""タスクの達成度を、冷蔵庫のドアの重さに変換する。

「何を終えたか」から level を決めて control.json に書く。ボードと話すのは
door_link.py の run（./run-string.sh）だけで、そちらが control.json を読み直し、
mounts/*.json の設定・開閉ログ・再接続を受け持つ。
タスクは tasks.json に保存される。

    python fridge.py list                  # タスク一覧と、いまの重さを表示
    python fridge.py done 2                # 2番のタスクを完了にする
    python fridge.py undo 2                # 2番を未完了に戻す
    python fridge.py add "レポートを出す"   # タスクを追加（重みを推定する）
    python fridge.py reset                 # 全部未完了に戻す（翌日の朝など）
    python fridge.py weights               # いまの重みを表示

    ./run-string.sh                        # 窓1: ボードにつなぐ（先に起動しておく）
    python fridge.py run                   # 窓2: tasks.json を見て level を control.json に書く
    python fridge.py run --dry-run         # control.json に書かず、送るはずの level を表示するだけ

## 重み付けについて

やるべきことの達成度は、本来「個数」ではなく「重さ」で決まる。
レポートを1ページ書くのと、ストレッチをするのとでは、終えたときの意味が違う。

そこでタスクごとに重み(0..1)を持たせ、未完了の重み合計から level を決める。
重みは add したときに一度だけ推定し、weights.json にキャッシュする。

安全のため、次のようにしてある。

- 重みが全部 1.0 なら、従来の「未完了の個数」とほぼ一致する
- weights.json が無い／壊れている／APIキーが無い／通信に失敗した
  → すべて重み 1.0 として扱う。つまり従来の挙動に戻るだけ
- API を呼ぶのは add のときだけ。制御ループ（run）からは一切呼ばない

API キーは環境変数 ANTHROPIC_API_KEY から読む。コードには書かない。
画面にも履歴にも残さず設定するには：

    read -rs "ANTHROPIC_API_KEY?API key: " && export ANTHROPIC_API_KEY
"""

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
TASKS_PATH = os.path.join(HERE, "tasks.json")
CONTROL_PATH = os.path.join(HERE, "control.json")  # door_link.py run が mtime の変化で読み直す
WEIGHTS_PATH = os.path.join(HERE, "weights.json")  # タスク文字列 -> 重み(0..1)

# 未完了の重み合計 → level(0..1) の対応表。
# 人間が重さの違いを感じ取れるのは 10% 程度の差からなので、
# 連続的に変えても意味がない。段階ごとに約2倍にして、はっきり分かるようにする。
LEVELS = [0.0, 0.25, 0.5, 1.0]

DEFAULT_TASKS = [
    "レポートを1ページ書く",
    "ストレッチをする",
    "明日の持ち物を用意する",
]

# 重みを推定できなかったときの値。1.0 にしておくと従来の挙動と一致する。
DEFAULT_WEIGHT = 1.0

API_URL = "https://api.anthropic.com/v1/messages"
API_MODEL = "claude-haiku-5-5"
API_TIMEOUT = 10.0


# --- タスク ---

def load_tasks():
    """tasks.json を読む。無ければ初期タスクで作る。"""
    if not os.path.exists(TASKS_PATH):
        tasks = [{"text": t, "done": False} for t in DEFAULT_TASKS]
        save_tasks(tasks)
        return tasks
    with open(TASKS_PATH, encoding="utf-8") as f:
        return json.load(f)


def save_tasks(tasks):
    with open(TASKS_PATH, "w", encoding="utf-8") as f:
        json.dump(tasks, f, ensure_ascii=False, indent=2)


# --- 重み ---

def load_weights():
    """weights.json を読む。無い・壊れている場合は空（= 全部 DEFAULT_WEIGHT）。"""
    try:
        with open(WEIGHTS_PATH, encoding="utf-8") as f:
            weights = json.load(f)
        return weights if isinstance(weights, dict) else {}
    except (OSError, ValueError):
        return {}


def save_weights(weights):
    with open(WEIGHTS_PATH, "w", encoding="utf-8") as f:
        json.dump(weights, f, ensure_ascii=False, indent=2)


def estimate_weight(text):
    """タスクの重さを 0..1 で推定する。失敗したら None を返す。

    ここが唯一の API 呼び出し。add のときだけ通る。
    SDK は使わず標準ライブラリだけで叩くので、pip install は要らない。
    """
    key = os.environ.get("ANTHROPIC_API_KEY")
    if not key:
        return None

    prompt = (
        "次のタスクの「やり終えたときの重み」を 0 から 1 の数値で答えてください。\n"
        "重いタスク（時間がかかる・先延ばしにしやすい・やると達成感が大きい）ほど 1 に近く、\n"
        "軽いタスク（すぐ終わる・習慣的）ほど 0 に近くします。\n"
        "数値だけを返してください。説明は不要です。\n\n"
        f"タスク: {text}"
    )
    body = json.dumps({
        "model": API_MODEL,
        "max_tokens": 16,
        "messages": [{"role": "user", "content": prompt}],
    }).encode("utf-8")

    req = urllib.request.Request(API_URL, data=body, method="POST", headers={
        "x-api-key": key,
        "anthropic-version": "2023-06-01",
        "content-type": "application/json",
    })

    try:
        with urllib.request.urlopen(req, timeout=API_TIMEOUT) as res:
            data = json.load(res)
        answer = data["content"][0]["text"].strip()
        value = float(answer)
        return min(max(value, 0.0), 1.0)
    except Exception as e:
        # 通信・認証・形式、どれで失敗しても推定なしにするだけ。
        # ここで例外を上に投げると add が落ちるので、必ず握りつぶす。
        print(f"  （重みの推定に失敗しました: {e}）")
        return None


def weight_of(text, weights):
    """重みを引く。分からなければ DEFAULT_WEIGHT。"""
    value = weights.get(text, DEFAULT_WEIGHT)
    try:
        return float(value)
    except (TypeError, ValueError):
        return DEFAULT_WEIGHT


# --- 達成度 → level ---

def level_for(tasks):
    """未完了タスクの重み合計から level を決める。

    重みが全部 1.0 なら「未完了の個数」とほぼ一致する。
    weights.json が無ければ全部 1.0 になるので、従来の挙動に戻る。
    """
    weights = load_weights()
    remaining = sum(weight_of(t["text"], weights) for t in tasks if not t["done"])
    total = sum(weight_of(t["text"], weights) for t in tasks)
    if total <= 0:
        return 0.0
    idx = round(remaining / total * (len(LEVELS) - 1))
    return LEVELS[min(max(idx, 0), len(LEVELS) - 1)]


def describe(level):
    if level == 0.0:
        return "いつもどおり開く"
    if level <= 0.25:
        return "少し引っかかる"
    if level <= 0.5:
        return "はっきり重い"
    return "両手が要る"


def show(tasks):
    weights = load_weights()
    for i, t in enumerate(tasks):
        mark = "x" if t["done"] else " "
        w = weight_of(t["text"], weights)
        suffix = "" if t["text"] not in weights else f"  (重み {w:.2f})"
        print(f"  [{mark}] {i}: {t['text']}{suffix}")
    done = sum(1 for t in tasks if t["done"])
    level = level_for(tasks)
    print(f"\n  {done}/{len(tasks)} 完了  ->  level {level:.2f}  ({describe(level)})")


# --- タスクをいじるコマンド ---

def cmd_list(args):
    show(load_tasks())


def cmd_done(args):
    set_done(args.index, True)


def cmd_undo(args):
    set_done(args.index, False)


def set_done(index, value):
    tasks = load_tasks()
    if not 0 <= index < len(tasks):
        sys.exit(f"{index} というタスクはありません（0〜{len(tasks)-1}）")
    tasks[index]["done"] = value
    save_tasks(tasks)
    show(tasks)


def cmd_add(args):
    tasks = load_tasks()
    tasks.append({"text": args.text, "done": False})
    save_tasks(tasks)

    # 重みの推定はここだけ。制御ループからは呼ばない。
    weights = load_weights()
    if args.text not in weights:
        w = estimate_weight(args.text)
        if w is not None:
            weights[args.text] = round(w, 2)
            save_weights(weights)

    show(tasks)


def cmd_reset(args):
    tasks = load_tasks()
    for t in tasks:
        t["done"] = False
    save_tasks(tasks)
    show(tasks)


def cmd_weights(args):
    weights = load_weights()
    if not weights:
        print("  weights.json はありません（全タスクの重みは 1.0 として扱われます）")
        return
    for text, w in weights.items():
        print(f"  {w:.2f}  {text}")


# --- ドアに level を渡すコマンド ---

def write_control(level):
    """control.json に {"level": X} を書く。

    run は mtime の変化で読み直すので、書きかけを読まれると JSON が壊れる。
    tmp に書いてから os.replace で一度に差し替える。
    """
    tmp = CONTROL_PATH + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump({"level": level}, f)
    os.replace(tmp, CONTROL_PATH)


def cmd_run(args):
    """tasks.json を監視して、level が変わったら control.json に書く。

    --dry-run（別名 --no-door）なら control.json に書かず、送るはずの level を表示するだけ。
    ここからは API を呼ばない。ネットワークが切れても止まらない。
    """
    if args.dry_run:
        print("--dry-run: control.json には書きません")
    else:
        print(f"level を {CONTROL_PATH} に書きます（先に ./run-string.sh を起動しておく）")

    print("tasks.json を監視中。別の端末から done / undo を実行してください。Ctrl+C で終了。\n")

    last = None
    try:
        while True:
            level = level_for(load_tasks())
            if level != last:
                print(f"level -> {level:.2f}  ({describe(level)})")
                if not args.dry_run:
                    write_control(level)
                last = level
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass


def main():
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("list").set_defaults(func=cmd_list)
    sub.add_parser("reset").set_defaults(func=cmd_reset)
    sub.add_parser("weights").set_defaults(func=cmd_weights)

    d = sub.add_parser("done")
    d.add_argument("index", type=int)
    d.set_defaults(func=cmd_done)

    u = sub.add_parser("undo")
    u.add_argument("index", type=int)
    u.set_defaults(func=cmd_undo)

    a = sub.add_parser("add")
    a.add_argument("text")
    a.set_defaults(func=cmd_add)

    r = sub.add_parser("run")
    # 電流や向きは mounts/*.json に一本化したので、ここでは持たない
    r.add_argument("--dry-run", "--no-door", dest="dry_run", action="store_true",
                   help="control.json に書かず、送るはずの level を表示するだけ")
    r.set_defaults(func=cmd_run)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
