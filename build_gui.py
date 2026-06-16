#!/usr/bin/env python3
import json
import os
import sys

# Force XWayland mode on Linux/Wayland to fix Tkinter popdown positioning bug (top-left of screen)
if sys.platform.startswith("linux"):
    os.environ["WAYLAND_DISPLAY"] = ""
import shutil
import subprocess
import threading
import zipfile
import queue
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog
from datetime import datetime

CONFIG_FILE = os.path.join(os.path.expanduser("~"), ".rusefi_build_gui.json")

# Color Palette (Catppuccin Mocha inspired dark theme)
BG_MAIN = "#1e1e2e"
BG_PANEL = "#252538"
FG_MAIN = "#cdd6f4"
FG_MUTED = "#a6adc8"
ACCENT = "#cba6f7"      # Lavender
ACCENT_HOVER = "#b4befe"
SUCCESS = "#a6e3a1"     # Green
ERROR = "#f38ba8"       # Red
HIGHLIGHT = "#89b4fa"   # Blue

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("rusEFI Board Compiler & Bundler")
        self.geometry("850x700")
        self.configure(bg=BG_MAIN)
        self.minsize(750, 550)
        
        # Paths & State Variables
        self.workspace_root = tk.StringVar()
        self.firmware_dir = ""
        self.boards_dir = ""
        
        self.use_custom_board = tk.BooleanVar(value=False)
        self.custom_board_dir = tk.StringVar()
        self.dest_dir = tk.StringVar()
        self.extract_zip = tk.BooleanVar(value=True)
        self.delete_zip = tk.BooleanVar(value=False)
        self.clean_build = tk.BooleanVar(value=False)
        
        self.boards_list = []
        self.custom_env_files = []
        self.selected_board = None
        
        self.build_thread = None
        self.stop_build_flag = False
        self.process = None
        self.queue = queue.Queue()
        self.process_queue()
        
        # UI Styles & Widgets Setup
        self.setup_styles()
        self.create_widgets()
        
        # Detect default repository root
        self.detect_default_repo()
        
        # Set default destination folder
        if self.workspace_root.get():
            self.dest_dir.set(os.path.join(self.workspace_root.get(), "out"))
        else:
            self.dest_dir.set(os.path.join(os.path.expanduser("~"), "rusefi_build_output"))

        # Override default with last-used directory, then persist future changes
        self.load_config()
        self.dest_dir.trace_add("write", self.save_config)

    def detect_default_repo(self):
        # Scan upwards from script location to find rusEFI repo root
        curr = os.path.dirname(os.path.abspath(__file__))
        detected_root = None
        while True:
            if os.path.exists(os.path.join(curr, "firmware", "bin", "compile.sh")):
                detected_root = curr
                break
            parent = os.path.dirname(curr)
            if parent == curr:
                break
            curr = parent
            
        if detected_root:
            self.workspace_root.set(detected_root)
            self.on_repo_changed()
        else:
            self.append_log("[WARNING] Could not auto-detect rusEFI repository root. Please select it manually.\n", "stderr")
            self.update_repo_status(False)

    def on_repo_changed(self, *args):
        path = self.workspace_root.get().strip()
        if not path:
            self.update_repo_status(False)
            return
            
        # Verify compile.sh exists
        f_dir = os.path.join(path, "firmware")
        compile_script = os.path.join(f_dir, "bin", "compile.sh")
        
        if os.path.exists(compile_script):
            self.firmware_dir = f_dir
            self.boards_dir = os.path.join(f_dir, "config", "boards")
            self.update_repo_status(True)
            if not self.use_custom_board.get():
                self.scan_boards()
        else:
            self.update_repo_status(False)
            self.append_log(f"[ERROR] Invalid repository path. Could not find {compile_script}\n", "stderr")

    def update_repo_status(self, is_valid):
        if is_valid:
            self.repo_status_lbl.configure(text="Valid rusEFI Repository", foreground=SUCCESS)
            self.build_btn.configure(state=tk.NORMAL)
            self.board_combo.configure(state="normal" if not self.use_custom_board.get() else "disabled")
        else:
            self.repo_status_lbl.configure(text="Invalid Repository (Select root containing 'firmware/')", foreground=ERROR)
            self.build_btn.configure(state=tk.DISABLED)
            self.board_combo.configure(state="disabled")

    def browse_repo(self):
        folder = filedialog.askdirectory(title="Select rusEFI Repository Root Folder", initialdir=self.workspace_root.get() or os.path.expanduser("~"))
        if folder:
            self.workspace_root.set(folder)
            self.on_repo_changed()

    def setup_styles(self):
        self.style = ttk.Style()
        self.style.theme_use("default")
        
        # Configure overall style
        self.style.configure(".", background=BG_MAIN, foreground=FG_MAIN, fieldbackground=BG_PANEL, insertcolor=FG_MAIN)
        self.style.configure("TLabel", background=BG_MAIN, foreground=FG_MAIN, font=("Helvetica", 10))
        self.style.configure("Title.TLabel", font=("Helvetica", 16, "bold"), foreground=ACCENT, background=BG_MAIN)
        self.style.configure("Sub.TLabel", font=("Helvetica", 11, "bold"), foreground=HIGHLIGHT, background=BG_MAIN)
        self.style.configure("Muted.TLabel", foreground=FG_MUTED, font=("Helvetica", 9), background=BG_MAIN)
        
        # Frame styles
        self.style.configure("TFrame", background=BG_MAIN)
        self.style.configure("Panel.TFrame", background=BG_PANEL, relief="flat")
        self.style.configure("Card.TFrame", background=BG_PANEL, borderwidth=1, relief="solid")
        self.style.configure("TLabelframe", background=BG_MAIN, foreground=ACCENT)
        self.style.configure("TLabelframe.Label", background=BG_MAIN, foreground=ACCENT, font=("Helvetica", 10, "bold"))
        
        # Combobox style
        self.style.configure("TCombobox", fieldbackground=BG_PANEL, background=BG_PANEL, foreground=FG_MAIN, arrowcolor=ACCENT)
        self.style.map("TCombobox",
            fieldbackground=[("readonly", BG_PANEL)],
            foreground=[("readonly", FG_MAIN)],
            selectbackground=[("readonly", ACCENT)],
            selectforeground=[("readonly", BG_MAIN)]
        )
        
        # Note: We do not style *TCombobox*Listbox globally because it triggers a bug 
        # on Linux/X11 where the popdown menu detaches and renders at the top-left of the screen (0, 0).
        
        # Button styles
        self.style.configure("TButton", background=BG_PANEL, foreground=FG_MAIN, borderwidth=0, relief="flat", font=("Helvetica", 10, "bold"), padding=6)
        self.style.map("TButton",
            background=[("active", ACCENT), ("pressed", BG_MAIN)],
            foreground=[("active", BG_MAIN), ("pressed", FG_MAIN)]
        )
        
        self.style.configure("Accent.TButton", background=ACCENT, foreground=BG_MAIN, borderwidth=0, font=("Helvetica", 10, "bold"), padding=8)
        self.style.map("Accent.TButton",
            background=[("active", ACCENT_HOVER), ("pressed", BG_MAIN)],
            foreground=[("active", BG_MAIN), ("pressed", FG_MAIN)]
        )
        
        # Checkbutton style
        self.style.configure("TCheckbutton", background=BG_MAIN, foreground=FG_MAIN, font=("Helvetica", 10))
        self.style.map("TCheckbutton",
            background=[("active", BG_MAIN), ("selected", BG_MAIN)],
            foreground=[("active", ACCENT), ("selected", ACCENT)]
        )
        
        # Entry style
        self.style.configure("TEntry", fieldbackground=BG_PANEL, foreground=FG_MAIN, insertcolor=FG_MAIN, bordercolor=BG_PANEL, lightcolor=BG_PANEL, darkcolor=BG_PANEL)

    def create_widgets(self):
        # Main container
        main_frame = ttk.Frame(self, padding=20)
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Header
        header_frame = ttk.Frame(main_frame)
        header_frame.pack(fill=tk.X, pady=(0, 10))
        
        title_label = ttk.Label(header_frame, text="rusEFI Board Compiler & Bundler", style="Title.TLabel")
        title_label.pack(side=tk.LEFT)
        
        # 1. Repo selector section
        repo_frame = ttk.Frame(main_frame)
        repo_frame.pack(fill=tk.X, pady=(0, 15))
        
        repo_grid = ttk.Frame(repo_frame)
        repo_grid.pack(fill=tk.X)
        repo_grid.columnconfigure(1, weight=1)
        
        ttk.Label(repo_grid, text="rusEFI Repo Root: ", font=("Helvetica", 10, "bold")).grid(row=0, column=0, sticky=tk.W, pady=5)
        repo_entry = ttk.Entry(repo_grid, textvariable=self.workspace_root)
        repo_entry.grid(row=0, column=1, sticky=tk.EW, pady=5, padx=5)
        self.workspace_root.trace_add("write", self.on_repo_changed)
        
        repo_browse_btn = ttk.Button(repo_grid, text="Browse...", command=self.browse_repo)
        repo_browse_btn.grid(row=0, column=2, sticky=tk.W, pady=5, padx=(5, 0))
        
        self.repo_status_lbl = ttk.Label(repo_frame, text="Checking...", font=("Helvetica", 9, "bold"))
        self.repo_status_lbl.pack(anchor=tk.W, padx=(115, 0))

        # Horizontal separator
        ttk.Separator(main_frame, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=15)
        
        # Configuration Selection Frame (Grid)
        selection_frame = ttk.Frame(main_frame)
        selection_frame.pack(fill=tk.X, pady=5)
        selection_frame.columnconfigure(0, weight=1)
        selection_frame.columnconfigure(1, weight=1)
        
        # Left Panel: Standard Repo Board selection
        self.std_board_panel = ttk.LabelFrame(selection_frame, text=" Standard Repository Board ", padding=10)
        self.std_board_panel.grid(row=0, column=0, sticky=tk.NSEW, padx=(0, 10))
        
        self.use_std_radio = tk.Checkbutton(
            self.std_board_panel, 
            text="Use standard scanned board", 
            variable=self.use_custom_board, 
            onvalue=False, 
            offvalue=True, 
            command=self.toggle_mode,
            bg=BG_MAIN, 
            fg=FG_MAIN,
            selectcolor=BG_PANEL,
            activebackground=BG_MAIN,
            activeforeground=ACCENT,
            relief="flat",
            borderwidth=0,
            highlightthickness=0,
            font=("Helvetica", 10)
        )
        self.use_std_radio.pack(anchor=tk.W, pady=(0, 10))
        
        self.board_var = tk.StringVar()
        self.board_combo = ttk.Combobox(self.std_board_panel, textvariable=self.board_var, state="normal", width=35)
        self.board_combo.pack(fill=tk.X, pady=5)
        self.board_combo.bind("<<ComboboxSelected>>", self.on_board_selected)
        self.board_combo.bind("<KeyRelease>", self.on_combo_keyrelease)
        self.board_combo.bind("<FocusIn>", self.on_combo_focusin)
        
        # Right Panel: Custom Board selection
        self.custom_board_panel = ttk.LabelFrame(selection_frame, text=" Custom Board Directory ", padding=10)
        self.custom_board_panel.grid(row=0, column=1, sticky=tk.NSEW, padx=(10, 0))
        
        self.use_custom_radio = tk.Checkbutton(
            self.custom_board_panel, 
            text="Use custom board folder", 
            variable=self.use_custom_board, 
            onvalue=True, 
            offvalue=False, 
            command=self.toggle_mode,
            bg=BG_MAIN, 
            fg=FG_MAIN,
            selectcolor=BG_PANEL,
            activebackground=BG_MAIN,
            activeforeground=ACCENT,
            relief="flat",
            borderwidth=0,
            highlightthickness=0,
            font=("Helvetica", 10)
        )
        self.use_custom_radio.pack(anchor=tk.W, pady=(0, 10))
        
        custom_path_frame = ttk.Frame(self.custom_board_panel)
        custom_path_frame.pack(fill=tk.X, pady=5)
        
        self.custom_entry = ttk.Entry(custom_path_frame, textvariable=self.custom_board_dir)
        self.custom_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        self.custom_board_dir.trace_add("write", self.on_custom_dir_changed)
        
        self.custom_browse_btn = ttk.Button(custom_path_frame, text="Browse...", command=self.browse_custom_board)
        self.custom_browse_btn.pack(side=tk.RIGHT)
        
        # Custom config files dropdown
        self.custom_env_var = tk.StringVar()
        self.custom_env_combo = ttk.Combobox(self.custom_board_panel, textvariable=self.custom_env_var, state="readonly")
        self.custom_env_combo.pack(fill=tk.X, pady=5)
        
        # Initialize widget states based on default mode
        self.toggle_mode()

        # Destination Panel
        dest_panel = ttk.Frame(main_frame, padding=(0, 15))
        dest_panel.pack(fill=tk.X)
        dest_panel.columnconfigure(1, weight=1)
        
        ttk.Label(dest_panel, text="Output Directory: ", font=("Helvetica", 10, "bold")).grid(row=0, column=0, sticky=tk.W, pady=5)
        self.dest_entry = ttk.Entry(dest_panel, textvariable=self.dest_dir)
        self.dest_entry.grid(row=0, column=1, sticky=tk.EW, pady=5, padx=5)
        
        browse_dest_btn = ttk.Button(dest_panel, text="Browse...", command=self.browse_dest)
        browse_dest_btn.grid(row=0, column=2, sticky=tk.W, pady=5, padx=(5, 5))
        
        new_folder_btn = ttk.Button(dest_panel, text="New Folder...", command=self.create_new_folder)
        new_folder_btn.grid(row=0, column=3, sticky=tk.W, pady=5)
        
        extract_chk = tk.Checkbutton(
            dest_panel,
            text="Extract zip bundle contents to target folder",
            variable=self.extract_zip,
            command=self._on_extract_zip_changed,
            bg=BG_MAIN,
            fg=FG_MAIN,
            selectcolor=BG_PANEL,
            activebackground=BG_MAIN,
            activeforeground=ACCENT,
            relief="flat",
            borderwidth=0,
            highlightthickness=0,
            font=("Helvetica", 10)
        )
        extract_chk.grid(row=1, column=1, columnspan=3, sticky=tk.W, pady=5, padx=5)

        self.delete_zip_chk = tk.Checkbutton(
            dest_panel,
            text="Delete zip file after extraction",
            variable=self.delete_zip,
            bg=BG_MAIN,
            fg=FG_MAIN,
            selectcolor=BG_PANEL,
            activebackground=BG_MAIN,
            activeforeground=ACCENT,
            relief="flat",
            borderwidth=0,
            highlightthickness=0,
            font=("Helvetica", 10)
        )
        self.delete_zip_chk.grid(row=2, column=1, columnspan=3, sticky=tk.W, pady=(0, 5), padx=25)
        self._on_extract_zip_changed()

        clean_chk = tk.Checkbutton(
            dest_panel, 
            text="Clean build folder (make clean) before compiling", 
            variable=self.clean_build,
            bg=BG_MAIN, 
            fg=FG_MAIN,
            selectcolor=BG_PANEL,
            activebackground=BG_MAIN,
            activeforeground=ACCENT,
            relief="flat",
            borderwidth=0,
            highlightthickness=0,
            font=("Helvetica", 10)
        )
        clean_chk.grid(row=3, column=1, columnspan=3, sticky=tk.W, pady=5, padx=5)
        
        # Action Buttons Frame
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X, pady=(10, 5))
        
        self.build_btn = ttk.Button(btn_frame, text="Compile Board & Build Console (.zip)", style="Accent.TButton", command=self.start_build)
        self.build_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        self.cancel_btn = ttk.Button(btn_frame, text="Cancel Build", state=tk.DISABLED, command=self.cancel_build)
        self.cancel_btn.pack(side=tk.LEFT)
        
        # Log Header
        log_label_frame = ttk.Frame(main_frame)
        log_label_frame.pack(fill=tk.X, pady=(15, 5))
        
        ttk.Label(log_label_frame, text="Build Output Logs", font=("Helvetica", 10, "bold")).pack(side=tk.LEFT)
        
        self.status_var = tk.StringVar(value="Idle")
        self.status_lbl = ttk.Label(log_label_frame, textvariable=self.status_var, font=("Helvetica", 9, "bold"), foreground=HIGHLIGHT)
        self.status_lbl.pack(side=tk.RIGHT)
        
        # Log Text area
        self.log_text = tk.Text(main_frame, bg=BG_PANEL, fg=FG_MAIN, insertbackground=FG_MAIN, relief="flat", font=("Courier New", 9))
        self.log_text.pack(fill=tk.BOTH, expand=True)
        
        # Scrollbar for text
        scrollbar = ttk.Scrollbar(self.log_text, command=self.log_text.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.log_text.configure(yscrollcommand=scrollbar.set)
        
        # Set log tags for colors
        self.log_text.tag_config("stdout", foreground=FG_MAIN)
        self.log_text.tag_config("stderr", foreground=ERROR)
        self.log_text.tag_config("info", foreground=HIGHLIGHT)
        self.log_text.tag_config("success", foreground=SUCCESS)

    def _on_extract_zip_changed(self):
        if self.extract_zip.get():
            self.delete_zip_chk.configure(state="normal")
        else:
            self.delete_zip.set(False)
            self.delete_zip_chk.configure(state="disabled")

    def toggle_mode(self):
        use_custom = self.use_custom_board.get()
        
        if use_custom:
            self.board_combo.configure(state="disabled")
            self.custom_entry.configure(state="normal")
            self.custom_browse_btn.configure(state="normal")
            self.custom_env_combo.configure(state="readonly" if self.custom_env_files else "disabled")
        else:
            self.board_combo.configure(state="normal" if self.boards_list else "disabled")
            self.custom_entry.configure(state="disabled")
            self.custom_browse_btn.configure(state="disabled")
            self.custom_env_combo.configure(state="disabled")

    def scan_boards(self):
        if not self.boards_dir or not os.path.exists(self.boards_dir):
            return
            
        self.append_log("[INFO] Scanning standard repository boards...\n", "info")
        self.boards_list = []
        
        for root, dirs, files in os.walk(self.boards_dir):
            for file in files:
                if file.startswith("meta-info") and file.endswith(".env"):
                    full_path = os.path.join(root, file)
                    rel_path = os.path.relpath(full_path, self.firmware_dir)
                    board_dir_name = os.path.basename(root)
                    
                    if file == "meta-info.env":
                        variant = "default"
                    else:
                        variant = file[len("meta-info-"):-len(".env")]
                        
                    display_name = f"{board_dir_name} ({variant})"
                    
                    # Parse SHORT_BOARD_NAME
                    short_board_name = board_dir_name
                    try:
                        with open(full_path, "r") as f:
                            for line in f:
                                if line.strip().startswith("SHORT_BOARD_NAME="):
                                    short_board_name = line.strip().split("=")[1].strip()
                                    break
                    except Exception:
                        pass
                        
                    self.boards_list.append({
                        "display_name": display_name,
                        "rel_path": rel_path,
                        "board_dir": board_dir_name,
                        "variant": variant,
                        "short_board_name": short_board_name,
                        "full_path": full_path
                    })
                    
        self.boards_list.sort(key=lambda x: x["display_name"])
        
        # Populate combobox
        combobox_values = [b["display_name"] for b in self.boards_list]
        self.board_combo["values"] = combobox_values
        
        if combobox_values:
            self.board_combo.current(0)
            self.on_board_selected(None)
            self.board_combo.configure(state="normal" if not self.use_custom_board.get() else "disabled")
            self.append_log(f"[SUCCESS] Scanned {len(combobox_values)} board configurations in repository.\n\n", "success")
        else:
            self.board_combo.configure(state="disabled")

    def on_board_selected(self, event):
        val = self.board_var.get()
        for b in self.boards_list:
            if b["display_name"] == val:
                self.selected_board = b
                break

    def on_combo_keyrelease(self, event):
        if event.keysym in ("Up", "Down", "Left", "Right", "Return", "Escape", "Tab", "Shift_L", "Shift_R", "Control_L", "Control_R"):
            return
            
        value = self.board_var.get()
        
        # Filter the list
        if not value:
            filtered_values = [b["display_name"] for b in self.boards_list]
        else:
            filtered_values = [
                b["display_name"] for b in self.boards_list 
                if value.lower() in b["display_name"].lower()
            ]
            
        self.board_combo["values"] = filtered_values
        
        # Try to show the dropdown
        try:
            if self.focus_get() == self.board_combo and filtered_values:
                self.board_combo.tk.call('ttk::combobox::Post', self.board_combo)
        except Exception:
            pass

    def on_combo_focusin(self, event):
        # Restore full list of values when user focuses the combobox
        self.board_combo["values"] = [b["display_name"] for b in self.boards_list]

    def browse_custom_board(self):
        folder = filedialog.askdirectory(title="Select Custom Board Directory", initialdir=self.workspace_root.get() or os.path.expanduser("~"))
        if folder:
            self.custom_board_dir.set(folder)
            self.on_custom_dir_changed()

    def on_custom_dir_changed(self, *args):
        path = self.custom_board_dir.get().strip()
        if not path or not os.path.exists(path):
            self.custom_env_files = []
            self.custom_env_combo["values"] = []
            self.custom_env_combo.configure(state="disabled")
            return
            
        # Scan files in directory for meta-info*.env
        self.custom_env_files = [f for f in os.listdir(path) if f.startswith("meta-info") and f.endswith(".env")]
        self.custom_env_files.sort()
        
        self.custom_env_combo["values"] = self.custom_env_files
        
        if self.custom_env_files:
            self.custom_env_combo.current(0)
            self.custom_env_combo.configure(state="readonly" if self.use_custom_board.get() else "disabled")
            self.append_log(f"[INFO] Found {len(self.custom_env_files)} configuration file(s) in custom folder.\n", "info")
        else:
            self.custom_env_combo.configure(state="disabled")
            self.append_log(f"[WARNING] No meta-info*.env files found in {path}\n", "stderr")

    def browse_dest(self):
        folder = filedialog.askdirectory(initialdir=self.dest_dir.get() or self.workspace_root.get() or os.path.expanduser("~"))
        if folder:
            self.dest_dir.set(folder)
            
    def create_new_folder(self):
        current_dest = self.dest_dir.get().strip()
        if not current_dest:
            current_dest = self.workspace_root.get() or os.path.expanduser("~")
            
        name = simpledialog.askstring("Create New Folder", f"Enter name of new folder to create in:\n{current_dest}", parent=self)
        if name:
            new_path = os.path.join(current_dest, name.strip())
            try:
                os.makedirs(new_path, exist_ok=True)
                self.dest_dir.set(new_path)
                self.append_log(f"[INFO] Created new folder: {new_path}\n", "info")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to create folder:\n{str(e)}")
            
    def process_queue(self):
        while True:
            try:
                callback = self.queue.get_nowait()
            except queue.Empty:
                break
            try:
                callback()
            except Exception as e:
                print(f"Error in queue callback: {e}", file=sys.stderr)
        self.after(100, self.process_queue)

    def append_log(self, text, tag="stdout"):
        if threading.current_thread() is threading.main_thread():
            self._append_log_main(text, tag)
        else:
            self.queue.put(lambda: self._append_log_main(text, tag))

    def _append_log_main(self, text, tag):
        self.log_text.insert(tk.END, text, tag)
        self.log_text.see(tk.END)
        
    def start_build(self):
        # Validation checks
        repo = self.workspace_root.get().strip()
        if not repo or not os.path.exists(os.path.join(repo, "firmware", "bin", "compile.sh")):
            messagebox.showerror("Error", "Please select a valid rusEFI repository root directory containing 'firmware/'")
            return
            
        dest = self.dest_dir.get().strip()
        if not dest:
            messagebox.showwarning("Warning", "Please specify a destination output directory.")
            return

        build_meta_info_path = ""
        board_dir_path = ""
        short_name = ""
        display_name = ""

        if self.use_custom_board.get():
            custom_dir = self.custom_board_dir.get().strip()
            if not custom_dir or not os.path.exists(custom_dir):
                messagebox.showerror("Error", "Please select a valid custom board directory.")
                return
            env_file = self.custom_env_var.get().strip()
            if not env_file:
                messagebox.showerror("Error", "No meta-info*.env file selected.")
                return
            
            build_meta_info_path = os.path.join(custom_dir, env_file)
            board_dir_path = custom_dir
            display_name = f"Custom: {os.path.basename(custom_dir)} ({env_file})"
            
            # Parse SHORT_BOARD_NAME from the custom file
            short_name = os.path.basename(custom_dir)
            try:
                with open(build_meta_info_path, "r") as f:
                    for line in f:
                        if line.strip().startswith("SHORT_BOARD_NAME="):
                            short_name = line.strip().split("=")[1].strip()
                            break
            except Exception as e:
                self.append_log(f"[WARNING] Failed to parse SHORT_BOARD_NAME: {str(e)}\n", "stderr")
        else:
            board_name = self.board_var.get().strip()
            self.selected_board = None
            for b in self.boards_list:
                if b["display_name"] == board_name:
                    self.selected_board = b
                    break
            
            if not self.selected_board:
                messagebox.showerror("Error", "Invalid target board. Please select a valid board from the list.")
                return
            build_meta_info_path = self.selected_board["full_path"]
            board_dir_path = os.path.join(self.boards_dir, self.selected_board["board_dir"])
            short_name = self.selected_board["short_board_name"]
            display_name = self.selected_board["display_name"]

        # Lock UI
        self.build_btn.configure(state=tk.DISABLED)
        self.cancel_btn.configure(state=tk.NORMAL)
        self.status_var.set("Building...")
        self.status_lbl.configure(foreground=ACCENT)
        self.stop_build_flag = False
        
        self.log_text.delete("1.0", tk.END)
        self.append_log(f"--- Starting Build for {display_name} ---\n", "info")
        self.append_log(f"Meta-info file: {build_meta_info_path}\n", "info")
        self.append_log(f"Short Board Name: {short_name}\n", "info")
        self.append_log(f"Destination: {dest}\n\n", "info")
        
        clean_build = self.clean_build.get()
        extract_zip = self.extract_zip.get()
        delete_zip = self.delete_zip.get()
        workspace_root = self.workspace_root.get()
        self.build_thread = threading.Thread(
            target=self.build_worker,
            args=(build_meta_info_path, board_dir_path, short_name, dest, clean_build, extract_zip, delete_zip, workspace_root)
        )
        self.build_thread.start()

    def cancel_build(self):
        if self.process:
            self.stop_build_flag = True
            self.append_log("\n[INFO] Cancelling build, killing process...\n", "stderr")
            try:
                self.process.terminate()
            except Exception:
                pass
                
    def build_worker(self, meta_info_file, board_dir, short_name, dest, clean_build, extract_zip, delete_zip, workspace_root):
        try:
            # Clean the deliver directory to prevent packing stale .srec files with different signature hashes
            deliver_dir = os.path.join(self.firmware_dir, "deliver")
            if os.path.exists(deliver_dir):
                self.append_log("[INFO] Cleaning stale outputs in deliver directory...\n", "info")
                try:
                    shutil.rmtree(deliver_dir)
                    os.makedirs(deliver_dir, exist_ok=True)
                except Exception as e:
                    self.append_log(f"[WARNING] Failed to clean deliver directory: {str(e)}\n", "stderr")

            # Automatically update the date stamp to the current date before compiling
            try:
                date_stamp_path = os.path.join(self.firmware_dir, "controllers", "date_stamp.h")
                current_date = datetime.now().strftime("%Y%m%d")
                self.append_log(f"[INFO] Automatically updating build date stamp in date_stamp.h to {current_date}...\n", "info")
                with open(date_stamp_path, "w") as f:
                    f.write(f"#pragma once\n#define VCS_DATE {current_date}\n")
                
                # Touch engine_controller.cpp and efi_gpio.cpp to force them to recompile with new dates/configs
                engine_controller_path = os.path.join(self.firmware_dir, "controllers", "engine_controller.cpp")
                if os.path.exists(engine_controller_path):
                    os.utime(engine_controller_path, None)
                    self.append_log("[INFO] Touched engine_controller.cpp to force recompilation.\n", "info")
                
                efi_gpio_path = os.path.join(self.firmware_dir, "controllers", "system", "efi_gpio.cpp")
                if os.path.exists(efi_gpio_path):
                    os.utime(efi_gpio_path, None)
                    self.append_log("[INFO] Touched efi_gpio.cpp to force recompilation.\n", "info")
            except Exception as e:
                self.append_log(f"[WARNING] Failed to automatically update date_stamp.h or touch source files: {str(e)}\n", "stderr")

            # Get paths relative to firmware directory
            rel_meta_info = os.path.relpath(meta_info_file, self.firmware_dir)
            rel_board_dir = os.path.relpath(board_dir, self.firmware_dir)

            # Set up environment variables, ensuring BUNDLE_SIMULATOR=false
            # to prevent trying to compile Windows simulator on Linux/macOS
            build_env = {**os.environ, "BUNDLE_SIMULATOR": "false"}

            # 1. Generate signature
            self.append_log("[STEP 1/3] Generating TunerStudio signatures...\n", "info")
            sig_cmd = ["bash", "gen_signature.sh", short_name]
            self.append_log(f"Running: {' '.join(sig_cmd)}\n", "info")
            
            sig_process = subprocess.Popen(
                sig_cmd,
                cwd=self.firmware_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                env=build_env
            )
            
            while True:
                line = sig_process.stdout.readline()
                if not line:
                    break
                self.append_log(line)
                
            sig_process.wait()
            if sig_process.returncode != 0:
                self.append_log(f"\n[ERROR] Signature generation failed with code {sig_process.returncode}\n", "stderr")
                self.build_failed()
                return

            # 2. Generate configuration headers
            self.append_log("\n[STEP 2/3] Generating board config headers...\n", "info")
            gen_cmd = ["bash", "gen_config_board.sh", rel_board_dir, short_name]
            self.append_log(f"Running: {' '.join(gen_cmd)}\n", "info")
            
            gen_process = subprocess.Popen(
                gen_cmd,
                cwd=self.firmware_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                env=build_env
            )
            
            while True:
                line = gen_process.stdout.readline()
                if not line:
                    break
                self.append_log(line)
                
            gen_process.wait()
            if gen_process.returncode != 0:
                self.append_log(f"\n[ERROR] Config generation failed with code {gen_process.returncode}\n", "stderr")
                self.build_failed()
                return

            # 2.5 Clean build folder if requested
            if clean_build:
                self.append_log("\n[INFO] Cleaning build folder (make clean)...\n", "info")
                clean_cmd = ["make", "clean"]
                self.append_log(f"Running: {' '.join(clean_cmd)}\n", "info")
                
                self.process = subprocess.Popen(
                    clean_cmd,
                    cwd=self.firmware_dir,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    env=build_env
                )
                
                while True:
                    line = self.process.stdout.readline()
                    if not line:
                        break
                    self.append_log(line)
                    
                self.process.wait()
                clean_code = self.process.returncode
                self.process = None
                
                if self.stop_build_flag:
                    self.append_log("\n[INFO] Build cancelled by user.\n", "stderr")
                    self.build_failed("Cancelled")
                    return
                    
                if clean_code != 0:
                    self.append_log(f"\n[WARNING] Make clean failed with code {clean_code}. Proceeding with build anyway...\n", "stderr")

            # 3. Main bundle compilation
            self.append_log("\n[STEP 3/3] Compiling firmware & building console bundle...\n", "info")
            # We only build the Linux console bundle; skip the Windows simulator
            # (.exe), which would require the i686-w64-mingw32 cross-compiler.
            cmd = ["bash", "bin/compile.sh", "-b", rel_meta_info, "BUNDLE_SIMULATOR=false"]
            self.append_log(f"Running: {' '.join(cmd)}\n", "info")
            
            self.process = subprocess.Popen(
                cmd,
                cwd=self.firmware_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                env=build_env
            )
            
            while True:
                line = self.process.stdout.readline()
                if not line:
                    break
                self.append_log(line)
                
            self.process.wait()
            ret_code = self.process.returncode
            self.process = None
            
            if self.stop_build_flag:
                self.append_log("\n[INFO] Build cancelled by user.\n", "stderr")
                self.build_failed("Cancelled")
                return
                
            if ret_code != 0:
                self.append_log(f"\n[ERROR] Build failed with exit code {ret_code}\n", "stderr")
                self.build_failed()
                return
                
            # 4. Copy results to destination
            self.append_log("\n--- Assembling Package Results ---\n", "info")
            artifacts_dir = os.path.join(workspace_root, "artifacts")
            zip_filename = f"rusefi_bundle_{short_name}.zip"
            src_zip = os.path.join(artifacts_dir, zip_filename)
            
            if not os.path.exists(src_zip):
                self.append_log(f"[WARNING] Could not find expected bundle zip at {src_zip}\n", "stderr")
                self.append_log("Searching artifacts directory...\n", "info")
                zips = [f for f in os.listdir(artifacts_dir) if f.endswith(".zip") and short_name in f]
                if zips:
                    src_zip = os.path.join(artifacts_dir, zips[0])
                    zip_filename = zips[0]
                    self.append_log(f"Found alternative zip: {zip_filename}\n", "info")
                else:
                    self.append_log("[ERROR] No bundle zip file found in artifacts directory!\n", "stderr")
                    self.build_failed()
                    return
            
            # Make sure destination folder exists
            os.makedirs(dest, exist_ok=True)
            
            # Copy ZIP file
            dest_zip = os.path.join(dest, zip_filename)
            if os.path.exists(dest_zip):
                self.append_log(f"Removing existing bundle ZIP: {dest_zip}\n", "info")
                if os.path.isdir(dest_zip):
                    shutil.rmtree(dest_zip)
                else:
                    os.remove(dest_zip)
            self.append_log(f"Copying bundle ZIP to: {dest_zip}\n", "info")
            shutil.copy2(src_zip, dest_zip)
            
            # Extract ZIP if checked
            if extract_zip:
                extract_path = os.path.join(dest, f"rusefi_bundle_{short_name}")
                self.append_log(f"Extracting package to: {extract_path}\n", "info")
                if os.path.exists(extract_path):
                    self.append_log(f"Removing existing extraction folder: {extract_path}\n", "info")
                    if os.path.isdir(extract_path):
                        shutil.rmtree(extract_path)
                    else:
                        os.remove(extract_path)
                os.makedirs(extract_path, exist_ok=True)
                
                with zipfile.ZipFile(dest_zip, 'r') as zip_ref:
                    zip_ref.extractall(extract_path)

                if delete_zip:
                    self.append_log(f"Deleting zip file: {dest_zip}\n", "info")
                    os.remove(dest_zip)

            # Copy the canonical OpenBLT update .srec directly from the build directory.
            # The bundle's own .srec embeds a SIGNATURE_HASH read via a recursively-expanded
            # $(shell) in bundle.mk; because the signature header is regenerated mid-build, that
            # hash can change between when the .srec is written and when it is zipped, so the srec
            # is frequently missing (or stale) inside rusefi_bundle_*.zip. build/rusefi.srec is the
            # CRC'd image OpenBLT actually needs and is unaffected by the filename-hash race.
            self.append_log("\n--- Copying OpenBLT update image (.srec) ---\n", "info")
            build_srec = os.path.join(self.firmware_dir, "build", "rusefi.srec")
            if os.path.exists(build_srec):
                if extract_zip:
                    updater_sh = None
                    for _root, _dirs, _files in os.walk(extract_path):
                        if "rusefi_updater.sh" in _files:
                            updater_sh = os.path.join(_root, "rusefi_updater.sh")
                            break
                    if updater_sh:
                        srec_dest = os.path.join(os.path.dirname(updater_sh), "rusefi_update.srec")
                    else:
                        self.append_log("[WARNING] rusefi_updater.sh not found in extracted bundle; placing srec at bundle root.\n", "stderr")
                        srec_dest = os.path.join(extract_path, "rusefi_update.srec")
                    self.append_log(f"Copying {build_srec}\n  -> {srec_dest}\n", "info")
                    shutil.copy2(build_srec, srec_dest)
                else:
                    self.append_log("[INFO] Zip not extracted; skipping srec placement.\n", "info")
            else:
                self.append_log(f"[WARNING] Expected srec not found at {build_srec}; OpenBLT image not copied.\n", "stderr")

            # Generate double-clickable launcher scripts inside the extracted bundle folder.
            # The .sh uses $(dirname "$0") so it works even if the folder is moved.
            # The .desktop file gives GNOME/KDE/etc a proper double-click entry pointing to the .sh.
            if extract_zip:
                self.append_log("\n--- Generating Launcher Scripts ---\n", "info")
                try:
                    launcher_sh = os.path.join(extract_path, "update_firmware.sh")
                    with open(launcher_sh, "w") as lf:
                        lf.write('#!/bin/bash\ncd "$(dirname "$0")"\nbash rusefi_updater.sh\n')
                    os.chmod(launcher_sh, 0o755)
                    self.append_log(f"Created launcher: {launcher_sh}\n", "info")

                    desktop_path = os.path.join(extract_path, "update_firmware.desktop")
                    with open(desktop_path, "w") as df:
                        df.write(
                            "[Desktop Entry]\n"
                            "Type=Application\n"
                            f"Name=Flash {short_name} Firmware\n"
                            "Comment=Flash rusEFI firmware via OpenBLT\n"
                            f'Exec=bash "{launcher_sh}"\n'
                            "Terminal=true\n"
                            "StartupNotify=false\n"
                        )
                    os.chmod(desktop_path, 0o755)
                    self.append_log(f"Created desktop launcher: {desktop_path}\n", "info")
                except Exception as e:
                    self.append_log(f"[WARNING] Failed to create launcher scripts: {str(e)}\n", "stderr")

            self.append_log("\n[SUCCESS] Build and bundle packaging completed successfully!\n", "success")
            self.build_success()
            
        except Exception as e:
            self.append_log(f"\n[FATAL ERROR] An unexpected exception occurred: {str(e)}\n", "stderr")
            self.build_failed()
        finally:
            self.build_complete()

    def load_config(self):
        try:
            if os.path.exists(CONFIG_FILE):
                with open(CONFIG_FILE, "r") as f:
                    cfg = json.load(f)
                saved = cfg.get("dest_dir", "")
                if saved:
                    self.dest_dir.set(saved)
        except Exception:
            pass

    def save_config(self, *args):
        try:
            with open(CONFIG_FILE, "w") as f:
                json.dump({"dest_dir": self.dest_dir.get()}, f)
        except Exception:
            pass

    def build_failed(self, status="Failed"):
        if threading.current_thread() is threading.main_thread():
            self._build_failed_main(status)
        else:
            self.queue.put(lambda: self._build_failed_main(status))

    def _build_failed_main(self, status):
        self.status_var.set(status)
        self.status_lbl.configure(foreground=ERROR)
        self.build_btn.configure(state=tk.NORMAL)
        self.cancel_btn.configure(state=tk.DISABLED)

    def build_success(self):
        if threading.current_thread() is threading.main_thread():
            self._build_success_main()
        else:
            self.queue.put(lambda: self._build_success_main())

    def _build_success_main(self):
        self.status_var.set("Success")
        self.status_lbl.configure(foreground=SUCCESS)

    def build_complete(self):
        if threading.current_thread() is threading.main_thread():
            self._build_complete_main()
        else:
            self.queue.put(lambda: self._build_complete_main())

    def _build_complete_main(self):
        self.build_btn.configure(state=tk.NORMAL)
        self.cancel_btn.configure(state=tk.DISABLED)

if __name__ == "__main__":
    app = App()
    app.mainloop()
