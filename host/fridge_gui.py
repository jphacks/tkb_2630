"""タスクの達成度と、いまのドアの重さを一画面で見せる。

fridge.py と同じ tasks.json を読み書きするので、
CLI (python fridge.py done 0) と併用しても食い違わない。

    python fridge_gui.py                 # ハードなし。表示だけ
    python fridge_gui.py --open-dir -1   # ドアに繋ぐ

ハードに繋がらなくても起動する。繋がっていなければ画面だけ動く。
"""

import argparse
import tkinter as tk
from tkinter import font as tkfont

import fridge  # load_tasks / save_tasks / level_for / LEVELS を借りる

# --- 見た目の設定。色を変えたいときはここだけ ---

BG = "#1a1a1a"          # 背景
FG = "#e8e8e8"          # 文字
SUB = "#8a8a8a"         # 薄い文字
BAR_BG = "#333333"      # バーの地の色

# 未完了の数 -> (バーと文字の色, 重さの呼び名)
# fridge.LEVELS と添字を揃えてある
STAGES = [
    ("#4ade80", "いつもどおり開く"),   # 0個 残り = 全部終わった
    ("#facc15", "少し引っかかる"),     # 1個 残り
    ("#fb923c", "はっきり重い"),       # 2個 残り
    ("#ef4444", "両手が要る"),         # 3個以上 残り
]

WINDOW = "520x460"
POLL_MS = 400           # tasks.json を見に行く間隔


class FridgeGUI:
    def __init__(self, root, link=None):
        self.root = root
        self.link = link
        self.last_level = None
        self.vars = []      # チェックボックスの状態

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
        self.label_link.config(
            text="ドアに接続中" if self.link else "表示のみ（ドア未接続）"
        )

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
        self.refresh()
        self.root.after(POLL_MS, self.poll)

    def refresh(self):
        tasks = fridge.load_tasks()
        done = sum(1 for t in tasks if t["done"])
        remaining = len(tasks) - done
        level = fridge.level_for(tasks)
        color, name = STAGES[min(remaining, len(STAGES) - 1)]

        self.label_count.config(text=f"{done} / {len(tasks)} 完了")
        self.label_state.config(text=name, fg=color)
        self.label_level.config(text=f"level {level:.2f}")
        self.ratio = done / len(tasks) if tasks else 0
        self.bar_color = color
        self.draw_bar()

        # level が変わったときだけドアに送る
        if level != self.last_level:
            if self.link:
                self.link.write(level=level)
            self.last_level = level

    def draw_bar(self):
        w = self.canvas.winfo_width()
        self.canvas.coords(self.bar_bg, 0, 0, w, 14)
        self.canvas.coords(self.bar_fg, 0, 0, w * self.ratio, 14)
        self.canvas.itemconfig(self.bar_fg, fill=self.bar_color)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--open-dir", type=int, choices=(-1, 1),
                   help="指定するとドアに接続する。calibrate で調べた値")
    p.add_argument("--wind-sign", type=int, choices=(-1, 1), default=1)
    p.add_argument("--i-take", type=float, default=0.2)
    p.add_argument("--i-max", type=float, default=1.0)
    args = p.parse_args()

    link = None
    if args.open_dir is not None:
        from door_link import DoorLink

        link = DoorLink()
        link.write(armed=0, level=0.0, i_take=args.i_take,
                   i_resist_max=args.i_max, open_dir=args.open_dir,
                   wind_sign=args.wind_sign)
        link.start_heartbeat()
        link.write(armed=1)

    root = tk.Tk()
    FridgeGUI(root, link)
    try:
        root.mainloop()
    finally:
        if link:
            link.close()


if __name__ == "__main__":
    main()
