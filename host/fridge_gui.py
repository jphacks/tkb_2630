"""タスクの達成度と、いまのドアの重さを一画面で見せる。

fridge.py と同じ tasks.json を読み書きするので、
CLI (python fridge.py done 0) と併用しても食い違わない。

    ./run-string.sh          # 窓1: ボードにつなぐ（先に起動しておく）
    python fridge_gui.py     # 窓2: level を control.json に書く

ボードと話すのは run-string.sh だけ。GUI は J-Link に触らないので、
run-string.sh が無くても画面だけ動く。
run-string.sh は自分が起動する前の control.json を無視するので、起動は窓1 → 窓2 の順。
"""

import tkinter as tk
from tkinter import font as tkfont

import fridge  # load_tasks / save_tasks / level_for / write_control を借りる

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
    def __init__(self, root):
        self.root = root
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
        self.label_link.config(text="ドアへは ./run-string.sh が送る")

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
