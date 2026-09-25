#!/usr/bin/env python3
"""
rusEFI Board CI Toggle Editor

Lists every meta-info*.env (board+variant) file under firmware/config/boards/
and lets you enable/disable it for GitHub Actions with a checkbox.

Mechanism: firmware/bin/generate_matrix.sh builds the GHA build matrix by
scanning for files matching the literal glob `meta-info*.env` (see that
script for the exact `find` call). Any meta-info file whose name does not
end in exactly ".env" is silently excluded from CI. The repo has historically
used several different "disabled" suffixes (.disabled_env, ._disabled_env,
.env.disabled, .env-disabled, .env_disabled) - this tool normalizes new
disables to ".disabled_env" but recognizes and can re-enable all of them.

Toggling renames the file via `git mv` (falling back to a plain rename if
the file isn't tracked or this isn't a git checkout) - the rename is staged
but never committed.
"""
import os
import re
import subprocess
import sys
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# Force XWayland mode on Linux/Wayland to fix Tkinter popdown positioning bug (top-left of screen)
if sys.platform.startswith("linux"):
    os.environ["WAYLAND_DISPLAY"] = ""

# Color Palette (Catppuccin Mocha inspired dark theme) - matches board_config_gui.py
BG_MAIN = "#1e1e2e"
BG_PANEL = "#252538"
BG_ROW = "#2a2a3d"
FG_MAIN = "#cdd6f4"
FG_MUTED = "#a6adc8"
ACCENT = "#cba6f7"      # Lavender
ACCENT_HOVER = "#b4befe"
SUCCESS = "#a6e3a1"     # Green
ERROR = "#f38ba8"       # Red
HIGHLIGHT = "#89b4fa"   # Blue

# Any of these trailing suffixes mark a meta-info file as disabled; matching
# is order-independent since re.sub with alternation always takes the
# longest/leftmost match anchored at end-of-string via $.
DISABLED_SUFFIX_RE = re.compile(
    r'(\.env\.disabled|\.env-disabled|\.env_disabled|\._?disabled_env)$'
)
CANONICAL_DISABLED_SUFFIX = ".disabled_env"


def canonical_enabled_name(fname):
    """Given any meta-info filename (enabled or disabled, any known disabled
    suffix), return the canonical enabled filename (ending in '.env')."""
    if fname.endswith(".env"):
        return fname
    return DISABLED_SUFFIX_RE.sub(".env", fname)


def variant_of(fname):
    """Extract the human variant name from a meta-info filename, e.g.
    'meta-info-proteus_f7.env' -> 'proteus_f7'; 'meta-info.env' -> 'default'."""
    enabled_name = canonical_enabled_name(fname)
    stem = enabled_name[len("meta-info"):-len(".env")]
    stem = stem.lstrip("-")
    return stem if stem else "default"


class BoardRow:
    __slots__ = ("dir_path", "rel_dir", "fname", "variant", "enabled")

    def __init__(self, dir_path, rel_dir, fname):
        self.dir_path = dir_path
        self.rel_dir = rel_dir
        self.fname = fname
        self.variant = variant_of(fname)
        self.enabled = fname.endswith(".env")

    @property
    def full_path(self):
        return os.path.join(self.dir_path, self.fname)

    @property
    def display(self):
        return f"{self.rel_dir} ({self.variant})"


class BoardCiApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("rusEFI Board CI Toggle Editor")
        self.geometry("900x760")
        self.configure(bg=BG_MAIN)
        self.minsize(700, 500)

        self.workspace_root = tk.StringVar()
        self.boards_dir = ""
        self.rows = []           # list[BoardRow]
        self.row_widgets = []    # list[(row, check_var, frame, label)]
        self.search_var = tk.StringVar()
        self.search_var.trace_add("write", lambda *a: self.rebuild_rows())

        self.setup_styles()
        self.create_widgets()
        self.detect_default_repo()

    def setup_styles(self):
        self.style = ttk.Style()
        self.style.theme_use("default")
        self.style.configure(".", background=BG_MAIN, foreground=FG_MAIN, fieldbackground=BG_PANEL, insertcolor=FG_MAIN)
        self.style.configure("TLabel", background=BG_MAIN, foreground=FG_MAIN, font=("Helvetica", 10))
        self.style.configure("Title.TLabel", font=("Helvetica", 15, "bold"), foreground=ACCENT)
        self.style.configure("Muted.TLabel", foreground=FG_MUTED, font=("Helvetica", 9))
        self.style.configure("TFrame", background=BG_MAIN)
        self.style.configure("Panel.TFrame", background=BG_PANEL, relief="flat")
        self.style.configure("TButton", background=BG_PANEL, foreground=FG_MAIN, font=("Helvetica", 9, "bold"), padding=5)
        self.style.map("TButton", background=[("active", ACCENT)], foreground=[("active", BG_MAIN)])

    def create_widgets(self):
        main_frame = ttk.Frame(self, padding=15)
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Repo selector
        repo_frame = ttk.Frame(main_frame)
        repo_frame.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(repo_frame, text="rusEFI Board CI Toggle Editor", style="Title.TLabel").pack(anchor=tk.W)
        ttk.Label(repo_frame, text="Check = built by GitHub Actions. Uncheck = excluded (meta-info renamed to *.disabled_env).",
                  style="Muted.TLabel").pack(anchor=tk.W)
        repo_grid = ttk.Frame(repo_frame)
        repo_grid.pack(fill=tk.X, pady=(6, 0))
        ttk.Label(repo_grid, text="Repo Root: ", font=("Helvetica", 10, "bold")).grid(row=0, column=0, sticky=tk.W)
        ttk.Entry(repo_grid, textvariable=self.workspace_root).grid(row=0, column=1, sticky=tk.EW, padx=5)
        ttk.Button(repo_grid, text="Browse...", command=self.browse_repo).grid(row=0, column=2)
        repo_grid.columnconfigure(1, weight=1)
        self.repo_status_lbl = ttk.Label(repo_frame, text="Checking...", font=("Helvetica", 9, "bold"))
        self.repo_status_lbl.pack(anchor=tk.W, padx=(80, 0))
        self.workspace_root.trace_add("write", self.on_repo_changed)

        # Search + summary
        toolbar = ttk.Frame(main_frame)
        toolbar.pack(fill=tk.X, pady=(10, 5))
        ttk.Label(toolbar, text="\U0001F50D").pack(side=tk.LEFT)
        ttk.Entry(toolbar, textvariable=self.search_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        ttk.Button(toolbar, text="Refresh", command=self.refresh_data).pack(side=tk.LEFT, padx=(5, 0))
        self.summary_lbl = ttk.Label(toolbar, text="", font=("Helvetica", 9, "bold"))
        self.summary_lbl.pack(side=tk.RIGHT)

        # Scrollable rows area
        list_container = ttk.Frame(main_frame)
        list_container.pack(fill=tk.BOTH, expand=True)
        self.canvas = tk.Canvas(list_container, bg=BG_PANEL, highlightthickness=0)
        scrollbar = ttk.Scrollbar(list_container, orient="vertical", command=self.canvas.yview)
        self.rows_frame = ttk.Frame(self.canvas, style="Panel.TFrame")
        self.rows_frame.bind("<Configure>", lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all")))
        self.canvas.create_window((0, 0), window=self.rows_frame, anchor="nw")
        self.canvas.configure(yscrollcommand=scrollbar.set)
        self.canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        self.canvas.bind_all("<MouseWheel>", self._on_mousewheel)
        self.canvas.bind_all("<Button-4>", lambda e: self.canvas.yview_scroll(-3, "units"))
        self.canvas.bind_all("<Button-5>", lambda e: self.canvas.yview_scroll(3, "units"))

        # Log panel
        log_frame = ttk.Frame(main_frame)
        log_frame.pack(fill=tk.X, side=tk.BOTTOM, pady=(10, 0))
        log_header = ttk.Frame(log_frame)
        log_header.pack(fill=tk.X, pady=(0, 3))
        ttk.Label(log_header, text="Log", font=("Helvetica", 10, "bold")).pack(side=tk.LEFT)
        self.log_text = tk.Text(log_frame, height=6, bg=BG_PANEL, fg=FG_MAIN, font=("Courier New", 9))
        self.log_text.pack(fill=tk.X)
        self.log_text.tag_config("stderr", foreground=ERROR)
        self.log_text.tag_config("success", foreground=SUCCESS)
        self.log_text.tag_config("info", foreground=HIGHLIGHT)

    def _on_mousewheel(self, event):
        self.canvas.yview_scroll(-1 * (event.delta // 120), "units")

    # --- repo detection (same approach as board_config_gui.py) ---
    def detect_default_repo(self):
        if len(sys.argv) > 1 and os.path.isdir(sys.argv[1]):
            candidate = os.path.abspath(sys.argv[1])
            if os.path.exists(os.path.join(candidate, "firmware", "bin", "compile.sh")):
                self.workspace_root.set(candidate)
                return

        def find_repo_root(start_path):
            curr = os.path.abspath(start_path)
            while True:
                if os.path.exists(os.path.join(curr, "firmware", "bin", "compile.sh")):
                    return curr
                parent = os.path.dirname(curr)
                if parent == curr:
                    return None
                curr = parent

        detected = find_repo_root(os.getcwd())
        if detected:
            self.workspace_root.set(detected)
            return
        detected = find_repo_root(os.path.dirname(os.path.abspath(__file__)))
        if detected:
            self.workspace_root.set(detected)
            return

        self.append_log("[WARNING] Could not auto-detect rusEFI repository root. Prompting selection...\n", "stderr")
        folder = filedialog.askdirectory(title="Select rusEFI Repository Root Folder", initialdir=os.path.expanduser("~"))
        if folder:
            self.workspace_root.set(folder)
        else:
            self.update_repo_status(False)

    def on_repo_changed(self, *args):
        path = self.workspace_root.get().strip()
        if not path:
            self.update_repo_status(False)
            return
        compile_script = os.path.join(path, "firmware", "bin", "compile.sh")
        if os.path.exists(compile_script):
            self.boards_dir = os.path.join(path, "firmware", "config", "boards")
            self.update_repo_status(True)
            self.refresh_data()
        else:
            self.update_repo_status(False)
            self.append_log(f"[ERROR] Invalid repository path. Could not find {compile_script}\n", "stderr")

    def update_repo_status(self, is_valid):
        if is_valid:
            self.repo_status_lbl.configure(text="Valid rusEFI Repository", foreground=SUCCESS)
        else:
            self.repo_status_lbl.configure(text="Invalid Repository (Select root containing 'firmware/')", foreground=ERROR)
            self.rows = []
            self.rebuild_rows()

    def browse_repo(self):
        folder = filedialog.askdirectory(title="Select rusEFI Repository Root Folder", initialdir=self.workspace_root.get() or os.path.expanduser("~"))
        if folder:
            self.workspace_root.set(folder)

    # --- scanning ---
    def scan_boards(self):
        self.rows = []
        if not self.boards_dir or not os.path.isdir(self.boards_dir):
            return
        for root, _dirs, files in os.walk(self.boards_dir):
            for f in files:
                if f.startswith("meta-info"):
                    rel_dir = os.path.relpath(root, self.boards_dir)
                    self.rows.append(BoardRow(root, rel_dir, f))
        self.rows.sort(key=lambda r: (r.rel_dir, r.variant))

    def refresh_data(self):
        self.scan_boards()
        self.rebuild_rows()
        self.append_log(f"Scanned {len(self.rows)} meta-info file(s) under firmware/config/boards/\n", "info")

    # --- row rendering ---
    def rebuild_rows(self):
        for child in self.rows_frame.winfo_children():
            child.destroy()
        self.row_widgets = []

        q = self.search_var.get().lower()
        visible = [r for r in self.rows if q in r.display.lower()]

        enabled_count = sum(1 for r in self.rows if r.enabled)
        self.summary_lbl.configure(
            text=f"{enabled_count} enabled / {len(self.rows) - enabled_count} disabled / {len(self.rows)} total")

        for idx, row in enumerate(visible):
            self._add_row_widget(row, idx)

    def _add_row_widget(self, row, idx):
        bg = BG_ROW if idx % 2 == 0 else BG_PANEL
        frame = tk.Frame(self.rows_frame, bg=bg)
        frame.pack(fill=tk.X, expand=True)

        var = tk.BooleanVar(value=row.enabled)
        fg = SUCCESS if row.enabled else ERROR
        text = f"{row.display}\n    {row.fname}"

        chk = tk.Checkbutton(
            frame, text=text, variable=var, bg=bg, fg=fg, activebackground=bg,
            activeforeground=fg, selectcolor=bg, anchor="w", justify=tk.LEFT,
            font=("Helvetica", 10, "bold"), padx=8, pady=4,
            command=lambda r=row, v=var: self.toggle_row(r, v),
        )
        chk.pack(fill=tk.X, expand=True)
        self.row_widgets.append((row, var, frame, chk))

    # --- toggling ---
    def toggle_row(self, row, var):
        want_enabled = var.get()
        if want_enabled == row.enabled:
            return

        new_fname = (
            canonical_enabled_name(row.fname) if want_enabled
            else canonical_enabled_name(row.fname)[:-len(".env")] + CANONICAL_DISABLED_SUFFIX
        )
        old_path = row.full_path
        new_path = os.path.join(row.dir_path, new_fname)

        if os.path.exists(new_path):
            messagebox.showerror("Conflict", f"Target already exists:\n{new_path}")
            var.set(row.enabled)
            return

        try:
            self._git_or_plain_move(old_path, new_path)
        except Exception as e:
            self.append_log(f"[ERROR] Failed to rename {row.fname}: {e}\n", "stderr")
            var.set(row.enabled)
            return

        action = "ENABLED" if want_enabled else "DISABLED"
        self.append_log(f"{action}: {row.rel_dir}/{row.fname} -> {new_fname}\n", "success" if want_enabled else "info")

        row.fname = new_fname
        row.enabled = want_enabled
        self.rebuild_rows()

    def _git_or_plain_move(self, old_path, new_path):
        repo_root = self.workspace_root.get().strip()
        try:
            result = subprocess.run(
                ["git", "mv", os.path.relpath(old_path, repo_root), os.path.relpath(new_path, repo_root)],
                cwd=repo_root, capture_output=True, text=True, check=False,
            )
            if result.returncode == 0:
                return
            self.append_log(f"[WARNING] git mv failed ({result.stderr.strip()}), falling back to plain rename\n", "stderr")
        except FileNotFoundError:
            pass
        os.rename(old_path, new_path)

    def append_log(self, text, tag="info"):
        self.log_text.insert(tk.END, text, tag)
        self.log_text.see(tk.END)


if __name__ == "__main__":
    app = BoardCiApp()
    app.mainloop()
