"""Тёмная тема конфигуратора: палитра совпадает с PWA и приложением."""

from __future__ import annotations

import tkinter as tk
from tkinter import ttk

BG = "#070b11"
PANEL = "#111a26"
CARD = "#1d2735"
LINE = "#2a3a50"
TEXT = "#ffffff"
MUTED = "#c8d6e8"
ACCENT = "#2f8cff"
HOVER = "#26344a"


def apply_theme(root: tk.Misc) -> None:
    style = ttk.Style(root)
    style.theme_use("clam")

    root.configure(background=BG)
    style.configure(".", background=BG, foreground=TEXT, bordercolor=LINE,
                    lightcolor=LINE, darkcolor=LINE, troughcolor=CARD,
                    fieldbackground=CARD, insertcolor=TEXT,
                    selectbackground=ACCENT, selectforeground=TEXT)
    style.configure("TFrame", background=BG)
    style.configure("TLabel", background=BG, foreground=TEXT)
    style.configure("TButton", background=CARD, foreground=TEXT,
                    bordercolor=LINE, padding=(12, 6), relief="flat")
    style.map("TButton",
              background=[("disabled", PANEL), ("active", HOVER)],
              foreground=[("disabled", MUTED)])
    style.configure("TEntry", fieldbackground=CARD, foreground=TEXT,
                    bordercolor=LINE, padding=4)
    style.configure("TCombobox", fieldbackground=CARD, foreground=TEXT,
                    background=CARD, arrowcolor=TEXT, bordercolor=LINE)
    style.map("TCombobox",
              fieldbackground=[("readonly", CARD)],
              foreground=[("readonly", TEXT)],
              selectbackground=[("readonly", CARD)])
    style.configure("TCheckbutton", background=BG, foreground=TEXT)
    style.map("TCheckbutton", background=[("active", BG)])
    style.configure("TNotebook", background=BG, bordercolor=LINE, tabmargins=(2, 4, 2, 0))
    style.configure("TNotebook.Tab", background=CARD, foreground=MUTED,
                    padding=(14, 6), bordercolor=LINE)
    style.map("TNotebook.Tab",
              background=[("selected", ACCENT), ("active", HOVER)],
              foreground=[("selected", TEXT)])
    style.configure("Horizontal.TProgressbar", background=ACCENT,
                    troughcolor=CARD, bordercolor=LINE, lightcolor=ACCENT, darkcolor=ACCENT)

    root.option_add("*Text.background", PANEL)
    root.option_add("*Text.foreground", TEXT)
    root.option_add("*Text.insertBackground", TEXT)
    root.option_add("*Text.selectBackground", ACCENT)
    root.option_add("*Text.relief", "flat")
    root.option_add("*Text.borderWidth", 0)
    root.option_add("*TCombobox*Listbox.background", CARD)
    root.option_add("*TCombobox*Listbox.foreground", TEXT)
    root.option_add("*TCombobox*Listbox.selectBackground", ACCENT)