#!/usr/bin/env python3
"""Зафицировать FIT-CSS в HTML-превьях: 800x480 + восстановленная шапка."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "preview"
MARK = "/* FIT: жёстко удерживаем 800x480"

FIT = """/* FIT: жёстко удерживаем 800x480, без съезда вправо/вниз */
.frame{grid-template-columns:minmax(0,1fr);overflow:hidden}
.frame>*{min-width:0;min-height:0}
.s1-body,.s2-body,.body{min-width:0;min-height:0;overflow:hidden}
.s1-body,.s1-body *, .s2-body,.s2-body *, .body,.body *{min-width:0}
.s1-body{grid-template-rows:170px minmax(0,1fr)}
.s2-body{grid-template-rows:minmax(0,1fr) 30px}
.body{grid-template-rows:170px minmax(0,1fr)}
.chart{margin-top:6px!important}
/* HEADER FIX: вернуть «ГОТОВ», часы, ? / fx и индикаторы на место */
.header{grid-template-columns:1fr auto 1fr;gap:8px}
.ready{font-size:15px;white-space:nowrap}
.title{font-size:26px;letter-spacing:2px;white-space:nowrap}
.stage{min-width:140px;padding:6px 12px;font-size:15px;white-space:nowrap}
.clock{font-size:15px;min-width:64px;white-space:nowrap;font-variant-numeric:tabular-nums}
.ovico{flex:0 0 22px;width:22px;min-width:22px}
.ico{flex:0 0 24px;width:24px;min-width:24px}
.head-right{gap:10px;min-width:max-content;flex:0 0 auto}
"""

for name in ["hmi_preview.html", "hmi_modes.html"]:
    p = ROOT / name
    text = p.read_text(encoding="utf-8")
    start = text.find(MARK)
    if start >= 0:
        end = text.find("</style>", start)
        if end < 0:
            raise SystemExit(f"не найден </style> после FIT в {name}")
        text = text[:start] + FIT + text[end:]
    else:
        idx = text.find("</style>")
        if idx < 0:
            raise SystemExit(f"нет </style> в {name}")
        text = text[:idx] + FIT + text[idx:]
    p.write_text(text, encoding="utf-8")
    print("обновлён:", name)
