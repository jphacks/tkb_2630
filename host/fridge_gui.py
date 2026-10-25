"""タスクの達成度と、いまのドアの重さを一画面で見せる。

fridge.py と同じ tasks.json を読み書きするので、
CLI (python fridge.py done 0) と併用しても食い違わない。

    ./run-string.sh          # 窓1: ボードにつなぐ（先に起動しておく）
    python fridge_gui.py     # 窓2: level を control.json に書く

ボードと話すのは run-string.sh だけ。GUI は J-Link に触らないので、
run-string.sh が無くても画面だけ動く。
run-string.sh は自分が起動する前の control.json を無視するので、起動は窓1 → 窓2 の順。

E84 AI Kit がつながっていれば、レーダーで「お願い」（手を合わせて前に突き出す =
Push）を見張る。お願いされたら、次に1回開け切るまで（GRANT_S 秒で打ち切り）
level 0 にして扉を軽くする。E84 が無ければ今までどおり動く。
"""

import csv
import os
import queue
import threading
import time
import tkinter as tk
from tkinter import font as tkfont

import fridge  # load_tasks / save_tasks / level_for / write_control を借りる

try:
    import gesture  # pyserial が要るので、無い環境ではお願い無しで動く
except ImportError:
    gesture = None

# --- 見た目の設定。色を変えたいときはここだけ ---

BG = "#1a1a1a"          # 背景
FG = "#e8e8e8"          # 文字
SUB = "#8a8a8a"         # 薄い文字
BAR_BG = "#333333"      # バーの地の色

# level -> (バーと文字の色, 重さの呼び名)
# fridge.LEVELS と添字を揃えてある。重み付きだと「残りの個数」と level がずれるので、level で引く
STAGES = [
    ("#4ade80", "いつもどおり開く"),   # level 0    = 全部終わった
    ("#facc15", "少し引っかかる"),     # level 0.25
    ("#fb923c", "はっきり重い"),       # level 0.5
    ("#ef4444", "両手が要る"),         # level 1.0
]

WINDOW = "520x460"
POLL_MS = 400           # tasks.json を見に行く間隔

GRANT_COLOR = "#60a5fa"  # お願いを聞いているあいだの文字色
GRANT_S = 30            # お願いしてもこの秒数のうちに開けなければ、元の重さに戻す
EVENTS_PATH = os.path.join(fridge.HERE, "logs", "events.csv")  # run-string.sh が開閉ごとに書く


def full_openings():
    """run-string.sh が記録した「開け切った」回数（events.csv の gave_up が 0 の行）。"""
    try:
        with open(EVENTS_PATH, newline="") as f:
            return sum(1 for row in csv.DictReader(f) if row.get("gave_up") == "0")
    except OSError:
        return 0


def watch_gestures(events):
    """E84 のレーダーで Push を見張り、見つけたら events に積む。抜き差しされても待ち直す。
    認識の様子はこの窓のターミナルに流す（gesture.py watch と同じ表示）。"""
    while True:
        port = gesture.find_port()
        if port:
            try:
                det = gesture.Detector()
                for scores in gesture.frames(port):
                    found = det.feed(scores)
                    print(gesture.describe(scores, found), flush=True)
                    if found == "Push":
                        events.put("Push")
            except (gesture.serial.SerialException, OSError):
                pass
        time.sleep(2)


class FridgeGUI:
    def __init__(self, root):
        self.root = root
        self.last_level = None
        self.vars = []      # チェックボックスの状態
        self.granted = False  # お願いを聞いて、1回だけ軽くしているところ
        self.grant_until = 0.0
        self.grant_seen = 0   # お願いされた時点の「開け切った」回数
        self.gestures = queue.Queue()
        if gesture:
            threading.Thread(target=watch_gestures, args=(self.gestures,), daemon=True).start()

        root.title("おあずけ冷蔵庫")
        root.configure(bg=BG)
        root.geometry(WINDOW)

        self.f_task = tkfont.Font(size=14)
        self.f_count = tkfont.Font(size=12)
        self.f_state = tkfont.Font(size=30, weight="bold")
        self.f_level = tkfont.Font(size=12)

        self.build()
        self.refresh()
        self.poll()

    def build(self):
        tasks = fridge.load_tasks()

        # チェックボックス
        self.frame_tasks = tk.Frame(self.root, bg=BG)
        self.frame_tasks.pack(pady=(28, 16), padx=32, fill="x")
        for i, t in enumerate(tasks):
            var = tk.BooleanVar(value=t["done"])
            self.vars.append(var)
            tk.Checkbutton(
                self.frame_tasks, text=t["text"], variable=var,
                command=lambda i=i: self.toggle(i),
                font=self.f_task, bg=BG, fg=FG, selectcolor=BG,
                activebackground=BG, activeforeground=FG,
                anchor="w", bd=0, highlightthickness=0, padx=4, pady=6,
            ).pack(fill="x")

        # 達成度のバー。Canvas に矩形を描くだけ
        self.canvas = tk.Canvas(
            self.root, height=14, bg=BG, highlightthickness=0
        )
        self.canvas.pack(fill="x", padx=32, pady=(12, 6))
        self.bar_bg = self.canvas.create_rectangle(0, 0, 0, 14, fill=BAR_BG, width=0)
        self.bar_fg = self.canvas.create_rectangle(0, 0, 0, 14, fill=FG, width=0)

        self.label_count = tk.Label(self.root, font=self.f_count, bg=BG, fg=SUB)
        self.label_count.pack()

        # いまの重さ
        self.label_state = tk.Label(self.root, font=self.f_state, bg=BG)
        self.label_state.pack(pady=(28, 2))

        self.label_level = tk.Label(self.root, font=self.f_level, bg=BG, fg=SUB)
        self.label_level.pack()

        self.label_link = tk.Label(self.root, font=self.f_level, bg=BG, fg=SUB)
        self.label_link.pack(side="bottom", pady=10)
        self.label_link.config(text="ドアへは ./run-string.sh が送る"
                               + ("  /  E84 でお願いを見張り中" if gesture else ""))

        self.canvas.bind("<Configure>", lambda e: self.draw_bar())

    def toggle(self, index):
        """チェックが変わったら tasks.json に書き戻す。"""
        tasks = fridge.load_tasks()
        tasks[index]["done"] = self.vars[index].get()
        fridge.save_tasks(tasks)
        self.refresh()

    def poll(self):
        """CLI 側から変更されたときのために、定期的に読み直す。"""
        tasks = fridge.load_tasks()
        for var, t in zip(self.vars, tasks):
            if var.get() != t["done"]:
                var.set(t["done"])

        # お願い（Push）されたら、次に1回開け切るまで軽くする。もう軽いときは何もしない
        while not self.gestures.empty():
            self.gestures.get()
            if not self.granted and fridge.level_for(tasks) > 0:
                self.granted = True
                self.grant_until = time.time() + GRANT_S
                self.grant_seen = full_openings()
                print(f"\n=== お願いを聞いた: 次の1回だけ軽くする（{GRANT_S} 秒以内）===\n", flush=True)
        if self.granted and (full_openings() > self.grant_seen or time.time() > self.grant_until):
            self.granted = False
            why = "開け切った" if full_openings() > self.grant_seen else "時間切れ"
            print(f"\n=== 元の重さに戻した（{why}）===\n", flush=True)

        self.refresh()
        self.root.after(POLL_MS, self.poll)

    def refresh(self):
        tasks = fridge.load_tasks()
        done = sum(1 for t in tasks if t["done"])
        level = fridge.level_for(tasks)
        color, name = STAGES[fridge.LEVELS.index(level)]
        note = ""
        if self.granted:
            level, name, color = 0.0, "今回だけだよ", GRANT_COLOR
            note = f"（お願いを聞いて1回だけ開く・あと {max(0, int(self.grant_until - time.time()))} 秒）"

        self.label_count.config(text=f"{done} / {len(tasks)} 完了")
        self.label_state.config(text=name, fg=color)
        self.label_level.config(text=f"level {level:.2f}{note}")
        self.ratio = done / len(tasks) if tasks else 0
        self.bar_color = color
        self.draw_bar()

        # level が変わったときだけ control.json に書く（run-string.sh がドアに送る）
        if level != self.last_level:
            fridge.write_control(level)
            self.last_level = level

    def draw_bar(self):
        w = self.canvas.winfo_width()
        self.canvas.coords(self.bar_bg, 0, 0, w, 14)
        self.canvas.coords(self.bar_fg, 0, 0, w * self.ratio, 14)
        self.canvas.itemconfig(self.bar_fg, fill=self.bar_color)


def main():
    # 電流・向き・距離は mounts/*.json に一本化したので、ここでは持たない
    root = tk.Tk()
    FridgeGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
