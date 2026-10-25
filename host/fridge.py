"""タスクの達成度を、冷蔵庫のドアの重さに変換する。

「何を終えたか」から level を決めて control.json に書く。ボードと話すのは
door_link.py の run（./run-string.sh）だけで、そちらが control.json を読み直し、
mounts/*.json の設定・開閉ログ・再接続を受け持つ。
タスクは tasks.json に保存される。

    python fridge.py list                  # タスク一覧と、いまの重さを表示
    python fridge.py done 2                # 2番のタスクを完了にする
    python fridge.py undo 2                # 2番を未完了に戻す
    python fridge.py add "レポートを出す"   # タスクを追加
    python fridge.py reset                 # 全部未完了に戻す（翌日の朝など）

    ./run-string.sh                        # 窓1: ボードにつなぐ（先に起動しておく）
    python fridge.py run                   # 窓2: tasks.json を見て level を control.json に書く
    python fridge.py run --dry-run         # control.json に書かず、送るはずの level を表示するだけ
"""

import argparse
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TASKS_PATH = os.path.join(HERE, "tasks.json")
CONTROL_PATH = os.path.join(HERE, "control.json")  # door_link.py run が mtime の変化で読み直す

# 未完了の数 → level(0..1) の対応表。
# 人間が重さの違いを感じ取れるのは 10% 程度の差からなので、
# 連続的に変えても意味がない。段階ごとに約2倍にして、はっきり分かるようにする。
# 添字 = 未完了の数。これより多く残っていれば最後の値（= 一番重い）。
LEVELS = [0.0, 0.25, 0.5, 1.0]

DEFAULT_TASKS = [
    "レポートを1ページ書く",
    "ストレッチをする",
    "明日の持ち物を用意する",
]


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


def level_for(tasks):
    """未完了の数から level を決める。"""
    remaining = sum(1 for t in tasks if not t["done"])
    return LEVELS[min(remaining, len(LEVELS) - 1)]


def describe(level):
    if level == 0.0:
        return "いつもどおり開く"
    if level <= 0.25:
        return "少し引っかかる"
    if level <= 0.5:
        return "はっきり重い"
    return "両手が要る"


def show(tasks):
    for i, t in enumerate(tasks):
        mark = "x" if t["done"] else " "
        print(f"  [{mark}] {i}: {t['text']}")
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
    show(tasks)


def cmd_reset(args):
    tasks = load_tasks()
    for t in tasks:
        t["done"] = False
    save_tasks(tasks)
    show(tasks)


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
