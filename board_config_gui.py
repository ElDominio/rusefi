#!/usr/bin/env python3
import os
import sys
import re
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# Force XWayland mode on Linux/Wayland to fix Tkinter popdown positioning bug (top-left of screen)
if sys.platform.startswith("linux"):
    os.environ["WAYLAND_DISPLAY"] = ""

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

# Flags from FEATURE_FLAGS.md (Curated list with friendly descriptions)
CURATED_FLAGS = [
    ("EFI_DOWNSHIFT_BLIPPER", "Downshift Blipper (needs ETB)"),
    ("EFI_EXHAUST_CUTOUT", "Exhaust Cutout support"),
    ("EFI_ENGINE_STATE_MACHINE", "Engine State Machine + Pops & Bangs"),
    ("EFI_MISFIRE_DETECTION", "Idle Misfire Detection (needs ESM)"),
    ("EFI_CLUTCH_DELAY_VALVE", "Clutch Delay Valve (CDV)"),
    ("EFI_OFF_IDLE_RPM_ADDER", "Off-idle RPM Adder"),
    ("EFI_LUA_LIMITER", "Lua Limiter support"),
    ("EFI_ADVANCED_FUEL_PUMP", "Advanced / PWM Fuel Pump"),
    ("EFI_VVT_COMPENSATION", "VVT Timing/Fuel Compensation"),
]

# Baseline known flags
ALL_KNOWN_FLAGS = [
    "EFI_ACTIVE_CONFIGURATION_IN_FLASH", "EFI_ADVANCED_FUEL_PUMP", "EFI_ALTERNATOR_CONTROL",
    "EFI_ANALOG_SENSORS", "EFI_ANTILAG_SYSTEM", "EFI_BACKUP_SRAM", "EFI_BLUETOOTH_SETUP",
    "EFI_BOOST_CONTROL", "EFI_BOR_LEVEL", "EFI_BOSCH_YAW", "EFI_CAN_GPIO", "EFI_CAN_SERIAL",
    "EFI_CAN_SUPPORT", "EFI_CDM_INTEGRATION", "EFI_CLI_SUPPORT", "EFI_CLOCK_LOCKS",
    "EFI_CLUTCH_DELAY_VALVE", "EFI_CONSOLE_AF", "EFI_CONSOLE_RX_BRAIN_PIN",
    "EFI_CONSOLE_RX_BRAIN_PIN_MODE", "EFI_CONSOLE_TX_BRAIN_PIN", "EFI_CONSOLE_TX_BRAIN_PIN_MODE",
    "EFI_CONSOLE_USB_DEVICE", "EFI_CUSTOM_PANIC_METHOD", "EFI_DAC", "EFI_DETAILED_LOGGING",
    "EFI_DFU_JUMP", "EFI_DOWNSHIFT_BLIPPER", "EFI_DYNO_VIEW", "EFI_ELECTRONIC_THROTTLE_BODY",
    "EFI_EMBED_INI_MSD", "EFI_EMULATE_POSITION_SENSORS", "EFI_ENABLE_ASSERTS",
    "EFI_ENGINE_CONTROL", "EFI_ENGINE_EMULATOR", "EFI_ENGINE_SNIFFER", "EFI_ENGINE_STATE_MACHINE",
    "EFI_ETHERNET", "EFI_EXHAUST_CUTOUT", "EFI_FILE_LOGGING", "EFI_GPIO_HARDWARE",
    "EFI_HD_ACR", "EFI_HELLA_OIL", "EFI_HISTOGRAMS", "EFI_HPFP", "EFI_IDLE_CONTROL",
    "EFI_IDLE_PID_CIC", "EFI_INTERNAL_ADC", "EFI_INTERNAL_FAST_ADC_GPT", "EFI_INTERNAL_FAST_ADC_PWM",
    "EFI_INTERNAL_SLOW_ADC_BACKGROUND", "EFI_LAUNCH_CONTROL", "EFI_LOGIC_ANALYZER",
    "EFI_LTFT_CONTROL", "EFI_LUA", "EFI_LUA_LIMITER", "EFI_LUA_LOOKUP", "EFI_MAIN_RELAY_CONTROL",
    "EFI_MALFUNCTION_INDICATOR", "EFI_MAP_AVERAGING", "EFI_MAX_31855", "EFI_MC33816",
    "EFI_MCP_3208", "EFI_MISFIRE_DETECTION", "EFI_OFF_IDLE_RPM_ADDER", "EFI_ONBOARD_MEMS", "EFI_PERF_METRICS",
    "EFI_POTENTIOMETER", "EFI_RTC", "EFI_SENT_SUPPORT", "EFI_SHAFT_POSITION_INPUT",
    "EFI_SIGNAL_EXECUTOR_ONE_TIMER", "EFI_SIGNAL_EXECUTOR_SLEEP", "EFI_SPI1_AF", "EFI_SPI2_AF",
    "EFI_SPI3_AF", "EFI_SPI4_AF", "EFI_SPI5_AF", "EFI_SPI6_AF", "EFI_STORAGE_INT_FLASH",
    "EFI_STORAGE_MFS", "EFI_STORAGE_SD", "EFI_SUPPORT_FATFS", "EFI_TCU", "EFI_TEXT_LOGGING",
    "EFI_TOOTH_LOGGER", "EFI_TS_SCATTER", "EFI_TUNER_STUDIO", "EFI_TUNER_STUDIO_VERBOSE",
    "EFI_UART_ECHO_TEST_MODE", "EFI_UART_GPS", "EFI_USB_SERIAL", "EFI_USE_COMPRESSED_INI_MSD",
    "EFI_USE_FAST_ADC", "EFI_USE_OPENBLT", "EFI_USE_UART_DMA", "EFI_VEHICLE_SPEED",
    "EFI_VVT_COMPENSATION", "EFI_VVT_PID", "EFI_WIDEBAND_FIRMWARE_UPDATE", "EFI_WIFI",
    "EFI_WS2812", "ROTATIONAL_IDLE_CONTROLLER", "MODULE_CDV_CONTROLLER", "MODULE_CONFIGURATION_WIZARD",
    "MODULE_EXAMPLE_LIVEDATA", "MODULE_FAN_CONTROL", "MODULE_FUEL_PUMP", "MODULE_GEAR_DETECTOR",
    "MODULE_LOGGING", "MODULE_MAP_AVERAGING", "MODULE_ODOMETER", "MODULE_TACHOMETER", "MODULE_VVL_CONTROLLER"
]

FLAG_DESCRIPTIONS = {f[0]: f[1] for f in CURATED_FLAGS}

# Tokens that mark a -D define as a genuine on/off feature flag. Anything else — numeric
# counts/depths (=4, =1, =0), pins (=Gpio::B3), device names (=ADCD1), engine types
# (=engine_type_e::...), quoted strings (=BOARD_SERIAL="..."), or bare defines
# (-DCPU_MKE16F512VLH16) — is a configuration value and must NEVER be rewritten to TRUE/FALSE.
BOOLEAN_FLAG_VALUES = ("TRUE", "FALSE")

def is_boolean_flag_value(value):
    """True only when a -D define's value is an explicit on/off boolean token.

    Bare defines (value is None) and any non-boolean value return False so the editor
    leaves them exactly as-is instead of corrupting them into =TRUE/=FALSE.
    """
    return value is not None and value.strip().upper() in BOOLEAN_FLAG_VALUES

class ScrollableFrame(ttk.Frame):
    def __init__(self, container, *args, **kwargs):
        super().__init__(container, *args, **kwargs)
        self.canvas = tk.Canvas(self, bg=BG_PANEL, highlightthickness=0)
        scrollbar = ttk.Scrollbar(self, orient="vertical", command=self.canvas.yview)
        self.scrollable_frame = ttk.Frame(self.canvas, style="Panel.TFrame")
        self.scrollable_frame.bind("<Configure>", lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all")))
        self.canvas.create_window((0, 0), window=self.scrollable_frame, anchor="nw")
        self.canvas.configure(yscrollcommand=scrollbar.set)
        self.canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

    def yview(self, *args):
        return self.canvas.yview(*args)

    def yview_moveto(self, fraction):
        self.canvas.yview_moveto(fraction)

class BoardConfigApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("rusEFI Board Configuration Editor")
        self.geometry("1000x820")
        self.configure(bg=BG_MAIN)
        self.minsize(900, 700)
        
        self.workspace_root = tk.StringVar()
        self.boards_dir = ""
        self.firmware_dir = ""
        self.boards_list = []
        self.filtered_boards = []
        self.search_var = tk.StringVar()
        self.search_var.trace_add("write", self.on_search_changed)
        self.skip_rate_var = tk.StringVar()
        self.openblt_var = tk.StringVar(value="0")
        self.custom_params_var = tk.StringVar()
        
        self.flag_vars = {f: tk.StringVar(value="none") for f in ALL_KNOWN_FLAGS}
        self.flag_checkboxes = []
        self.show_all_flags = tk.BooleanVar(value=False)
        self.flag_search_var = tk.StringVar()
        self.flag_search_var.trace_add("write", lambda *args: self.refresh_flag_ui())
        
        self.setup_styles()
        self.create_widgets()
        self.detect_default_repo()

    def setup_styles(self):
        self.style = ttk.Style()
        self.style.theme_use("default")
        self.style.configure(".", background=BG_MAIN, foreground=FG_MAIN, fieldbackground=BG_PANEL, insertcolor=FG_MAIN)
        self.style.configure("TLabel", background=BG_MAIN, foreground=FG_MAIN, font=("Helvetica", 10))
        self.style.configure("Title.TLabel", font=("Helvetica", 15, "bold"), foreground=ACCENT)
        self.style.configure("Sub.TLabel", font=("Helvetica", 10, "bold"), foreground=HIGHLIGHT)
        self.style.configure("Muted.TLabel", foreground=FG_MUTED, font=("Helvetica", 9))
        self.style.configure("TFrame", background=BG_MAIN)
        self.style.configure("Panel.TFrame", background=BG_PANEL, relief="flat")
        self.style.configure("TLabelframe", background=BG_MAIN, foreground=ACCENT)
        self.style.configure("TLabelframe.Label", background=BG_MAIN, foreground=ACCENT, font=("Helvetica", 10, "bold"))
        self.style.configure("TButton", background=BG_PANEL, foreground=FG_MAIN, font=("Helvetica", 9, "bold"), padding=5)
        self.style.map("TButton", background=[("active", ACCENT)], foreground=[("active", BG_MAIN)])
        self.style.configure("Accent.TButton", background=ACCENT, foreground=BG_MAIN, font=("Helvetica", 10, "bold"), padding=8)
        self.style.map("Accent.TButton", background=[("active", ACCENT_HOVER)])

    def create_widgets(self):
        main_frame = ttk.Frame(self, padding=15)
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Repo Selector
        repo_frame = ttk.Frame(main_frame)
        repo_frame.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(repo_frame, text="rusEFI Board Configuration Editor", style="Title.TLabel").pack(anchor=tk.W)
        repo_grid = ttk.Frame(repo_frame)
        repo_grid.pack(fill=tk.X)
        ttk.Label(repo_grid, text="Repo Root: ", font=("Helvetica", 10, "bold")).grid(row=0, column=0, sticky=tk.W)
        ttk.Entry(repo_grid, textvariable=self.workspace_root).grid(row=0, column=1, sticky=tk.EW, padx=5)
        ttk.Button(repo_grid, text="Browse...", command=self.browse_repo).grid(row=0, column=2)
        repo_grid.columnconfigure(1, weight=1)
        self.repo_status_lbl = ttk.Label(repo_frame, text="Checking...", font=("Helvetica", 9, "bold"))
        self.repo_status_lbl.pack(anchor=tk.W, padx=(80, 0))
        self.workspace_root.trace_add("write", self.on_repo_changed)

        paned = tk.PanedWindow(main_frame, orient=tk.HORIZONTAL, bg=BG_MAIN, bd=0, sashwidth=6)
        paned.pack(fill=tk.BOTH, expand=True)

        # Left Panel
        left_panel = ttk.Frame(paned, padding=(0, 0, 10, 0))
        paned.add(left_panel, width=320)
        ttk.Label(left_panel, text="Boards Scanned", style="Sub.TLabel").pack(anchor=tk.W)
        
        search_frame = ttk.Frame(left_panel)
        search_frame.pack(fill=tk.X, pady=5)
        ttk.Label(search_frame, text="🔍").pack(side=tk.LEFT)
        ttk.Entry(search_frame, textvariable=self.search_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)

        list_ctrls = ttk.Frame(left_panel, padding=(0, 5, 0, 0))
        list_ctrls.pack(fill=tk.X, side=tk.BOTTOM)
        ttk.Button(list_ctrls, text="Select All", command=self.select_all_boards).pack(side=tk.LEFT, padx=2)
        ttk.Button(list_ctrls, text="Refresh", command=self.refresh_data).pack(side=tk.LEFT, padx=2)
        self.selection_status_lbl = ttk.Label(list_ctrls, text="Selected: 0", font=("Helvetica", 9))
        self.selection_status_lbl.pack(side=tk.RIGHT)

        list_frame = ttk.Frame(left_panel)
        list_frame.pack(fill=tk.BOTH, expand=True)
        self.board_listbox = tk.Listbox(list_frame, selectmode=tk.MULTIPLE, bg=BG_PANEL, fg=FG_MAIN, selectbackground=ACCENT, relief="flat", highlightthickness=0)
        self.board_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        sb = ttk.Scrollbar(list_frame, command=self.board_listbox.yview)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        self.board_listbox.config(yscrollcommand=sb.set)
        self.board_listbox.bind("<<ListboxSelect>>", self.on_board_select)

        # Right Panel
        self.right_panel = ttk.LabelFrame(paned, text=" Board Configuration Editor ", padding=10)
        paned.add(self.right_panel)
        self.board_details_lbl = ttk.Label(self.right_panel, text="Select a board to edit.", font=("Helvetica", 10, "bold"))
        self.board_details_lbl.pack(anchor=tk.W, pady=(0, 10))

        actions_frame = ttk.Frame(self.right_panel)
        actions_frame.pack(fill=tk.X, side=tk.BOTTOM, pady=5)
        self.save_env_btn = ttk.Button(actions_frame, text="Save to .env", command=self.save_selected_boards)
        self.save_env_btn.pack(side=tk.RIGHT, padx=5)
        self.save_mk_btn = ttk.Button(actions_frame, text="Save to board.mk", style="Accent.TButton", command=self.save_selected_boards_to_mk)
        self.save_mk_btn.pack(side=tk.RIGHT, padx=5)
        self.refresh_btn_right = ttk.Button(actions_frame, text="Refresh Flags", command=self.refresh_data)
        self.refresh_btn_right.pack(side=tk.LEFT)

        self.edit_container = ttk.Frame(self.right_panel)
        self.edit_container.pack(fill=tk.BOTH, expand=True)

        top_settings = ttk.Frame(self.edit_container)
        top_settings.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(top_settings, text="Skip Rate: ", font=("Helvetica", 10, "bold")).grid(row=0, column=0, sticky=tk.W)
        ttk.Spinbox(top_settings, from_=0, to=100, textvariable=self.skip_rate_var, width=5).grid(row=0, column=1, padx=5)
        self.openblt_chk = tk.Checkbutton(top_settings, text="Use OpenBLT", variable=self.openblt_var, onvalue="1", offvalue="0", bg=BG_MAIN, fg=FG_MAIN, selectcolor=BG_PANEL)
        self.openblt_chk.grid(row=0, column=2, padx=10)

        flags_label_frame = ttk.LabelFrame(self.edit_container, text=" Feature Compiler Flags ", padding=5)
        flags_label_frame.pack(fill=tk.BOTH, expand=True)
        
        flags_header = ttk.Frame(flags_label_frame)
        flags_header.pack(fill=tk.X, pady=5)
        ttk.Label(flags_header, text="🔍").pack(side=tk.LEFT)
        ttk.Entry(flags_header, textvariable=self.flag_search_var, width=15).pack(side=tk.LEFT, padx=5)
        tk.Checkbutton(flags_header, text="Show All Available Flags", variable=self.show_all_flags, command=self.refresh_flag_ui, bg=BG_MAIN, fg=ACCENT, selectcolor=BG_PANEL, font=("Helvetica", 9, "bold")).pack(side=tk.RIGHT)

        self.flags_scrollable = ScrollableFrame(flags_label_frame)
        self.flags_scrollable.pack(fill=tk.BOTH, expand=True)

        custom_params_frame = ttk.Frame(self.edit_container)
        custom_params_frame.pack(fill=tk.X, pady=10)
        ttk.Label(custom_params_frame, text="Other Compiler Params:", font=("Helvetica", 10, "bold")).pack(anchor=tk.W)
        ttk.Entry(custom_params_frame, textvariable=self.custom_params_var).pack(fill=tk.X)

        log_frame = ttk.Frame(main_frame)
        log_frame.pack(fill=tk.X, side=tk.BOTTOM, pady=(10, 0))
        
        log_header = ttk.Frame(log_frame)
        log_header.pack(fill=tk.X, pady=(0, 3))
        ttk.Label(log_header, text="Configuration Update Logs", font=("Helvetica", 10, "bold")).pack(side=tk.LEFT)
        self.status_var = tk.StringVar(value="Idle")
        self.status_lbl = ttk.Label(log_header, textvariable=self.status_var, font=("Helvetica", 9, "bold"), foreground=HIGHLIGHT)
        self.status_lbl.pack(side=tk.RIGHT)
        
        self.log_text = tk.Text(log_frame, height=4, bg=BG_PANEL, fg=FG_MAIN, font=("Courier New", 9))
        self.log_text.pack(fill=tk.X)
        self.log_text.tag_config("stderr", foreground=ERROR)
        self.log_text.tag_config("success", foreground=SUCCESS)
        self.log_text.tag_config("info", foreground=HIGHLIGHT)

        self.disable_edit_panel()

    def cycle_flag_state(self, flag_name):
        var = self.flag_vars[flag_name]
        curr = var.get()
        if curr in ("none", "mixed"): var.set("0")
        elif curr == "0": var.set("1")
        else: var.set("none")
        self.refresh_flag_ui(preserve_scroll=True)

    def refresh_flag_ui(self, preserve_scroll=False):
        scroll_y = self.flags_scrollable.yview() if hasattr(self, 'flags_scrollable') else (0, 0)
        
        for chk in self.flag_checkboxes: chk.destroy()
        self.flag_checkboxes = []
        search = self.flag_search_var.get().upper()
        show_all = self.show_all_flags.get()
        
        curated_keys = {item[0] for item in CURATED_FLAGS}
        visible = []
        for f in ALL_KNOWN_FLAGS:
            val = self.flag_vars[f].get()
            is_active = val in ("0", "1", "mixed")
            is_curated = f in curated_keys
            
            if (not search or search in f) and (show_all or is_active or is_curated):
                visible.append(f)
        
        for idx, f in enumerate(visible):
            var = self.flag_vars[f]
            val = var.get()
            
            if val == "1":
                icon, fg = "☑", SUCCESS
            elif val == "0":
                icon, fg = "☒", ERROR
            elif val == "mixed":
                icon, fg = "◪", HIGHLIGHT
            else:
                icon, fg = "☐", FG_MAIN

            desc = FLAG_DESCRIPTIONS.get(f, "")
            text = f"{icon} {f}" + (f"\n   ({desc})" if desc else "")
            
            btn = tk.Button(
                self.flags_scrollable.scrollable_frame, 
                text=text, 
                command=lambda fn=f: self.cycle_flag_state(fn),
                bg=BG_PANEL, 
                fg=fg, 
                activebackground=BG_MAIN,
                activeforeground=ACCENT,
                bd=0,
                relief="flat",
                justify=tk.LEFT, 
                anchor=tk.W, 
                font=("Helvetica", 9, "bold" if val != "none" else "normal"),
                padx=10,
                pady=2
            )
            btn.grid(row=idx // 2, column=idx % 2, sticky=tk.W + tk.E, padx=5, pady=2)
            self.flag_checkboxes.append(btn)
            
        if preserve_scroll:
            self.flags_scrollable.yview_moveto(scroll_y[0])

    def detect_default_repo(self):
        # 1. Check command line arguments first
        if len(sys.argv) > 1 and os.path.isdir(sys.argv[1]):
            candidate = os.path.abspath(sys.argv[1])
            if os.path.exists(os.path.join(candidate, "firmware", "bin", "compile.sh")):
                self.workspace_root.set(candidate)
                return

        # Helper function to find root climbing up from a starting path
        def find_repo_root(start_path):
            curr = os.path.abspath(start_path)
            while True:
                if os.path.exists(os.path.join(curr, "firmware", "bin", "compile.sh")):
                    return curr
                parent = os.path.dirname(curr)
                if parent == curr:
                    break
                curr = parent
            return None

        # 2. Check current working directory
        detected = find_repo_root(os.getcwd())
        if detected:
            self.workspace_root.set(detected)
            return
            
        # 3. Check script folder location
        detected = find_repo_root(os.path.dirname(os.path.abspath(__file__)))
        if detected:
            self.workspace_root.set(detected)
            return
            
        # 4. Prompt selection if not found
        self.append_log("[WARNING] Could not auto-detect rusEFI repository root. Prompting selection...\n", "stderr")
        messagebox.showwarning(
            "Select rusEFI Root",
            "Could not auto-detect the rusEFI repository root directory.\n\nPlease select it manually in the directory dialog."
        )
        folder = filedialog.askdirectory(title="Select rusEFI Repository Root Folder", initialdir=os.path.expanduser("~"))
        if folder:
            self.workspace_root.set(folder)
        else:
            self.append_log("[WARNING] No rusEFI repository selected. Please use 'Browse...' button to select it.\n", "stderr")
            self.update_repo_status(False)

    def on_repo_changed(self, *args):
        path = self.workspace_root.get().strip()
        if not path:
            self.update_repo_status(False)
            return
            
        f_dir = os.path.join(path, "firmware")
        compile_script = os.path.join(f_dir, "bin", "compile.sh")
        
        if os.path.exists(compile_script):
            self.firmware_dir = f_dir
            self.boards_dir = os.path.join(f_dir, "config", "boards")
            self.update_repo_status(True)
            self.scan_boards()
        else:
            self.update_repo_status(False)
            self.append_log(f"[ERROR] Invalid repository path. Could not find {compile_script}\n", "stderr")

    def update_repo_status(self, is_valid):
        if is_valid:
            self.repo_status_lbl.configure(text="Valid rusEFI Repository", foreground=SUCCESS)
        else:
            self.repo_status_lbl.configure(text="Invalid Repository (Select root containing 'firmware/')", foreground=ERROR)
            self.board_listbox.delete(0, tk.END)
            self.boards_list = []
            self.filtered_boards = []
            self.disable_edit_panel()

    def browse_repo(self):
        folder = filedialog.askdirectory(title="Select rusEFI Repository Root Folder", initialdir=self.workspace_root.get() or os.path.expanduser("~"))
        if folder:
            self.workspace_root.set(folder)

    def scan_boards(self):
        self.boards_list = []
        all_detected_flags = set()
        
        # Walk and collect configs
        for root, _, files in os.walk(self.boards_dir):
            for f in files:
                if f.startswith("meta-info") and f.endswith(".env"):
                    path = os.path.join(root, f)
                    board_name = os.path.basename(root)
                    variant = "default" if f == "meta-info.env" else f[10:-4]
                    
                    cfg = self.parse_env_file(path)
                    all_detected_flags.update(cfg['flags'].keys())
                    
                    self.boards_list.append({
                        "display": f"{board_name} ({variant})", 
                        "path": path, 
                        "dir": root,
                        "config": cfg
                    })
                    
        # Dynamically register newly found compiler flags in the repository
        global ALL_KNOWN_FLAGS
        for flag in all_detected_flags:
            if flag not in self.flag_vars:
                ALL_KNOWN_FLAGS.append(flag)
                self.flag_vars[flag] = tk.StringVar(value="none")
                    
        ALL_KNOWN_FLAGS.sort()
        self.boards_list.sort(key=lambda x: x["display"])
        self.on_search_changed()

    def on_search_changed(self, *args):
        q = self.search_var.get().lower()
        
        # Save selection
        selected_indices = self.board_listbox.curselection()
        selected_paths = []
        for i in selected_indices:
            if i < len(self.filtered_boards):
                selected_paths.append(self.filtered_boards[i]["path"])
                
        self.board_listbox.delete(0, tk.END)
        self.filtered_boards = [b for b in self.boards_list if q in b["display"].lower()]
        for b in self.filtered_boards: 
            self.board_listbox.insert(tk.END, b["display"])
            
        # Restore selections
        for idx, board in enumerate(self.filtered_boards):
            if board["path"] in selected_paths:
                self.board_listbox.select_set(idx)
                
        self.update_selection_count()
        self.load_selected_boards()

    def select_all_boards(self):
        self.board_listbox.select_set(0, tk.END)
        self.update_selection_count()
        self.load_selected_boards()

    def update_selection_count(self):
        sel = self.board_listbox.curselection()
        self.selection_status_lbl.configure(text=f"Selected: {len(sel)} / {len(self.filtered_boards)}")

    def on_board_select(self, event):
        self.update_selection_count()
        sel = self.board_listbox.curselection()
        if sel: 
            self.enable_edit_panel()
            self.load_selected_boards()
        else: 
            self.disable_edit_panel()

    def load_selected_boards(self):
        sel = self.board_listbox.curselection()
        configs = [self.filtered_boards[i]["config"] for i in sel]
        if not configs: return
        
        # Reset all to none first
        for v in self.flag_vars.values(): v.set("none")
        
        if len(configs) == 1:
            cfg = configs[0]
            board = self.filtered_boards[sel[0]]
            self.board_details_lbl.configure(text=f"Editing: {board['display']}\nPath: {board['path']}", font=("Helvetica", 10, "bold"))
            
            self.skip_rate_var.set(str(cfg['SKIP_RATE']))
            self.openblt_var.set("1" if cfg['USE_OPENBLT'].lower() == 'yes' else "0")
            for f, val in cfg['flags'].items():
                if f in self.flag_vars: 
                    self.flag_vars[f].set("1" if val else "0")
            
            custom = []
            for p in cfg['EXTRA_PARAMS'].split():
                if p.startswith('-D'):
                    fk = p[2:].split('=')[0]
                    if fk not in self.flag_vars: custom.append(p)
                else: custom.append(p)
            self.custom_params_var.set(" ".join(custom))
        else:
            self.board_details_lbl.configure(text=f"Bulk Editing {len(configs)} selected boards...", font=("Helvetica", 10, "bold"))
            
            rates = {cfg['SKIP_RATE'] for cfg in configs}
            if len(rates) == 1:
                self.skip_rate_var.set(str(list(rates)[0]))
            else:
                self.skip_rate_var.set("")
                
            openblt_vals = {cfg['USE_OPENBLT'].lower() for cfg in configs}
            if len(openblt_vals) == 1:
                self.openblt_var.set("1" if list(openblt_vals)[0] == 'yes' else "0")
            else:
                self.openblt_var.set("mixed")
                
            for f in ALL_KNOWN_FLAGS:
                vals = set()
                for cfg in configs:
                    val = cfg['flags'].get(f)
                    if val is True: vals.add("1")
                    elif val is False: vals.add("0")
                    else: vals.add("none")
                if len(vals) == 1:
                    self.flag_vars[f].set(list(vals)[0])
                else:
                    self.flag_vars[f].set("mixed")
            self.custom_params_var.set("")
            
        self.refresh_flag_ui()

    def parse_board_mk(self, path, bdir, fdir):
        flags = {}
        if not os.path.exists(path): return flags
        try:
            with open(path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.split('#')[0].strip()
                    if line.startswith('include '):
                        inc = line[8:].strip().replace('$(BOARD_DIR)', bdir).replace('$(PROJECT_DIR)', fdir)
                        if not os.path.isabs(inc): inc = os.path.abspath(os.path.join(os.path.dirname(path), inc))
                        flags.update(self.parse_board_mk(inc, bdir, fdir))
                    for fk, fv in re.findall(r'-D([A-Za-z0-9_]+)(?:=([A-Za-z0-9_"\':\\/]+))?', line):
                        # Only register genuine on/off flags; skip numeric/symbolic/bare defines.
                        if not is_boolean_flag_value(fv):
                            continue
                        flags[fk] = (fv.upper() == 'TRUE')
        except: pass
        return flags

    def parse_env_file(self, path):
        cfg = {'SKIP_RATE': 100, 'USE_OPENBLT': 'no', 'EXTRA_PARAMS': '', 'flags': {}}
        bdir = os.path.dirname(path)
        fdir = self.firmware_dir
        cfg['flags'] = self.parse_board_mk(os.path.join(bdir, "board.mk"), bdir, fdir)
        try:
            with open(path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.split('#')[0].strip()
                    if '=' in line:
                        k, v = line.split('=', 1)
                        if k == 'SKIP_RATE': cfg['SKIP_RATE'] = v
                        elif k == 'USE_OPENBLT': cfg['USE_OPENBLT'] = v
                        elif k == 'EXTRA_PARAMS':
                            cfg['EXTRA_PARAMS'] = v
                            for p in v.split():
                                if p.startswith('-D'):
                                    fk, fv = (p[2:].split('=', 1) + [None])[:2]
                                    # Only register genuine on/off flags; numeric/symbolic
                                    # defines stay as custom params and are preserved verbatim.
                                    if not is_boolean_flag_value(fv):
                                        continue
                                    cfg['flags'][fk] = (fv.upper() == 'TRUE')
        except: pass
        return cfg

    def save_selected_boards_to_mk(self):
        sel = self.board_listbox.curselection()
        if not sel:
            messagebox.showwarning("Warning", "No boards selected.")
            return
            
        flags_to_save = {f: v.get() for f, v in self.flag_vars.items() if v.get() in ("0", "1")}
        
        updated_count = 0
        processed_mk_files = set()
        
        self.append_log(f"\n--- Saving compiler flags to board.mk for {len(sel)} board(s) ---\n", "info")
        
        for i in sel:
            board = self.filtered_boards[i]
            mk_path = os.path.join(board["dir"], "board.mk")
            
            if mk_path in processed_mk_files:
                continue
                
            try:
                lines = []
                if os.path.exists(mk_path):
                    with open(mk_path, "r", encoding="utf-8") as f: 
                        lines = f.readlines()
                
                new_lines = []
                seen = set()
                for line in lines:
                    line_stripped = line.strip()
                    if not line_stripped or line_stripped.startswith('#'):
                        new_lines.append(line)
                        continue
                        
                    mod = line
                    for f, val in flags_to_save.items():
                        # Match the whole flag name (lookahead stops -DEFI_FOO matching -DEFI_FOO_BAR)
                        # and capture the current value.
                        pattern = rf'-D{re.escape(f)}(?![A-Za-z0-9_])(?:=([A-Za-z0-9_"\':\\/]+))?'
                        m = re.search(pattern, mod)
                        if not m:
                            continue
                        # Safety net: only rewrite if the existing value is on/off. Never convert
                        # a numeric/symbolic define (=4, =Gpio::B3, ...) or a bare define to FALSE.
                        if not is_boolean_flag_value(m.group(1)):
                            continue
                        val_str = "TRUE" if val == "1" else "FALSE"
                        mod = re.sub(pattern, f"-D{f}={val_str}", mod)
                        seen.add(f)
                    new_lines.append(mod)

                # Append new ones
                remaining_flags = set(flags_to_save.keys()) - seen
                if remaining_flags:
                    new_lines.append("\n# Added by Board Configuration Editor\n")
                    for f in sorted(remaining_flags):
                        val = flags_to_save[f]
                        val_str = "TRUE" if val == "1" else "FALSE"
                        new_lines.append(f"DDEFS += -D{f}={val_str}\n")
                
                with open(mk_path, "w", encoding="utf-8") as f: 
                    f.writelines(new_lines)
                
                processed_mk_files.add(mk_path)
                updated_count += 1
                self.append_log(f"Updated baseline: {os.path.basename(board['dir'])}/board.mk\n", "success")
            except Exception as e:
                self.append_log(f"Error updating {os.path.basename(board['dir'])}/board.mk: {e}\n", "stderr")
                
        self.status_var.set(f"Saved: {updated_count} board.mk file(s)")
        self.refresh_data()

    def save_selected_boards(self):
        sel = self.board_listbox.curselection()
        if not sel: 
            messagebox.showwarning("Warning", "No boards selected.")
            return
        
        flags_to_save = {f: v.get() for f, v in self.flag_vars.items() if v.get() in ("0", "1")}
        gui_skip = self.skip_rate_var.get().strip()
        gui_oblt = self.openblt_var.get()
        gui_custom = self.custom_params_var.get().strip()
        
        is_multi = len(sel) > 1
        updated_count = 0
        
        self.append_log(f"\n--- Applying changes to meta-info.env for {len(sel)} board(s) ---\n", "info")

        for i in sel:
            board = self.filtered_boards[i]
            path = board["path"]
            
            try:
                with open(path, "r", encoding="utf-8") as f: 
                    lines = f.readlines()
                
                new_lines = []
                keys_seen = set()
                
                for line in lines:
                    line_s = line.strip()
                    if not line_s or line_s.startswith("#"):
                        new_lines.append(line)
                        continue
                    
                    if "=" in line_s:
                        k, v = line_s.split("=", 1)
                        k = k.strip()
                        if k == "SKIP_RATE":
                            if gui_skip or not is_multi:
                                new_lines.append(f"SKIP_RATE={gui_skip if gui_skip else v}\n")
                            else:
                                new_lines.append(line)
                            keys_seen.add(k)
                        elif k == "USE_OPENBLT":
                            if gui_oblt != "mixed":
                                new_lines.append(f"USE_OPENBLT={'yes' if gui_oblt=='1' else 'no'}\n")
                            else:
                                new_lines.append(line)
                            keys_seen.add(k)
                        elif k == "EXTRA_PARAMS":
                            # Merge flags
                            current_config = self.parse_env_file(path)
                            merged_flags = dict(current_config['flags'])
                            
                            for f_key, f_val in flags_to_save.items():
                                board_var_val = self.flag_vars[f_key].get()
                                if board_var_val != "mixed":
                                    merged_flags[f_key] = (board_var_val == "1")
                                    
                            preserve_custom = (is_multi and not gui_custom)
                            other_parts = []
                            if preserve_custom:
                                for part in v.strip().split():
                                    if part.startswith('-D'):
                                        flag_part = part[2:]
                                        if '=' in flag_part:
                                            fkey, _ = flag_part.split('=', 1)
                                        else:
                                            fkey = flag_part
                                        if fkey not in self.flag_vars:
                                            other_parts.append(part)
                                    else:
                                        other_parts.append(part)
                            else:
                                other_parts = gui_custom.split()
                                
                            managed = []
                            for f_key, f_val in sorted(merged_flags.items()):
                                if f_key in self.flag_vars:
                                    val_str = "TRUE" if f_val else "FALSE"
                                    managed.append(f"-D{f_key}={val_str}")
                                    
                            final = other_parts + managed
                            new_lines.append(f"EXTRA_PARAMS={' '.join(final)}\n")
                            keys_seen.add(k)
                        else: 
                            new_lines.append(line)
                    else: 
                        new_lines.append(line)

                if "SKIP_RATE" not in keys_seen: 
                    if gui_skip or not is_multi:
                        new_lines.append(f"SKIP_RATE={gui_skip if gui_skip else '100'}\n")
                if "USE_OPENBLT" not in keys_seen: 
                    if gui_oblt != "mixed":
                        new_lines.append(f"USE_OPENBLT={'yes' if gui_oblt=='1' else 'no'}\n")
                if "EXTRA_PARAMS" not in keys_seen:
                    managed = [f"-D{f}={'TRUE' if val=='1' else 'FALSE'}" for f, val in sorted(flags_to_save.items())]
                    final = gui_custom.split() + managed
                    new_lines.append(f"EXTRA_PARAMS={' '.join(final)}\n")

                with open(path, "w", encoding="utf-8") as f: 
                    f.writelines(new_lines)
                
                updated_count += 1
                self.append_log(f"Updated variant: {board['display']} (.env)\n", "success")
            except Exception as e:
                self.append_log(f"Error saving {board['display']}: {e}\n", "stderr")
                
        self.status_var.set(f"Saved: {updated_count} .env file(s)")
        self.refresh_data()

    def disable_edit_panel(self):
        self.set_widgets_state(self.edit_container, "disabled")
        self.save_env_btn.configure(state="disabled")
        self.save_mk_btn.configure(state="disabled")
        self.refresh_btn_right.configure(state="disabled")

    def enable_edit_panel(self):
        self.set_widgets_state(self.edit_container, "normal")
        self.save_env_btn.configure(state="normal")
        self.save_mk_btn.configure(state="normal")
        self.refresh_btn_right.configure(state="normal")

    def set_widgets_state(self, widget, state):
        try: 
            widget.configure(state=state)
        except: 
            pass
        for child in widget.winfo_children(): 
            self.set_widgets_state(child, state)

    def refresh_data(self):
        # Save selection
        selected_indices = self.board_listbox.curselection()
        selected_paths = []
        for i in selected_indices:
            if i < len(self.filtered_boards):
                selected_paths.append(self.filtered_boards[i]["path"])
                
        # Re-scan
        self.scan_boards()
        
        # Restore selections
        for idx, board in enumerate(self.filtered_boards):
            if board["path"] in selected_paths:
                self.board_listbox.select_set(idx)
                
        self.update_selection_count()
        self.load_selected_boards()
        self.append_log("Refreshed configurations from disk.\n", "info")

    def select_all_boards(self): 
        self.board_listbox.select_set(0, tk.END)
        self.update_selection_count()
        self.load_selected_boards()
        
    def append_log(self, text, tag="stdout"): 
        self.log_text.insert(tk.END, text, tag)
        self.log_text.see(tk.END)

if __name__ == "__main__":
    app = BoardConfigApp()
    app.mainloop()
