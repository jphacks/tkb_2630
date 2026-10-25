"""タスクの達成度と、いまのドアの重さを一画面で見せる。

fridge.py と同じ tasks.json を読み書きするので、
CLI (python fridge.py done 0) と併用しても食い違わない。

    ./run-string.sh                                      # 窓1: ボードにつなぐ（先に起動しておく）
    uv run --python 3.11 --with pyserial fridge_gui.py   # 窓2: level を control.json に書く

macOS 付属の Python 3.9（Tk 8.5）では画面が描かれないので、Tk 8.6 の入った 3.11 で動かす。

ボードと話すのは run-string.sh だけ。GUI は J-Link に触らないので、
run-string.sh が無くても画面だけ動く。
run-string.sh は自分が起動する前の control.json を無視するので、起動は窓1 → 窓2 の順。

E84 AI Kit がつながっていれば、レーダーで「お願い」（手を合わせて前に突き出す =
Push）を見張る。お願いされたら、次に1回開け切るまで（GRANT_S 秒で打ち切り）
level 0 にして扉を軽くする。聞くのは3時間に1回まで（GUI で Ctrl+R を押すと数え直し）。
E84 が無ければ今までどおり動く。

開け切るたびに「食べてどうだった？」と聞き、答えを reflect.py で記録する。
「学習して重さを調整する」がオンなら、時間帯ごとの最近の答えから最大の力を変える
（後悔が多ければ強く、満足が多ければ弱く）。段階（level）の決まり方は変えない。
"""

import csv
import json
import os
import queue
import threading
import time
import tkinter as tk
from tkinter import font as tkfont

import fridge  # load_tasks / save_tasks / level_for を借りる
import reflect  # 気持ちの記録と、次の重さへの返し方

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

WINDOW = "520x600"
POLL_MS = 400           # tasks.json を見に行く間隔

GRANT_COLOR = "#60a5fa"  # お願いを聞いているあいだの文字色
GRANT_S = 30            # お願いしてもこの秒数のうちに開けなければ、元の重さに戻す
GRANT_EVERY_S = 3 * 3600  # お願いを聞くのは3時間に1回まで
# 最後にお願いを聞いた時刻。GUI を起動し直しても3時間は数え続ける。消せば数え直し
GRANT_STATE_PATH = os.path.join(fridge.HERE, "logs", "onegai.json")
ASK_S = 60              # 「食べてどうだった？」をこの秒数で引っ込める（答えなければ記録しない）
NOTE_S = 4              # 「我慢できた」の表示を出しておく秒数
EVENTS_PATH = os.path.join(fridge.HERE, "logs", "events.csv")  # run-string.sh が開閉ごとに書く


def read_events():
    """run-string.sh が記録した開閉（events.csv）。gave_up が 0 なら開け切った、1 なら途中でやめた。"""
    try:
        with open(EVENTS_PATH, newline="") as f:
            return [row for row in csv.DictReader(f) if row.get("gave_up") in ("0", "1")]
    except OSError:
        return []


def load_last_grant():
    try:
        with open(GRANT_STATE_PATH, encoding="utf-8") as f:
            return float(json.load(f)["last"])
    except (OSError, ValueError, KeyError, TypeError):
        return 0.0


def save_last_grant(t):
    os.makedirs(os.path.dirname(GRANT_STATE_PATH), exist_ok=True)
    with open(GRANT_STATE_PATH, "w", encoding="utf-8") as f:
        json.dump({"last": t, "local_time": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(t))}, f)


def write_control(level, max_a):
    """control.json に level と最大電流を書く。書きかけを読まれないよう tmp から差し替える。"""
    tmp = fridge.CONTROL_PATH + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump({"level": level, "max": round(max_a, 2)}, f)
    os.replace(tmp, fridge.CONTROL_PATH)


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
        self.last_sent = None  # 最後に control.json に書いた (level, 最大電流)
        self.vars = []      # チェックボックスの状態
        self.granted = False  # お願いを聞いて、1回だけ軽くしているところ
        self.grant_until = 0.0
        self.grant_seen = 0   # お願いされた時点の「開け切った」回数
        self.events_seen = len(read_events())  # 起動前の開閉は聞かない
        self.asking = None    # 「食べてどうだった？」を聞いている開閉の記録
        self.ask_until = 0.0
        self.note_until = 0.0  # 一言（我慢できた／お願いは3時間に1回）を出しておく期限
        self.note_text = ""
        self.last_grant = load_last_grant()
        self.feelings = reflect.load()
        self.gestures = queue.Queue()
        if gesture:
            threading.Thread(target=watch_gestures, args=(self.gestures,), daemon=True).start()

        root.title("オモイゾウコ")
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

        # 学習：最近の気持ちから最大の力をどう変えているか
        self.label_learn = tk.Label(self.root, font=self.f_level, bg=BG, fg=SUB, wraplength=460)
        self.label_learn.pack(pady=(14, 0))

        # 開け切ったあとに出す「食べてどうだった？」。ふだんは隠しておく
        self.frame_ask = tk.Frame(self.root, bg=BG)
        tk.Label(self.frame_ask, text="食べてどうだった？", font=self.f_task, bg=BG, fg=FG).pack(side="left")
        for answer in reflect.ANSWERS:
            tk.Button(self.frame_ask, text=answer, font=self.f_count,
                      command=lambda a=answer: self.answer(a)).pack(side="left", padx=4)

        self.label_link = tk.Label(self.root, font=self.f_level, bg=BG, fg=SUB)
        self.label_link.pack(side="bottom", pady=10)
        self.label_link.config(text="ドアへは ./run-string.sh が送る"
                               + ("  /  E84 でお願いを見張り中" if gesture else ""))

        # 本番で動きを固定したいときは、学習をオフにする
        self.adapt = tk.BooleanVar(value=True)
        tk.Checkbutton(self.root, text="気持ちから学んで重さを調整する", variable=self.adapt,
                       command=self.refresh, font=self.f_level, bg=BG, fg=SUB, selectcolor=BG,
                       activebackground=BG, activeforeground=FG, bd=0,
                       highlightthickness=0).pack(side="bottom")

        self.canvas.bind("<Configure>", lambda e: self.draw_bar())
        # 本番直前やリハーサルのあとに、3時間の数えをリセットする
        for key in ("<Control-r>", "<Command-r>"):
            self.root.bind(key, lambda e: self.reset_grant())

    def reset_grant(self):
        self.last_grant = 0.0
        save_last_grant(0.0)
        self.note("お願いの3時間をリセットしました")
        print("\n=== お願いの3時間をリセット ===\n", flush=True)

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

        # run-string.sh が新しく記録した開閉：開け切ったら気持ちを聞き、途中でやめたら「我慢」
        events = read_events()
        for event in events[self.events_seen:]:
            self.on_event(event, tasks)
        self.events_seen = len(events)
        full = sum(1 for e in events if e["gave_up"] == "0")
        if self.asking and time.time() > self.ask_until:
            self.hide_ask()

        # お願い（Push）されたら、次に1回開け切るまで軽くする。もう軽いときは何もしない。
        # 聞くのは3時間に1回まで。それより早いお願いは断って、あとどれくらいかを出す
        while not self.gestures.empty():
            self.gestures.get()
            if self.granted or fridge.level_for(tasks) == 0:
                continue
            wait = self.last_grant + GRANT_EVERY_S - time.time()
            if wait > 0:
                h, m = divmod(int(wait // 60), 60)
                self.note(f"お願いは3時間に1回まで（あと{h}時間{m}分）")
                print(f"\n=== お願いを断った: 3時間に1回まで（あと {h}:{m:02d}）===\n", flush=True)
                continue
            self.granted = True
            self.grant_until = time.time() + GRANT_S
            self.grant_seen = full
            self.last_grant = time.time()
            save_last_grant(self.last_grant)
            print(f"\n=== お願いを聞いた: 次の1回だけ軽くする（{GRANT_S} 秒以内）===\n", flush=True)
        if self.granted and (full > self.grant_seen or time.time() > self.grant_until):
            self.granted = False
            why = "開け切った" if full > self.grant_seen else "時間切れ"
            print(f"\n=== 元の重さに戻した（{why}）===\n", flush=True)

        self.refresh()
        self.root.after(POLL_MS, self.poll)

    def on_event(self, event, tasks):
        if event["gave_up"] == "0":
            self.asking = event
            self.ask_until = time.time() + ASK_S
            self.frame_ask.pack(pady=(14, 0))
        elif int(event.get("max_dist") or 0) >= reflect.HELD_BACK_MIN_COUNTS:
            self.save_feeling(reflect.HELD_BACK, event, tasks)
            self.note("途中でやめた：我慢できた！として記録しました")

    def note(self, text):
        """学習の理由の欄に、NOTE_S 秒だけ一言を出す。"""
        self.note_text = text
        self.note_until = time.time() + NOTE_S

    def answer(self, feeling):
        if self.asking:
            self.save_feeling(feeling, self.asking, fridge.load_tasks())
        self.hide_ask()
        self.refresh()

    def hide_ask(self):
        self.asking = None
        self.frame_ask.pack_forget()

    def save_feeling(self, feeling, event, tasks):
        level = float(event.get("level") or 0)
        max_a = float(event.get("max") or reflect.base_max())
        remaining = sum(1 for t in tasks if not t["done"])
        reflect.record(feeling, event, level, max_a, remaining)
        self.feelings = reflect.load()
        print(f"=== 記録: {feeling}（引いた力 {event.get('energy_mj', '?')} mJ）===", flush=True)

    def refresh(self):
        tasks = fridge.load_tasks()
        done = sum(1 for t in tasks if t["done"])
        level = fridge.level_for(tasks)
        color, name = STAGES[fridge.LEVELS.index(level)]
        note = ""
        if self.granted:
            level, name, color = 0.0, "今回だけだよ", GRANT_COLOR
            note = f"（お願いを聞いて1回だけ開く・あと {max(0, int(self.grant_until - time.time()))} 秒）"

        if self.adapt.get():
            max_a, why = reflect.max_for(self.feelings)
        else:
            max_a, why = reflect.base_max(), "学習はオフ（いつもの重さ）"
        if time.time() < self.note_until:
            why = self.note_text

        self.label_count.config(text=f"{done} / {len(tasks)} 完了")
        self.label_state.config(text=name, fg=color)
        self.label_level.config(text=f"level {level:.2f}  最大 {max_a:.1f} A{note}")
        self.label_learn.config(text=why)
        self.ratio = done / len(tasks) if tasks else 0
        self.bar_color = color
        self.draw_bar()

        # 変わったときだけ control.json に書く（run-string.sh がドアに送る）
        sent = (level, round(max_a, 2))
        if sent != self.last_sent:
            write_control(*sent)
            self.last_sent = sent

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
