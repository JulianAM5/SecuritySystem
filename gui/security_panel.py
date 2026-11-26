#!/usr/bin/env python3
"""
Panel de control del sistema de seguridad (ATmega328P / Arduino NANO).

Es solo un "envoltorio" grafico sobre los comandos UART del firmware:
    PASS (vieja) (nueva)      ARMADO [s]         DESARMADO [s]
    LIST                      NEW (nombre)       DEL (id)       MOD (id) (nombre)
    LOG alarm                 TIME [YYYY-MM-DD HH:MM:SS]

Requisitos:  pip install pyserial      (tkinter viene con Python)
Uso:         python security_panel.py
"""
import csv
import datetime
import queue
import threading
import traceback
import time
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

import serial
from serial.tools import list_ports

BAUDRATE = 9600
RESET_DELAY_S = 2.5        # el Arduino NANO se reinicia al abrir el puerto
RESPONSE_TIMEOUT_S = 3.0
NAME_MAX = 12
TIME_MIN, TIME_MAX = 10, 60
AUTO_REFRESH_MS = 2000


# --------------------------------------------------------------------------
#  Comunicacion serial
# --------------------------------------------------------------------------
class DeviceError(Exception):
    """El firmware respondio ERROR."""


class SerialClient:
    def __init__(self, on_traffic=None):
        self.ser = None
        self.lock = threading.Lock()
        self.on_traffic = on_traffic

    @property
    def is_open(self):
        return self.ser is not None and self.ser.is_open

    def open(self, port, baud=BAUDRATE, reset_delay=RESET_DELAY_S):
        self.ser = serial.Serial(port, baud, timeout=0.2, write_timeout=2)
        time.sleep(reset_delay)
        self.ser.reset_input_buffer()

    def close(self):
        with self.lock:
            if self.ser is not None:
                try:
                    self.ser.close()
                finally:
                    self.ser = None

    def _log(self, direction, text):
        if self.on_traffic:
            self.on_traffic(direction, text)

    def request(self, cmd, kind="ok", timeout=RESPONSE_TIMEOUT_S):
        """
        Envia un comando y espera la respuesta.
          kind="ok"    -> lee lineas hasta 'OK' (devuelve las lineas previas).
          kind="value" -> devuelve la primera linea (ARMADO/DESARMADO/TIME sin
                          parametros responden solo el valor, sin 'OK').
        Lanza DeviceError si el firmware responde ERROR y TimeoutError si no responde.
        """
        if len(cmd) > 39:
            raise ValueError("El comando excede 39 caracteres (limite del firmware).")
        with self.lock:
            if not self.is_open:
                raise serial.SerialException("Puerto cerrado")
            self.ser.reset_input_buffer()
            self.ser.write((cmd + "\n").encode("ascii"))
            self._log(">>", cmd)

            lines = []
            deadline = time.time() + timeout
            while time.time() < deadline:
                raw = self.ser.readline()
                if not raw:
                    continue
                line = raw.decode("ascii", "replace").strip()
                if not line:
                    continue
                self._log("<<", line)
                if line == "ERROR":
                    raise DeviceError(cmd)
                if kind == "value":
                    return line
                if line == "OK":
                    return lines
                lines.append(line)
        raise TimeoutError(cmd)


def parse_table(lines, ncols):
    """Convierte lineas '| a | b | c |' en listas, ignorando el encabezado."""
    rows = []
    for line in lines:
        if not line.startswith("|"):
            continue
        parts = [p.strip() for p in line.strip().strip("|").split("|")]
        if len(parts) != ncols or parts[0] in ("ID", "#"):
            continue
        rows.append(parts)
    return rows


# --------------------------------------------------------------------------
#  Interfaz grafica
# --------------------------------------------------------------------------
class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Sistema de seguridad - Panel de control")
        self.geometry("780x600")
        self.minsize(720, 520)

        self.client = SerialClient(self._traffic)
        self.busy = 0
        self._controls = []          # widgets que se habilitan al conectar
        self._ui_q = queue.Queue()   # los hilos de trabajo NO tocan Tk: publican aqui

        self._build_ui()
        self.refresh_ports()
        self._set_connected(False)
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self.after(30, self._drain)
        self.after(AUTO_REFRESH_MS, self._auto_tick)

    # ------------------------------------------- comunicacion con la UI --
    def _post(self, fn, *args):
        """Seguro desde cualquier hilo: encola una llamada para el hilo de la UI."""
        self._ui_q.put((fn, args))

    def _drain(self):
        try:
            while True:
                fn, args = self._ui_q.get_nowait()
                try:
                    fn(*args)
                except Exception:                         # noqa: BLE001
                    traceback.print_exc()
        except queue.Empty:
            pass
        self.after(30, self._drain)

    # ---------------------------------------------------------------- UI --
    def _reg(self, widget):
        self._controls.append(widget)
        return widget

    def _build_ui(self):
        # --- barra de conexion ---
        bar = ttk.Frame(self, padding=8)
        bar.pack(fill="x")
        ttk.Label(bar, text="Puerto:").pack(side="left")
        self.port_var = tk.StringVar()
        self.port_cb = ttk.Combobox(bar, textvariable=self.port_var, width=38, state="readonly")
        self.port_cb.pack(side="left", padx=6)
        ttk.Button(bar, text="Actualizar", command=self.refresh_ports).pack(side="left")
        self.conn_btn = ttk.Button(bar, text="Conectar", command=self.toggle_connection)
        self.conn_btn.pack(side="left", padx=6)
        self.conn_lbl = ttk.Label(bar, text="Desconectado", foreground="#a00")
        self.conn_lbl.pack(side="right")

        # --- pestanas ---
        nb = ttk.Notebook(self)
        nb.pack(fill="both", expand=True, padx=8, pady=(0, 4))
        self._build_config_tab(nb)
        self._build_entries_tab(nb)
        self._build_log_tab(nb)
        self._build_console_tab(nb)

        # --- barra de estado ---
        self.status = tk.StringVar(value="Listo.")
        ttk.Label(self, textvariable=self.status, relief="sunken", anchor="w",
                  padding=(6, 2)).pack(fill="x", side="bottom")

    # ---- Configuracion ----
    def _build_config_tab(self, nb):
        tab = ttk.Frame(nb, padding=12)
        nb.add(tab, text="Configuración")

        # Clave
        f = ttk.LabelFrame(tab, text="Clave del sistema (4 dígitos)", padding=10)
        f.pack(fill="x", pady=(0, 10))
        self.old_pass = tk.StringVar()
        self.new_pass = tk.StringVar()
        self.new_pass2 = tk.StringVar()
        for i, (txt, var) in enumerate((("Clave actual:", self.old_pass),
                                        ("Clave nueva:", self.new_pass),
                                        ("Confirmar nueva:", self.new_pass2))):
            ttk.Label(f, text=txt).grid(row=i, column=0, sticky="w", pady=2)
            e = self._reg(ttk.Entry(f, textvariable=var, show="*", width=10))
            e.grid(row=i, column=1, padx=8, pady=2, sticky="w")
        self._reg(ttk.Button(f, text="Cambiar clave", command=self.change_password)
                  ).grid(row=1, column=2, padx=12)

        # Tiempos
        f = ttk.LabelFrame(tab, text="Tiempos (segundos)", padding=10)
        f.pack(fill="x", pady=(0, 10))
        self.arm_var = tk.IntVar(value=20)
        self.disarm_var = tk.IntVar(value=20)
        rows = (("Tiempo de armado (salida):", self.arm_var, "ARMADO"),
                ("Tiempo de desarmado (entrada):", self.disarm_var, "DESARMADO"))
        for i, (txt, var, cmd) in enumerate(rows):
            ttk.Label(f, text=txt).grid(row=i, column=0, sticky="w", pady=4)
            sb = self._reg(ttk.Spinbox(f, from_=TIME_MIN, to=TIME_MAX, textvariable=var, width=6))
            sb.grid(row=i, column=1, padx=8)
            self._reg(ttk.Button(f, text="Leer", command=lambda c=cmd, v=var: self.read_time_param(c, v))
                      ).grid(row=i, column=2, padx=2)
            self._reg(ttk.Button(f, text="Aplicar", command=lambda c=cmd, v=var: self.write_time_param(c, v))
                      ).grid(row=i, column=3, padx=2)
        ttk.Label(f, text=f"Rango permitido: {TIME_MIN} a {TIME_MAX} s",
                  foreground="#666").grid(row=2, column=0, columnspan=4, sticky="w")

        # Reloj
        f = ttk.LabelFrame(tab, text="Reloj del sistema", padding=10)
        f.pack(fill="x")
        ttk.Label(f, text="Hora del dispositivo:").grid(row=0, column=0, sticky="w")
        self.clock_var = tk.StringVar(value="—")
        ttk.Label(f, textvariable=self.clock_var, font=("TkDefaultFont", 10, "bold")
                  ).grid(row=0, column=1, padx=8, sticky="w")
        self._reg(ttk.Button(f, text="Leer", command=self.read_clock)).grid(row=0, column=2, padx=2)
        self._reg(ttk.Button(f, text="Sincronizar con la PC", command=self.sync_clock)
                  ).grid(row=0, column=3, padx=2)
        ttk.Label(f, text="Ajuste manual:").grid(row=1, column=0, sticky="w", pady=(8, 0))
        self.manual_time = tk.StringVar(value=datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
        self._reg(ttk.Entry(f, textvariable=self.manual_time, width=22)
                  ).grid(row=1, column=1, padx=8, pady=(8, 0), sticky="w")
        self._reg(ttk.Button(f, text="Ajustar", command=self.set_clock_manual)
                  ).grid(row=1, column=2, pady=(8, 0))
        ttk.Label(f, text="Formato: AAAA-MM-DD HH:MM:SS. La hora se pierde al quitar la alimentación.",
                  foreground="#666").grid(row=2, column=0, columnspan=4, sticky="w", pady=(6, 0))

    # ---- Entradas ----
    def _build_entries_tab(self, nb):
        tab = ttk.Frame(nb, padding=12)
        nb.add(tab, text="Entradas")

        tb = ttk.Frame(tab)
        tb.pack(fill="x", pady=(0, 6))
        self._reg(ttk.Button(tb, text="Actualizar (LIST)", command=self.refresh_list)).pack(side="left")
        self._reg(ttk.Button(tb, text="Nueva…", command=self.new_entry)).pack(side="left", padx=(12, 2))
        self._reg(ttk.Button(tb, text="Renombrar…", command=self.mod_entry)).pack(side="left", padx=2)
        self._reg(ttk.Button(tb, text="Eliminar", command=self.del_entry)).pack(side="left", padx=2)
        self.auto_var = tk.BooleanVar(value=False)
        self._reg(ttk.Checkbutton(tb, text="Actualizar automáticamente", variable=self.auto_var)
                  ).pack(side="right")

        cols = ("id", "nombre", "estado")
        self.tree_entries = ttk.Treeview(tab, columns=cols, show="headings", height=10,
                                         selectmode="browse")
        for c, txt, w in (("id", "ID", 60), ("nombre", "Nombre", 260), ("estado", "Estado", 120)):
            self.tree_entries.heading(c, text=txt)
            self.tree_entries.column(c, width=w, anchor="center" if c != "nombre" else "w")
        self.tree_entries.tag_configure("ABIERTO", foreground="#b00020")
        self.tree_entries.tag_configure("CERRADO", foreground="#1b7a34")
        self.tree_entries.pack(fill="both", expand=True)
        ttk.Label(tab, text="El ID es el bit del PCF8574 al que está conectado el sensor (0–7). "
                            f"Máximo 8 entradas; nombres de hasta {NAME_MAX} caracteres.",
                  foreground="#666", wraplength=700).pack(anchor="w", pady=(6, 0))

    # ---- Bitacora ----
    def _build_log_tab(self, nb):
        tab = ttk.Frame(nb, padding=12)
        nb.add(tab, text="Bitácora de alarmas")

        tb = ttk.Frame(tab)
        tb.pack(fill="x", pady=(0, 6))
        self._reg(ttk.Button(tb, text="Actualizar (LOG alarm)", command=self.refresh_log)).pack(side="left")
        self._reg(ttk.Button(tb, text="Exportar CSV…", command=self.export_log)).pack(side="left", padx=8)

        cols = ("n", "id", "nombre", "fecha")
        self.tree_log = ttk.Treeview(tab, columns=cols, show="headings", height=12)
        for c, txt, w, anc in (("n", "#", 50, "center"), ("id", "ID", 60, "center"),
                               ("nombre", "Entrada", 200, "w"), ("fecha", "Fecha y hora", 220, "center")):
            self.tree_log.heading(c, text=txt)
            self.tree_log.column(c, width=w, anchor=anc)
        sb = ttk.Scrollbar(tab, orient="vertical", command=self.tree_log.yview)
        self.tree_log.configure(yscrollcommand=sb.set)
        self.tree_log.pack(side="left", fill="both", expand=True)
        sb.pack(side="left", fill="y")

    # ---- Consola ----
    def _build_console_tab(self, nb):
        tab = ttk.Frame(nb, padding=12)
        nb.add(tab, text="Consola")

        self.console = tk.Text(tab, height=14, state="disabled", wrap="none",
                               font=("Courier", 10))
        sb = ttk.Scrollbar(tab, orient="vertical", command=self.console.yview)
        self.console.configure(yscrollcommand=sb.set)
        self.console.tag_configure("tx", foreground="#0b5394")
        self.console.tag_configure("rx", foreground="#222")
        bottom = ttk.Frame(tab)
        bottom.pack(side="bottom", fill="x", pady=(6, 0))
        self.raw_var = tk.StringVar()
        e = self._reg(ttk.Entry(bottom, textvariable=self.raw_var))
        e.pack(side="left", fill="x", expand=True)
        e.bind("<Return>", lambda _e: self.send_raw())
        self._reg(ttk.Button(bottom, text="Enviar", command=self.send_raw)).pack(side="left", padx=6)
        ttk.Button(bottom, text="Limpiar", command=self.clear_console).pack(side="left")
        self.console.pack(side="left", fill="both", expand=True)
        sb.pack(side="left", fill="y")

    # ------------------------------------------------------- utilidades --
    def _traffic(self, direction, text):
        # Llamado desde hilos de trabajo -> pasar al hilo de la UI
        self._post(self._append_console, direction, text)

    def _append_console(self, direction, text):
        self.console.configure(state="normal")
        self.console.insert("end", f"{direction} {text}\n", "tx" if direction == ">>" else "rx")
        self.console.see("end")
        self.console.configure(state="disabled")

    def clear_console(self):
        self.console.configure(state="normal")
        self.console.delete("1.0", "end")
        self.console.configure(state="disabled")

    def _set_status(self, text):
        self.status.set(text)

    def _set_connected(self, connected):
        for w in self._controls:
            try:
                w.state(["!disabled"] if connected else ["disabled"])
            except tk.TclError:
                pass
        self.conn_btn.configure(text="Desconectar" if connected else "Conectar")
        self.conn_lbl.configure(text="Conectado" if connected else "Desconectado",
                                foreground="#1b7a34" if connected else "#a00")

    def run(self, func, done=None, what="la operación", silent=False):
        """Ejecuta func() en un hilo y entrega el resultado en el hilo de la UI."""
        self.busy += 1

        def worker():
            try:
                res, err = func(), None
            except Exception as exc:                      # noqa: BLE001
                res, err = None, exc
            self._post(self._finish, res, err, done, what, silent)

        threading.Thread(target=worker, daemon=True).start()

    def _finish(self, res, err, done, what, silent):
        self.busy -= 1
        if err is not None:
            self._handle_error(err, what, silent)
            return
        if done:
            done(res)

    def _handle_error(self, err, what, silent):
        if isinstance(err, serial.SerialException):
            self._disconnect()
            if not silent:
                messagebox.showerror("Puerto serial", f"Se perdió la comunicación:\n{err}")
            return
        if silent:
            return
        if isinstance(err, DeviceError):
            self._set_status(f"El dispositivo rechazó {what} (ERROR).")
            messagebox.showerror("Comando rechazado",
                                 f"El dispositivo respondió ERROR al intentar {what}.\n"
                                 "Verifica los datos (clave, rangos, entrada existente, etc.).")
        elif isinstance(err, TimeoutError):
            self._set_status("Sin respuesta del dispositivo.")
            messagebox.showwarning("Sin respuesta",
                                   f"El dispositivo no respondió al intentar {what}.")
        else:
            messagebox.showerror("Error", str(err))

    # ------------------------------------------------------- conexion ----
    def refresh_ports(self):
        ports = list(list_ports.comports())
        self._ports = {f"{p.device} — {p.description}": p.device for p in ports}
        self.port_cb["values"] = list(self._ports)
        if self._ports and not self.port_var.get():
            self.port_cb.current(0)

    def toggle_connection(self):
        if self.client.is_open:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        label = self.port_var.get()
        if not label:
            messagebox.showinfo("Puerto", "Selecciona un puerto serial.")
            return
        port = self._ports.get(label, label)
        self.conn_btn.state(["disabled"])
        self._set_status(f"Conectando a {port}…")

        def work():
            self.client.open(port)
            return self._read_everything()

        def done(data):
            self.conn_btn.state(["!disabled"])
            self._set_connected(True)
            self._apply_all(data)
            self._set_status(f"Conectado a {port}.")

        def fail(err):
            self.conn_btn.state(["!disabled"])
            self.client.close()
            self._set_connected(False)
            messagebox.showerror("No se pudo conectar", str(err))
            self._set_status("Desconectado.")

        self.busy += 1

        def worker():
            try:
                res, err = work(), None
            except Exception as exc:                      # noqa: BLE001
                res, err = None, exc
            self._post(self._connect_finished, res, err, done, fail)

        threading.Thread(target=worker, daemon=True).start()

    def _connect_finished(self, res, err, done, fail):
        self.busy -= 1
        if err is not None:
            fail(err)
        else:
            done(res)

    def _disconnect(self):
        self.client.close()
        self._set_connected(False)
        self._set_status("Desconectado.")

    def _on_close(self):
        self.client.close()
        self.destroy()

    # ------------------------------------------------ lectura completa ---
    def _read_everything(self):
        return {
            "arm": self.client.request("ARMADO", "value"),
            "disarm": self.client.request("DESARMADO", "value"),
            "clock": self.client.request("TIME", "value"),
            "list": self.client.request("LIST"),
            "log": self.client.request("LOG alarm"),
        }

    def _apply_all(self, d):
        self._set_int(self.arm_var, d["arm"])
        self._set_int(self.disarm_var, d["disarm"])
        self.clock_var.set(d["clock"])
        self._fill_entries(d["list"])
        self._fill_log(d["log"])

    @staticmethod
    def _set_int(var, text):
        try:
            var.set(int(text))
        except ValueError:
            pass

    # ------------------------------------------------- configuracion -----
    def change_password(self):
        old, new, new2 = self.old_pass.get(), self.new_pass.get(), self.new_pass2.get()
        for p in (old, new):
            if not (len(p) == 4 and p.isdigit() and p.isascii()):
                messagebox.showwarning("Clave", "La clave debe tener exactamente 4 dígitos (0–9).")
                return
        if new != new2:
            messagebox.showwarning("Clave", "La confirmación no coincide con la clave nueva.")
            return

        def done(_):
            for v in (self.old_pass, self.new_pass, self.new_pass2):
                v.set("")
            self._set_status("Clave cambiada correctamente.")
            messagebox.showinfo("Clave", "La clave se cambió correctamente.")

        self.run(lambda: self.client.request(f"PASS {old} {new}"), done, "cambiar la clave")

    def read_time_param(self, cmd, var):
        def done(text):
            self._set_int(var, text)
            self._set_status(f"{cmd}: {text} s")
        self.run(lambda: self.client.request(cmd, "value"), done, f"leer {cmd}")

    def write_time_param(self, cmd, var):
        try:
            value = int(var.get())
        except (tk.TclError, ValueError):
            messagebox.showwarning("Valor inválido", "Ingresa un número entero.")
            return
        if not TIME_MIN <= value <= TIME_MAX:
            messagebox.showwarning("Fuera de rango", f"El valor debe estar entre {TIME_MIN} y {TIME_MAX} s.")
            return
        self.run(lambda: self.client.request(f"{cmd} {value}"),
                 lambda _: self._set_status(f"{cmd} ajustado a {value} s."),
                 f"ajustar {cmd}")

    def read_clock(self):
        self.run(lambda: self.client.request("TIME", "value"),
                 lambda t: self.clock_var.set(t), "leer la hora")

    def _send_time(self, text):
        def work():
            self.client.request(f"TIME {text}")
            return self.client.request("TIME", "value")

        def done(now):
            self.clock_var.set(now)
            self._set_status("Reloj ajustado.")
        self.run(work, done, "ajustar la hora")

    def sync_clock(self):
        self._send_time(datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

    def set_clock_manual(self):
        text = self.manual_time.get().strip()
        try:
            dt = datetime.datetime.strptime(text, "%Y-%m-%d %H:%M:%S")
        except ValueError:
            messagebox.showwarning("Formato", "Usa el formato AAAA-MM-DD HH:MM:SS.")
            return
        if not 2000 <= dt.year <= 2099:
            messagebox.showwarning("Año", "El año debe estar entre 2000 y 2099.")
            return
        self._send_time(dt.strftime("%Y-%m-%d %H:%M:%S"))

    # ---------------------------------------------------- entradas -------
    def _fill_entries(self, lines):
        self.tree_entries.delete(*self.tree_entries.get_children())
        for id_, name, state in parse_table(lines, 3):
            self.tree_entries.insert("", "end", values=(id_, name, state), tags=(state,))

    def refresh_list(self, silent=False):
        self.run(lambda: self.client.request("LIST"), self._fill_entries,
                 "consultar las entradas", silent=silent)

    def _selected_entry(self):
        sel = self.tree_entries.selection()
        if not sel:
            messagebox.showinfo("Entradas", "Selecciona una entrada de la lista.")
            return None
        return self.tree_entries.item(sel[0], "values")

    def _ask_name(self, title, initial=""):
        name = simpledialog.askstring(title, f"Nombre (máx. {NAME_MAX} caracteres):",
                                      initialvalue=initial, parent=self)
        if name is None:
            return None
        name = name.strip()
        if not name or len(name) > NAME_MAX or not name.isascii() or "|" in name:
            messagebox.showwarning("Nombre inválido",
                                   f"El nombre debe tener de 1 a {NAME_MAX} caracteres ASCII (sin '|').")
            return None
        return name

    def _after_change(self, msg):
        self._set_status(msg)
        self.refresh_list()

    def new_entry(self):
        name = self._ask_name("Nueva entrada")
        if name:
            self.run(lambda: self.client.request(f"NEW {name}"),
                     lambda _: self._after_change(f"Entrada '{name}' creada."),
                     "crear la entrada (¿ya hay 8?)")

    def mod_entry(self):
        sel = self._selected_entry()
        if not sel:
            return
        id_, old_name = sel[0], sel[1]
        name = self._ask_name("Renombrar entrada", old_name)
        if name:
            self.run(lambda: self.client.request(f"MOD {id_} {name}"),
                     lambda _: self._after_change(f"Entrada {id_} renombrada a '{name}'."),
                     "renombrar la entrada")

    def del_entry(self):
        sel = self._selected_entry()
        if not sel:
            return
        id_, name = sel[0], sel[1]
        if messagebox.askyesno("Eliminar", f"¿Eliminar la entrada {id_} ('{name}')?"):
            self.run(lambda: self.client.request(f"DEL {id_}"),
                     lambda _: self._after_change(f"Entrada {id_} eliminada."),
                     "eliminar la entrada")

    def _auto_tick(self):
        if self.client.is_open and self.auto_var.get() and self.busy == 0:
            self.refresh_list(silent=True)
        self.after(AUTO_REFRESH_MS, self._auto_tick)

    # ---------------------------------------------------- bitacora -------
    def _fill_log(self, lines):
        self.tree_log.delete(*self.tree_log.get_children())
        for row in parse_table(lines, 4):
            self.tree_log.insert("", "end", values=row)

    def refresh_log(self):
        def done(lines):
            self._fill_log(lines)
            self._set_status(f"Bitácora: {len(self.tree_log.get_children())} registro(s).")
        self.run(lambda: self.client.request("LOG alarm"), done, "consultar la bitácora")

    def export_log(self):
        rows = [self.tree_log.item(i, "values") for i in self.tree_log.get_children()]
        if not rows:
            messagebox.showinfo("Exportar", "La bitácora está vacía. Actualízala primero.")
            return
        path = filedialog.asksaveasfilename(defaultextension=".csv",
                                            filetypes=[("CSV", "*.csv")],
                                            initialfile="bitacora_alarmas.csv")
        if path:
            with open(path, "w", newline="", encoding="utf-8") as f:
                w = csv.writer(f)
                w.writerow(["#", "ID", "Entrada", "Fecha y hora"])
                w.writerows(rows)
            self._set_status(f"Bitácora exportada a {path}")

    # ---------------------------------------------------- consola --------
    def send_raw(self):
        cmd = self.raw_var.get().strip()
        if not cmd:
            return
        self.raw_var.set("")
        first = cmd.split()[0].upper()
        kind = "value" if first in ("ARMADO", "DESARMADO", "TIME") and len(cmd.split()) == 1 else "ok"
        self.run(lambda: self.client.request(cmd, kind), None, f"ejecutar '{cmd}'")


if __name__ == "__main__":
    App().mainloop()
