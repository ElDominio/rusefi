#!/usr/bin/env python3
import os
import sys

# Auto-re-execute in the virtual environment if it exists and we're not already running inside it
script_dir = os.path.dirname(os.path.abspath(__file__))
venv_dir = os.path.join(script_dir, '.venv')

if os.path.isdir(venv_dir):
    venv_python = os.path.join(venv_dir, 'bin', 'python3')
    if not os.path.exists(venv_python):
        venv_python = os.path.join(venv_dir, 'bin', 'python')
    if not os.path.exists(venv_python):
        venv_python = os.path.join(venv_dir, 'Scripts', 'python.exe')

    if os.path.exists(venv_python):
        real_sys_prefix = os.path.realpath(sys.prefix)
        real_venv_dir = os.path.realpath(venv_dir)
        if real_sys_prefix != real_venv_dir:
            if os.environ.get('_AUTOVENV_RETRY') != '1':
                os.environ['_AUTOVENV_RETRY'] = '1'
                os.execv(venv_python, [venv_python] + sys.argv)

import tkinter as tk
from tkinter import scrolledtext, messagebox, ttk, filedialog, simpledialog
import subprocess
import threading
import os
import signal
import re
import zlib

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

# --- CONFIGURATION ---
# Common STM32 targets for rusEFI
TARGETS = [
    "target/stm32f4x.cfg",
    "target/stm32f7x.cfg",
    "target/stm32h7x.cfg",
    "target/stm32f1x.cfg",
    "target/stm32f0x.cfg",
]

INTERFACES = [
    "interface/stlink.cfg",
    "interface/stlink-v2.cfg",
    "interface/stlink-v2-1.cfg",
    "interface/cmsis-dap.cfg",
    "interface/jlink.cfg",
]

class OpenOCDGui:
    def __init__(self, root):
        self.root = root
        self.root.title("rusEFI OpenOCD Tool")
        self.root.geometry("900x650")
        self.process = None
        self.dfu_device = None # Stores (vid_pid, alt_setting, serial)
        self.password = None
        
        # --- Serial Connection Frame (for DFU trigger) ---
        serial_frame = tk.LabelFrame(root, text="Serial Connection (for DFU Trigger)", padx=10, pady=10)
        serial_frame.pack(fill="x", padx=10, pady=5)

        tk.Label(serial_frame, text="Port:").grid(row=0, column=0, sticky="w")
        self.serial_port_var = tk.StringVar()
        self.serial_combo = ttk.Combobox(serial_frame, textvariable=self.serial_port_var)
        self.serial_combo.grid(row=0, column=1, padx=5, pady=2, sticky="ew")

        self.refresh_ports_btn = tk.Button(serial_frame, text="Refresh Ports", command=self.refresh_ports)
        self.refresh_ports_btn.grid(row=0, column=2, padx=5, pady=2)

        self.reboot_dfu_btn = tk.Button(serial_frame, text="Reboot to DFU (Serial)", command=self.reboot_to_dfu_serial, bg="#6f42c1", fg="white")
        self.reboot_dfu_btn.grid(row=0, column=3, padx=5, pady=2)

        serial_frame.columnconfigure(1, weight=1)
        self.refresh_ports()

        # --- Settings Frame ---
        settings_frame = tk.LabelFrame(root, text="Connection Settings", padx=10, pady=10)
        settings_frame.pack(fill="x", padx=10, pady=5)

        tk.Label(settings_frame, text="Interface:").grid(row=0, column=0, sticky="w")
        self.interface_var = tk.StringVar(value=INTERFACES[0])
        self.interface_combo = ttk.Combobox(settings_frame, textvariable=self.interface_var, values=INTERFACES)
        self.interface_combo.grid(row=0, column=1, padx=5, pady=2, sticky="ew")

        tk.Label(settings_frame, text="Target:").grid(row=1, column=0, sticky="w")
        self.target_var = tk.StringVar(value=TARGETS[0])
        self.target_combo = ttk.Combobox(settings_frame, textvariable=self.target_var, values=TARGETS)
        self.target_combo.grid(row=1, column=1, padx=5, pady=2, sticky="ew")

        self.use_pkexec_var = tk.BooleanVar(value=True)
        self.use_pkexec_check = tk.Checkbutton(settings_frame, text="Use Elevated Privileges", variable=self.use_pkexec_var)
        self.use_pkexec_check.grid(row=2, column=1, sticky="w")

        self.set_pwd_btn = tk.Button(settings_frame, text="Set Password (Cache)", command=self.set_password)
        self.set_pwd_btn.grid(row=2, column=2, sticky="e")

        settings_frame.columnconfigure(1, weight=1)

        # --- Flash/Dump Frame ---
        action_frame = tk.LabelFrame(root, text="Actions", padx=10, pady=10)
        action_frame.pack(fill="x", padx=10, pady=5)

        # Flash File Selection
        tk.Label(action_frame, text="File to Flash:").grid(row=0, column=0, sticky="w")
        self.flash_file_var = tk.StringVar(value=os.path.abspath("rusefi.bin") if os.path.exists("rusefi.bin") else "")
        self.flash_file_entry = tk.Entry(action_frame, textvariable=self.flash_file_var)
        self.flash_file_entry.grid(row=0, column=1, padx=5, pady=2, sticky="ew")
        self.browse_flash_btn = tk.Button(action_frame, text="Browse...", command=self.browse_flash_file)
        self.browse_flash_btn.grid(row=0, column=2, padx=5, pady=2)

        # Dump Settings
        tk.Label(action_frame, text="Dump Size (bytes):").grid(row=1, column=0, sticky="w")
        self.dump_size_var = tk.StringVar(value="1048576") # Default 1MB
        self.dump_size_entry = tk.Entry(action_frame, textvariable=self.dump_size_var)
        self.dump_size_entry.grid(row=1, column=1, padx=5, pady=2, sticky="w")
        tk.Label(action_frame, text="(e.g. 1MB=1048576, 2MB=2097152)").grid(row=1, column=2, sticky="w")

        action_frame.columnconfigure(1, weight=1)

        # --- Control Buttons ---
        self.btn_frame = tk.Frame(root)
        self.btn_frame.pack(pady=5)

        self.start_btn = tk.Button(self.btn_frame, text="Start Server", command=self.toggle_server, bg="#28a745", fg="white", width=12)
        self.start_btn.grid(row=0, column=0, padx=5)

        self.detect_btn = tk.Button(self.btn_frame, text="Detect Chip", command=self.detect_chip, bg="#17a2b8", fg="white", width=12)
        self.detect_btn.grid(row=0, column=1, padx=5)

        self.flash_btn = tk.Button(self.btn_frame, text="Flash Chip", command=self.flash_chip, bg="#007bff", fg="white", width=12)
        self.flash_btn.grid(row=0, column=2, padx=5)

        self.dfu_flash_btn = tk.Button(self.btn_frame, text="DFU Flash", command=self.flash_dfu, bg="#fd7e14", fg="white", width=12)
        self.dfu_flash_btn.grid(row=0, column=3, padx=5)

        self.dump_btn = tk.Button(self.btn_frame, text="Dump Chip", command=self.dump_chip, bg="#6c757d", fg="white", width=12)
        self.dump_btn.grid(row=0, column=4, padx=5)

        self.unlock_btn = tk.Button(self.btn_frame, text="Unlock Chip", command=self.unlock_chip, bg="#d63384", fg="white", width=12)
        self.unlock_btn.grid(row=0, column=5, padx=5)

        self.clear_btn = tk.Button(self.btn_frame, text="Clear Logs", command=self.clear_logs, width=12)
        self.clear_btn.grid(row=0, column=6, padx=5)

        # Console Output
        self.console = scrolledtext.ScrolledText(root, bg="#1e1e1e", fg="#d4d4d4", font=("Courier", 10))
        self.console.pack(padx=10, pady=10, fill="both", expand=True)

        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)

    def log(self, text):
        self.console.insert(tk.END, text + "\n")
        self.console.see(tk.END)

    def set_password(self):
        pwd = simpledialog.askstring("Password", "Enter sudo password (cached for session):", show='*')
        if pwd is not None:
            self.password = pwd

    def _run_subprocess(self, cmd, **kwargs):
        """Helper to run a subprocess, optionally using sudo -S if password is set."""
        is_elevated_cmd = cmd[0] in ["pkexec", "sudo"]
        
        if is_elevated_cmd and self.use_pkexec_var.get() and self.password:
            # Replace whatever elevation with sudo -S
            if cmd[0] == "pkexec":
                cmd = ["sudo", "-S"] + cmd[1:]
            elif cmd[0] == "sudo" and "-S" not in cmd:
                cmd = ["sudo", "-S"] + cmd[1:]
            
            # Ensure stdin is PIPE to send password
            kwargs['stdin'] = subprocess.PIPE
            
            # Check for text/universal_newlines to know if we send string or bytes
            is_text = kwargs.get('text', False) or kwargs.get('universal_newlines', False)
            
            process = subprocess.Popen(cmd, **kwargs)
            
            pwd_input = self.password + "\n"
            if not is_text:
                pwd_input = pwd_input.encode()
            
            try:
                process.stdin.write(pwd_input)
                process.stdin.flush()
            except Exception:
                pass
            
            return process
        else:
            return subprocess.Popen(cmd, **kwargs)

    def browse_flash_file(self):
        filename = filedialog.askopenfilename(filetypes=[("Binary files", "*.bin"), ("Hex files", "*.hex"), ("All files", "*.*")])
        if filename:
            self.flash_file_var.set(filename)

    def toggle_server(self):
        if self.process is None:
            if self.use_pkexec_var.get() and self.password is None:
                pwd = simpledialog.askstring("Password Required", "Enter sudo password (cached for session):", show="*")
                if pwd:
                    self.password = pwd
            self.start_server()
        else:
            self.stop_server()

    def get_base_cmd(self):
        cmd = []
        if self.use_pkexec_var.get():
            cmd.append("pkexec")
        cmd += [
            "openocd",
            "-f", self.interface_var.get(),
            "-f", self.target_var.get()
        ]
        return cmd

    def detect_dfu_devices(self):
        try:
            # We don't use pkexec for listing DFU devices, as it doesn't need root permissions
            # and running pkexec in a background thread can block/fail.
            p_dfu = subprocess.Popen(["dfu-util", "--list"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            dfu_list, _ = p_dfu.communicate()
            if "Found DFU" in dfu_list:
                dfu_matches = re.findall(r"Found DFU: \[(.*?)\] .*? alt=(\d+), name=\"(.*?)\"(?:, serial=\"(.*?)\")?", dfu_list)
                return [(vid_pid, alt, name, serial if serial else "") for vid_pid, alt, name, serial in dfu_matches]
        except Exception as e:
            self.log(f"  (DFU scan failed: {e})")
        return []

    def detect_chip(self):
        if self.process:
            messagebox.showwarning("Warning", "Please stop the OpenOCD server before detecting chip.")
            return
        
        if self.use_pkexec_var.get() and self.password is None:
            pwd = simpledialog.askstring("Password Required", "Enter sudo password to avoid multiple prompts (cached for session):", show="*")
            if pwd:
                self.password = pwd

        self.log("> Probing chip for information...")
        threading.Thread(target=self.run_detect).start()

    def run_detect(self):
        self.log("> Starting Auto-Scan for connected hardware...")
        self.dfu_device = None
        
        # 1. Try to guess interface from lsusb
        guessed_interface = None
        try:
            lsusb = subprocess.run(["lsusb"], capture_output=True, text=True).stdout.lower()
            
            # Check for DFU specifically via dfu-util for precise mapping
            try:
                dfu_matches = self.detect_dfu_devices()
                if dfu_matches:
                    self.log("  [Detected DFU-capable device(s)]")
                    candidates = []
                    for vid_pid, alt, name, serial in dfu_matches:
                        # Prioritize internal flash partitions
                        if "0x08000000" in name or "Internal Flash" in name:
                            candidates.append((vid_pid, alt, name, serial))
                    
                    if len(candidates) > 1:
                        self.log(f"    -> Multiple DFU candidates found ({len(candidates)}). Prompting for selection...")
                        self.root.after(0, lambda: self.prompt_dfu_selection(candidates))
                    elif len(candidates) == 1:
                        c = candidates[0]
                        self.dfu_device = (c[0], c[1], c[3])
                        self.log(f"    -> Found Flash Partition: {c[0]} (alt={c[1]}) serial={c[3]}")
                    elif dfu_matches:
                        # Fallback to first if no internal flash found
                        c = dfu_matches[0]
                        self.dfu_device = (c[0], c[1], c[3])
                        self.log(f"    -> Suggest DFU Flash using {c[0]} (alt={c[1]}) serial={c[3]}")
            except:
                if "0483:df11" in lsusb or "dfu" in lsusb:
                    self.log("  [!!! Detected Device in DFU Mode via USB !!!]")

            if "st-link" in lsusb or "stlink" in lsusb:
                guessed_interface = "interface/stlink.cfg"
                self.log("  [Detected ST-Link via USB]")
            elif "j-link" in lsusb or "jlink" in lsusb:
                guessed_interface = "interface/jlink.cfg"
                self.log("  [Detected J-Link via USB]")
        except Exception as e:
            self.log(f"  (USB scan failed: {e})")

        # 2. Define search order
        interfaces_to_try = [guessed_interface] if guessed_interface else []
        interfaces_to_try += [i for i in INTERFACES if i != guessed_interface]
        
        # 3. Iterate through combinations
        found = False
        for interface in interfaces_to_try:
            if found: break
            for target in TARGETS:
                self.log(f"  Trying {interface} with {target}...")
                cmd = ["openocd", "-f", interface, "-f", target, "-c", "init; flash probe 0; flash list; exit"]
                if self.use_pkexec_var.get():
                    cmd = ["pkexec"] + cmd
                
                try:
                    # Run with a short timeout to prevent hanging on wrong combinations
                    process = self._run_subprocess(
                        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                        bufsize=1, universal_newlines=True
                    )
                    
                    full_output = []
                    # Wait up to 5 seconds for this specific combo (pkexec might take time)
                    def kill_proc():
                        try:
                            if process.poll() is None: process.terminate()
                        except: pass
                    
                    timer = threading.Timer(5.0, kill_proc)
                    timer.start()
                    
                    for line in iter(process.stdout.readline, ''):
                        l = line.strip()
                        full_output.append(l)
                        self.root.after(0, self.log, f"      {l}")
                    
                    process.wait()
                    timer.cancel()
                    
                    output = "\n".join(full_output)
                    
                    # Look for evidence of success
                    # We check for "flash size" or "device id" or "flash bank"
                    if "flash size" in output.lower() or "device id" in output.lower() or "flash bank" in output.lower():
                        # Basic check to see if it actually worked vs just printed an error
                        if "error" not in output.lower() or "flash size" in output.lower():
                            self.root.after(0, lambda i=interface, t=target, o=output: self.update_ui_after_detect(i, t, o))
                            found = True
                            break
                except Exception as e:
                    self.log(f"    Error trying combo: {e}")
                    continue
        
        if not found:
            self.root.after(0, lambda: messagebox.showerror("Auto-Detect Failed", "Could not automatically detect any chip.\n\nPlease ensure your debugger is plugged in and select the Interface/Target manually."))

    def update_ui_after_detect(self, interface, target, output):
        self.interface_var.set(interface)
        self.target_var.set(target)
        self.log(f"*** AUTO-DETECT SUCCESS: {interface} + {target} ***")
        
        # Logic to find size in output
        size_bytes = None
        # Look for "flash size 1024kbytes", "flash size = 2048 KiB", or hex sizes
        match = re.search(r"size\s*(?:=)?\s*(0x[0-9a-fA-F]+|[0-9]+(?:\s*[Kk][Ii]?[Bb](?:ytes)?)?)", output)
        if match:
            size_str = match.group(1).lower().replace(" ", "")
            if any(unit in size_str for unit in ["kib", "kb", "kbytes"]):
                digits = re.search(r"([0-9]+)", size_str).group(1)
                size_bytes = int(digits) * 1024
            elif size_str.startswith("0x"):
                size_bytes = int(size_str, 16)
            else:
                try:
                    size_bytes = int(size_str)
                except:
                    size_bytes = None
        
        if size_bytes:
            self.update_dump_size(size_bytes)
        else:
            self.log("! Connected, but could not parse flash size automatically.")
            messagebox.showinfo("Chip Detected", f"Connected via {interface} + {target}\n\nPlease enter dump size manually.")

    def update_dump_size(self, size):
        self.dump_size_var.set(str(size))
        self.log(f"*** Detected Flash Size: {size} bytes ({size//1024} KB) ***")
        messagebox.showinfo("Chip Detected", f"Auto-detected hardware:\n\nInterface: {self.interface_var.get()}\nTarget: {self.target_var.get()}\nSize: {size//1024} KB")

    def prompt_dfu_selection(self, candidates, auto_flash=False):
        # Create a selection window
        top = tk.Toplevel(self.root)
        top.title("Select DFU Device")
        top.geometry("600x400")
        top.transient(self.root)
        top.grab_set()

        tk.Label(top, text="Multiple DFU devices found. Please select the target device:", font=("Helvetica", 10, "bold")).pack(pady=10)
        
        frame = tk.Frame(top)
        frame.pack(padx=10, pady=5, fill=tk.BOTH, expand=True)
        
        scrollbar = tk.Scrollbar(frame)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        listbox = tk.Listbox(frame, width=70, yscrollcommand=scrollbar.set, font=("Courier", 9))
        listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.config(command=listbox.yview)
        
        for vid_pid, alt, name, serial in candidates:
            listbox.insert(tk.END, f"[{vid_pid}] Alt:{alt} | {name} | S/N: {serial}")
            
        def on_select():
            selection = listbox.curselection()
            if selection:
                idx = selection[0]
                c = candidates[idx]
                self.dfu_device = (c[0], c[1], c[3])
                self.log(f"*** Selected DFU Device: {c[0]} (alt={c[1]}) serial={c[3]} ***")
                top.destroy()
                if auto_flash:
                    self.flash_dfu()
                else:
                    messagebox.showinfo("DFU Device Selected", f"Selected for DFU Flash:\n\nDevice: {c[2]}\nVID:PID: {c[0]}\nAlt: {c[1]}\nSerial: {c[3]}")
                
        tk.Button(top, text="Select Device", command=on_select, bg="#28a745", fg="white", padx=20).pack(pady=15)
        
        # Center the window
        top.update_idletasks()
        x = self.root.winfo_x() + (self.root.winfo_width() // 2) - (top.winfo_width() // 2)
        y = self.root.winfo_y() + (self.root.winfo_height() // 2) - (top.winfo_height() // 2)
        top.geometry(f"+{x}+{y}")

    def start_server(self):
        self.log(f"> Starting OpenOCD with {self.interface_var.get()} and {self.target_var.get()}...")
        try:
            self.process = self._run_subprocess(
                self.get_base_cmd(),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                preexec_fn=os.setsid
            )
            self.start_btn.config(text="Stop Server", bg="#dc3545")
            threading.Thread(target=self.read_output, daemon=True).start()
        except Exception as e:
            self.log(f"ERROR: Could not start OpenOCD: {e}")

    def read_output(self):
        if not self.process:
            return
        for line in iter(self.process.stdout.readline, ''):
            self.root.after(0, self.log, line.strip())
        self.process.wait()
        self.root.after(0, self.on_process_end)

    def stop_server(self):
        if self.process:
            self.log("> Stopping Server...")
            os.killpg(os.getpgid(self.process.pid), signal.SIGTERM)
            self.process = None
            self.start_btn.config(text="Start Server", bg="#28a745")

    def on_process_end(self):
        self.process = None
        self.start_btn.config(text="Start Server", bg="#28a745")

    def clear_logs(self):
        self.console.delete('1.0', tk.END)

    def flash_dfu(self):
        if self.process:
            messagebox.showwarning("Warning", "Please stop the OpenOCD server before DFU flashing.")
            return

        file_path = self.flash_file_var.get()
        if not file_path or not os.path.exists(file_path):
            messagebox.showerror("Error", f"File not found: {file_path}")
            return

        # Auto-detect DFU device if not set yet
        if not self.dfu_device:
            self.log("> Scanning for DFU devices before flashing...")
            dfu_matches = self.detect_dfu_devices()
            
            if dfu_matches:
                # Prioritize STM32 bootloader (0483:df11)
                stm32_matches = [m for m in dfu_matches if m[0].lower() == "0483:df11"]
                target_matches = stm32_matches if stm32_matches else dfu_matches
                
                # Filter for internal flash partitions (usually alt=0 or name contains 0x08000000)
                internal_flash_matches = [m for m in target_matches if "0x08000000" in m[2] or m[1] == '0' or "Internal Flash" in m[2]]
                
                if internal_flash_matches:
                    candidates = internal_flash_matches
                else:
                    candidates = target_matches
                
                # Check how many physical devices we have (by serial, or VID:PID if serial is missing)
                unique_devices = list(set((c[0], c[3]) for c in candidates))
                
                if len(unique_devices) == 1:
                    # Single physical device found! Pick the first candidate
                    c = candidates[0]
                    self.dfu_device = (c[0], c[1], c[3])
                    self.log(f"  -> Automatically selected DFU device: {c[0]} (alt={c[1]}) serial={c[3]}")
                elif len(unique_devices) > 1:
                    # Multiple physical DFU devices! Prompt user to select
                    self.log("  -> Multiple DFU devices found. Prompting user...")
                    self.prompt_dfu_selection(candidates, auto_flash=True)
                    return
            else:
                self.log("  -> No DFU devices detected. Will attempt standard DFU fallback.")

        if self.use_pkexec_var.get() and self.password is None:
            pwd = simpledialog.askstring("Password Required", "Enter sudo password (cached for session):", show="*")
            if pwd:
                self.password = pwd

        self.log(f"> Starting DFU Flash process for: {file_path}")
        threading.Thread(target=self.run_dfu_flash, args=(file_path,)).start()

    def run_dfu_flash(self, file_path):
        # Build DFU command
        # If we detected a specific partition, use its VID:PID and Alt setting
        dfu_bin = "dfu-util"
        base_cmd = ["pkexec", dfu_bin] if self.use_pkexec_var.get() else [dfu_bin]
        
        if self.dfu_device:
            vid_pid, alt, serial = self.dfu_device
            # Use -d for VID:PID, -a for alt setting, and -S for serial to be specific
            cmd = base_cmd + ["-d", vid_pid, "-a", alt]
            if serial:
                cmd += ["-S", serial]
            cmd += ["-s", "0x08000000:leave", "-D", file_path]
        else:
            # Standard STM32 DFU command fallback
            cmd = base_cmd + ["-a", "0", "-s", "0x08000000:leave", "-D", file_path]
            
        self.log(f"> Executing: {' '.join(cmd)}")

        try:
            process = self._run_subprocess(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                bufsize=1, universal_newlines=True
            )
            for line in iter(process.stdout.readline, ''):
                self.root.after(0, self.log, line.strip())
            process.wait()
            
            if process.returncode == 0:
                self.root.after(0, lambda: messagebox.showinfo("Success", "DFU Flash successful!"))
            else:
                self.root.after(0, lambda: messagebox.showerror("Failure", "DFU Flash failed. Check logs and ensure dfu-util is installed and you have permissions."))
        except Exception as e:
            self.root.after(0, lambda: self.log(f"DFU ERROR: {e}"))

    def flash_chip(self):
        if self.process:
            messagebox.showwarning("Warning", "Please stop the OpenOCD server before flashing.")
            return
        
        file_path = self.flash_file_var.get()
        if not file_path or not os.path.exists(file_path):
            messagebox.showerror("Error", f"File not found: {file_path}")
            return

        if self.use_pkexec_var.get() and self.password is None:
            pwd = simpledialog.askstring("Password Required", "Enter sudo password (cached for session):", show="*")
            if pwd:
                self.password = pwd

        self.log(f"> Preparing to flash: {file_path}")
        threading.Thread(target=self.run_flash, args=(file_path,)).start()

    def run_flash(self, file_path):
        # We wrap the file path in curly braces for OpenOCD TCL commands to handle spaces
        cmd = self.get_base_cmd() + [
            "-c", f"program {{{file_path}}} verify reset exit 0x08000000"
        ]
        try:
            process = self._run_subprocess(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            
            for line in iter(process.stdout.readline, ''):
                self.root.after(0, self.log, line.strip())
            
            process.wait()
            
            if process.returncode == 0:
                self.root.after(0, lambda: messagebox.showinfo("Success", "Flash successful!"))
            else:
                self.root.after(0, lambda: messagebox.showerror("Failure", "Flash failed. Check logs."))
        except Exception as e:
            self.root.after(0, lambda: self.log(f"FLASH ERROR: {e}"))

    def dump_chip(self):
        if self.process:
            messagebox.showwarning("Warning", "Please stop the OpenOCD server before dumping.")
            return

        save_path = filedialog.asksaveasfilename(defaultextension=".bin", filetypes=[("Binary files", "*.bin"), ("All files", "*.*")])
        if not save_path:
            return

        size = self.dump_size_var.get()

        if self.use_pkexec_var.get() and self.password is None:
            pwd = simpledialog.askstring("Password Required", "Enter sudo password (cached for session):", show="*")
            if pwd:
                self.password = pwd

        self.log(f"> Preparing to dump {size} bytes to: {save_path}")
        threading.Thread(target=self.run_dump, args=(save_path, size)).start()

    def run_dump(self, save_path, size):
        # command: init; halt; dump_image <file> <addr> <size>; reset; exit
        # We wrap the file path in curly braces for OpenOCD TCL commands to handle spaces
        cmd = self.get_base_cmd() + [
            "-c", "init",
            "-c", "halt",
            "-c", f"dump_image {{{save_path}}} 0x08000000 {size}",
            "-c", "reset",
            "-c", "exit"
        ]
        try:
            process = self._run_subprocess(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            
            for line in iter(process.stdout.readline, ''):
                self.root.after(0, self.log, line.strip())
            
            process.wait()
            
            if process.returncode == 0:
                self.root.after(0, lambda: messagebox.showinfo("Success", f"Dump complete!\nSaved to: {save_path}"))
            else:
                self.root.after(0, lambda: messagebox.showerror("Failure", "Dump failed. Check logs."))
        except Exception as e:
            self.root.after(0, lambda: self.log(f"DUMP ERROR: {e}"))

    def unlock_chip(self):
        if self.process:
            messagebox.showwarning("Warning", "Please stop the OpenOCD server before unlocking.")
            return

        confirm = messagebox.askyesno(
            "Unlock / Disable Write Protect",
            "This will attempt to disable write protection and unlock readout protection (RDP/WRP) on the target STM32.\n\n"
            "WARNING: If Readout Protection (RDP Level 1) is currently active, unlocking WILL perform a full chip mass erase!\n\n"
            "Do you want to proceed?"
        )
        if not confirm:
            return

        if self.use_pkexec_var.get() and self.password is None:
            pwd = simpledialog.askstring("Password Required", "Enter sudo password (cached for session):", show="*")
            if pwd:
                self.password = pwd

        self.log(f"> Preparing to unlock chip ({self.target_var.get()})...")
        threading.Thread(target=self.run_unlock).start()

    def run_unlock(self):
        target_cfg = self.target_var.get().lower()
        if "stm32f4" in target_cfg or "stm32f7" in target_cfg or "stm32f2" in target_cfg:
            family_unlock = "stm32f2x unlock 0"
        elif "stm32h7" in target_cfg:
            family_unlock = "stm32h7x unlock 0"
        elif "stm32l4" in target_cfg or "stm32g4" in target_cfg:
            family_unlock = "stm32l4x unlock 0"
        elif "stm32l0" in target_cfg or "stm32l1" in target_cfg:
            family_unlock = "stm32lx unlock 0"
        else:
            family_unlock = "stm32f1x unlock 0"

        # TCL script: halt target, run family unlock (if applicable), remove sector write protection, and reset device
        tcl_script = f"init; reset halt; catch {{{family_unlock}}}; catch {{flash protect 0 0 last off}}; reset; exit"
        cmd = self.get_base_cmd() + ["-c", tcl_script]

        self.log(f"> Executing unlock sequence: {' '.join(cmd)}")
        try:
            process = self._run_subprocess(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            
            for line in iter(process.stdout.readline, ''):
                self.root.after(0, self.log, line.strip())
            
            process.wait()
            
            if process.returncode == 0:
                self.root.after(0, lambda: messagebox.showinfo("Success", "Unlock procedure complete!\n\nNote: If option bytes were modified, power-cycle the board or reset for changes to take effect."))
            else:
                self.root.after(0, lambda: messagebox.showerror("Failure", "Unlock failed. Check logs for details."))
        except Exception as e:
            self.root.after(0, lambda: self.log(f"UNLOCK ERROR: {e}"))

    def on_closing(self):
        if self.process:
            self.stop_server()
        self.root.destroy()

    def refresh_ports(self):
        if not HAS_SERIAL:
            self.serial_combo['values'] = ["pyserial not installed"]
            return
        
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.serial_combo['values'] = ports
        if ports:
            if not self.serial_port_var.get() or self.serial_port_var.get() not in ports:
                self.serial_port_var.set(ports[0])
        else:
            self.serial_port_var.set("")

    def reboot_to_dfu_serial(self):
        if not HAS_SERIAL:
            messagebox.showerror("Error", "pyserial is not installed. Please run:\npip install pyserial")
            return

        port = self.serial_port_var.get()
        if not port:
            messagebox.showwarning("Warning", "Please select a COM port first.")
            return

        self.log(f"> Attempting to reboot {port} to DFU mode...")
        try:
            # rusEFI TS binary protocol packet for TS_IO_TEST_COMMAND ('Z') with
            # subsystem=JUMP_DFU_COMMAND (0x00ba) + index=0x0000, both big-endian u16.
            # Wire format is [BE u16 length][command+payload][BE u32 CRC32 of command+payload]
            # (see TsChannelBase::crcAndWriteBuffer / handleCrcCommand in tunerstudio_io.cpp) -
            # the command+payload bytes alone are not a valid packet on their own.
            content = b"Z\x00\xba\x00\x00"
            crc = zlib.crc32(content) & 0xFFFFFFFF
            packet = len(content).to_bytes(2, "big") + content + crc.to_bytes(4, "big")

            ser = serial.Serial(port, 115200, timeout=1)
            ser.write(packet)
            ser.close()
            self.log(f"  Success: Reboot command sent ({packet.hex(' ')}).")
            messagebox.showinfo("Success", f"Reboot command sent to {port}.\nDevice should now enter DFU mode.")
        except Exception as e:
            self.log(f"  ERROR: Failed to send reboot command: {e}")
            messagebox.showerror("Error", f"Failed to send command to {port}:\n{e}")

if __name__ == "__main__":
    root = tk.Tk()
    app = OpenOCDGui(root)
    try:
        root.mainloop()
    except KeyboardInterrupt:
        print("\nInterrupted by user, shutting down...")
        if app.process:
            app.stop_server()
        root.destroy()
