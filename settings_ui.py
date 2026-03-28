"""Simple settings window for email and language configuration."""

import tkinter as tk
from tkinter import messagebox

from settings_store import load_user_settings, save_user_settings


def open_settings_window() -> None:
    settings = load_user_settings()

    root = tk.Tk()
    root.title("SS-EXT Settings")
    root.geometry("460x440")
    root.resizable(False, False)

    frame = tk.Frame(root, padx=16, pady=16)
    frame.pack(fill="both", expand=True)

    tk.Label(frame, text="Language").grid(row=0, column=0, sticky="w")
    language_var = tk.StringVar(value=str(settings.get("SSEXT_LANG", "en")))
    lang_menu = tk.OptionMenu(frame, language_var, "en", "tr")
    lang_menu.grid(row=0, column=1, sticky="ew", padx=(8, 0), pady=(0, 8))

    tk.Label(frame, text="Email Address").grid(row=1, column=0, sticky="w")
    email_var = tk.StringVar(value=str(settings.get("SSEXT_EMAIL_ADDRESS", "")))
    tk.Entry(frame, textvariable=email_var, width=42).grid(row=1, column=1, sticky="ew", padx=(8, 0), pady=(0, 8))

    tk.Label(frame, text="Email Password").grid(row=2, column=0, sticky="w")
    password_var = tk.StringVar(value=str(settings.get("SSEXT_EMAIL_PASSWORD", "")))
    tk.Entry(frame, textvariable=password_var, show="*", width=42).grid(row=2, column=1, sticky="ew", padx=(8, 0), pady=(0, 8))

    tk.Label(frame, text="IMAP Server").grid(row=3, column=0, sticky="w")
    imap_server_var = tk.StringVar(value=str(settings.get("SSEXT_IMAP_SERVER", "")))
    tk.Entry(frame, textvariable=imap_server_var, width=42).grid(row=3, column=1, sticky="ew", padx=(8, 0), pady=(0, 8))

    tk.Label(frame, text="IMAP Port").grid(row=4, column=0, sticky="w")
    imap_port_var = tk.StringVar(value=str(settings.get("SSEXT_IMAP_PORT", "993")))
    tk.Entry(frame, textvariable=imap_port_var, width=42).grid(row=4, column=1, sticky="ew", padx=(8, 0), pady=(0, 8))

    tk.Label(frame, text="SMTP Server").grid(row=5, column=0, sticky="w")
    smtp_server_var = tk.StringVar(value=str(settings.get("SSEXT_SMTP_SERVER", "")))
    tk.Entry(frame, textvariable=smtp_server_var, width=42).grid(row=5, column=1, sticky="ew", padx=(8, 0), pady=(0, 8))

    tk.Label(frame, text="SMTP Port").grid(row=6, column=0, sticky="w")
    smtp_port_var = tk.StringVar(value=str(settings.get("SSEXT_SMTP_PORT", "587")))
    tk.Entry(frame, textvariable=smtp_port_var, width=42).grid(row=6, column=1, sticky="ew", padx=(8, 0), pady=(0, 8))

    imap_ssl_var = tk.BooleanVar(value=bool(settings.get("SSEXT_IMAP_SSL", True)))
    tk.Checkbutton(frame, text="Use IMAP SSL", variable=imap_ssl_var).grid(row=7, column=1, sticky="w", padx=(8, 0), pady=(0, 8))

    smtp_tls_var = tk.BooleanVar(value=bool(settings.get("SSEXT_SMTP_STARTTLS", True)))
    tk.Checkbutton(frame, text="Use SMTP STARTTLS", variable=smtp_tls_var).grid(row=8, column=1, sticky="w", padx=(8, 0), pady=(0, 8))

    enabled_var = tk.BooleanVar(value=bool(settings.get("SSEXT_EMAIL_ENABLED", True)))
    tk.Checkbutton(frame, text="Enable Email Notifications", variable=enabled_var).grid(row=9, column=1, sticky="w", padx=(8, 0), pady=(0, 8))

    check_interval_var = tk.StringVar(value=str(settings.get("SSEXT_EMAIL_CHECK_INTERVAL", "10")))
    tk.Label(frame, text="Email Check Interval (s)").grid(row=10, column=0, sticky="w")
    tk.Entry(frame, textvariable=check_interval_var, width=42).grid(row=10, column=1, sticky="ew", padx=(8, 0), pady=(0, 8))

    display_duration_var = tk.StringVar(value=str(settings.get("SSEXT_EMAIL_DISPLAY_DURATION", "10")))
    tk.Label(frame, text="Email Display Duration (s)").grid(row=11, column=0, sticky="w")
    tk.Entry(frame, textvariable=display_duration_var, width=42).grid(row=11, column=1, sticky="ew", padx=(8, 0), pady=(0, 8))

    frame.columnconfigure(1, weight=1)

    def on_save() -> None:
        try:
            imap_port = int(imap_port_var.get().strip())
            smtp_port = int(smtp_port_var.get().strip())
            check_interval = int(check_interval_var.get().strip())
            display_duration = int(display_duration_var.get().strip())
        except ValueError:
            messagebox.showerror("Invalid Input", "Port and interval fields must be numeric.")
            return

        if check_interval <= 0 or display_duration <= 0:
            messagebox.showerror("Invalid Input", "Intervals must be greater than 0.")
            return

        new_settings = {
            "SSEXT_LANG": language_var.get().strip().lower() or "en",
            "SSEXT_EMAIL_ADDRESS": email_var.get().strip(),
            "SSEXT_EMAIL_PASSWORD": password_var.get(),
            "SSEXT_IMAP_SERVER": imap_server_var.get().strip(),
            "SSEXT_IMAP_PORT": imap_port,
            "SSEXT_IMAP_SSL": bool(imap_ssl_var.get()),
            "SSEXT_SMTP_SERVER": smtp_server_var.get().strip(),
            "SSEXT_SMTP_PORT": smtp_port,
            "SSEXT_SMTP_STARTTLS": bool(smtp_tls_var.get()),
            "SSEXT_EMAIL_ENABLED": bool(enabled_var.get()),
            "SSEXT_EMAIL_CHECK_INTERVAL": check_interval,
            "SSEXT_EMAIL_DISPLAY_DURATION": display_duration,
        }

        path = save_user_settings(new_settings)
        messagebox.showinfo("Saved", f"Settings saved to: {path}")

    btn_frame = tk.Frame(frame)
    btn_frame.grid(row=12, column=0, columnspan=2, sticky="e", pady=(12, 0))

    tk.Button(btn_frame, text="Save", command=on_save, width=12).pack(side="right")
    tk.Button(btn_frame, text="Close", command=root.destroy, width=12).pack(side="right", padx=(0, 8))

    root.mainloop()
