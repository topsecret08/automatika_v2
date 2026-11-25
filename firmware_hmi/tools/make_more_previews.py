#!/usr/bin/env python3
"""Собрать hmi_more.html с пятью дополнительными превью в стиле hmi_preview.html."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREVIEW = ROOT / "preview"
src = (PREVIEW / "hmi_preview.html").read_text(encoding="utf-8")
prefix = src.split("<!-- ================= ЭКРАН 1 ================= -->")[0]

EXTRA_CSS = """
<style>
body{padding:0}
/* FIT: жёстко удерживаем 800x480, без съезда вправо/вниз */
.frame{grid-template-columns:minmax(0,1fr);overflow:hidden}
.frame>*{min-width:0;min-height:0}
.content,.content *, .stack,.stack *{min-width:0}
.content{min-height:0;overflow:hidden;grid-template-rows:minmax(0,1fr) 30px}
/* HEADER FIX: вернуть «ГОТОВ», часы, ? / fx и индикаторы на место */
.header{grid-template-columns:1fr auto 1fr;gap:8px}
.ready{font-size:15px;white-space:nowrap}
.title{font-size:26px;letter-spacing:2px;white-space:nowrap}
.stage{min-width:140px;padding:6px 12px;font-size:15px;white-space:nowrap}
.clock{font-size:15px;min-width:64px;white-space:nowrap;font-variant-numeric:tabular-nums}
.ovico{flex:0 0 22px;width:22px;min-width:22px}
.ico{flex:0 0 24px;width:24px;min-width:24px}
.head-right{gap:10px;min-width:max-content;flex:0 0 auto}
.chart{margin-top:6px!important}
.steprow{gap:8px}
.step{padding:10px}
.step .sv{font-size:28px}
.content .top3{margin-top:8px}
.stack{display:flex;flex-direction:column;gap:10px;min-height:0;overflow:hidden}
.stack>*{min-height:0}
.tiles{grid-template-columns:repeat(4,minmax(0,1fr))}
.tile>div:last-child{min-width:0;overflow:hidden}
.chart{margin-top:4px!important}
.content{display:grid;grid-template-rows:1fr 30px;gap:10px;min-height:0}
.tiles{display:grid;grid-template-columns:repeat(4,1fr);grid-template-rows:1fr 1fr;gap:12px;min-height:0}
.tile{
  position:relative;padding:14px 16px;display:grid;grid-template-columns:54px 1fr;
  align-items:center;gap:12px;border-radius:16px;
  background:linear-gradient(145deg,rgba(15,30,54,.92),rgba(8,18,36,.94));
  border:1px solid rgba(95,145,195,.2);
}
.tile.active{border:2px solid var(--cyan);box-shadow:0 0 26px rgba(34,211,238,.55),inset 0 0 26px rgba(34,211,238,.16)}
.tile .tbig{font-size:34px;font-weight:800;color:var(--cyan);text-align:center;line-height:1}
.tile .tname{font-size:15px;font-weight:800;letter-spacing:.2px;line-height:1.05;white-space:normal;overflow-wrap:anywhere}
.tile .tsub{font-size:12px;color:var(--muted);margin-top:6px;letter-spacing:.3px}
.tile .tval{font-size:15px;color:var(--yellow);margin-top:6px;font-weight:650}
.top3{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;align-items:stretch}
.kcard{padding:14px 18px;display:flex;flex-direction:column;justify-content:space-between}
.kcard h3{margin:0;font-size:14px;letter-spacing:2px;color:var(--muted)}
.kcard .kv{font-size:46px;font-weight:800;line-height:1;margin:8px 0 4px}
.kcard .ku{font-size:13px;color:var(--muted)}
.steprow{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;align-items:stretch}
.step{padding:14px;display:flex;flex-direction:column;gap:6px;justify-content:center;text-align:center}
.step.done{border-color:rgba(57,255,136,.55);box-shadow:0 0 18px rgba(57,255,136,.18)}
.step.now{border-color:var(--cyan);box-shadow:0 0 22px rgba(34,211,238,.42)}
.step .st{font-size:12px;color:var(--muted);letter-spacing:1px}
.step .sv{font-size:34px;font-weight:800}
.step .sd{font-size:12px;color:var(--yellow)}
.helplay{display:grid;grid-template-columns:250px 1fr;gap:12px;min-height:0}
.hnav{display:flex;flex-direction:column;gap:8px;padding:12px;overflow:hidden}
.hnav div{padding:12px 14px;border-radius:10px;background:rgba(20,42,74,.55);border:1px solid rgba(100,205,255,.18);font-size:15px}
.hnav div.on{background:linear-gradient(145deg,rgba(22,58,102,.94),rgba(9,28,56,.94));border-color:var(--cyan);box-shadow:0 0 18px rgba(34,211,238,.35)}
.hart{padding:18px 22px;overflow:hidden}
.hart h2{margin:0 0 12px;font-size:25px;letter-spacing:1.5px}
.hart p{margin:0 0 10px;font-size:14px;line-height:1.55;color:#C9DDF2}
.hart code{color:var(--yellow);font-size:13px}
.calcs{display:grid;grid-template-columns:1fr 1fr;grid-template-rows:1fr 1fr;gap:12px}
.calc{padding:14px 18px;display:grid;grid-template-rows:auto 1fr auto;gap:8px}
.calc h3{margin:0;font-size:15px;letter-spacing:1.6px;color:var(--muted)}
.calc .formula{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:14px;color:var(--cyan)}
.calc .res{font-size:32px;font-weight:800;color:var(--text);text-align:right}
.calc .hint{font-size:12px;color:var(--muted)}
</style>
"""

FOOTER = '<div class="footer">● ВХОД  •  УДЕРЖ. 2с — НАЗАД  •  ЭНКОДЕР — НАВИГАЦИЯ</div>'


def header(title, stage, ready="ГОТОВ"):
    return f"""<div class="header glass">
    <div class="ready"><span class="dot"></span>{ready}</div>
    <div class="title">{title}</div>
    <div class="head-right">
      <span class="stage">{stage}</span>
      <span class="clock" data-clock>--:--:--</span>
      <span class="ovico" title="Справка">?</span>
      <span class="ovico" title="Калькуляторы">fx</span>
      <svg class="ico" viewBox="0 0 24 24"><use href="#i-wifi"/></svg>
      <svg class="ico" viewBox="0 0 24 24"><use href="#i-thermo"/></svg>
    </div>
  </div>"""


def screen(sid, title, stage, body, ready="ГОТОВ", on=False):
    cls = "screen on" if on else "screen"
    return f"""<!-- ============ {title} ============ -->
<div class="{cls}" id="{sid}">
<div class="frame">
{header(title, stage, ready)}
<div class="content">
{body}
{FOOTER}
</div>
</div>
</div>
"""


def tile(tbig, name, sub, val, icon, active=False):
    cls = "tile active" if active else "tile"
    return f"""<div class="{cls}">
    <div class="tbig"><svg class="ico" style="width:34px;height:34px"><use href="#{icon}"/></svg></div>
    <div><div class="tname">{name}</div><div class="tsub">{sub}</div><div class="tval">{val}</div></div>
  </div>"""


modes_body = '<div class="tiles">' + "\n".join([
    tile("▶", "ДИСТИЛЛЯЦИЯ", "простой перегон", "T1 78.4°", "i-play", True),
    tile("◈", "РЕКТИФИКАЦИЯ", "колонна", "T2 80.0°", "i-list"),
    tile("▣", "ПИВО", "затор / варка", "65.0°", "i-thermo"),
    tile("⚙", "ПВК", "поправка сахара", "1.042", "i-gear"),
    tile("◉", "ЗАТОР", "температурные паузы", "72.0°", "i-thermo"),
    tile("✎", "НАСТОЙ", "мацерация", "12 ч", "i-pencil"),
    tile("?", "СПРАВКА", "безопасность / глоссарий", "RU", "i-warn"),
    tile("fx", "КАЛЬКУЛЯТОРЫ", "крепость / поток", "4 режима", "i-list"),
]) + "</div>"

distill_body = """
<div class="stack">
<div class="top3">
  <div class="kcard glass"><h3>КУБ</h3><div class="kv">92.4<sup style="font-size:22px">°</sup></div><div class="ku">УСТ 95.0° · Δ -2.6°</div></div>
  <div class="kcard glass"><h3>ПАР</h3><div class="kv">78.6<sup style="font-size:22px">°</sup></div><div class="ku">T ФИКС 78.50°</div></div>
  <div class="kcard glass"><h3>МОЩНОСТЬ</h3><div class="kv" style="color:var(--yellow)">64%</div><div class="ku">ПИД · клапан открыт</div></div>
</div>
<div class="chart glass" style="margin-top:10px">
  <div class="chart-head"><h3>ТЕМПЕРАТУРА · 10 МИН</h3><div class="legend"><span><i class="lg-c"></i>КУБ</span><span><i class="lg-o"></i>ПАР</span></div></div>
  <svg class="plot" viewBox="0 0 760 150" preserveAspectRatio="none">
    <g stroke="rgba(130,180,230,.13)" stroke-width="1"><line x1="0" y1="30" x2="760" y2="30"/><line x1="0" y1="65" x2="760" y2="65"/><line x1="0" y1="100" x2="760" y2="100"/><line x1="190" y1="0" x2="190" y2="150"/><line x1="380" y1="0" x2="380" y2="150"/><line x1="570" y1="0" x2="570" y2="150"/></g>
    <line x1="0" y1="72" x2="760" y2="72" stroke="#FFC857" stroke-width="1.5" stroke-dasharray="9 6" opacity=".85"/>
    <path d="M0 140 C 90 136 150 92 240 76 S 470 54 600 48 S 730 45 760 44" fill="none" stroke="#2E7CF6" stroke-width="2.8"/>
    <path d="M0 146 C 100 141 170 104 260 84 S 490 62 620 55 S 740 51 760 50" fill="none" stroke="#FF8A3D" stroke-width="2.8"/>
  </svg>
  <div class="xaxis"><span>-10M</span><span>-7M</span><span>-3M</span><span>сейчас</span></div>
</div>
</div>
"""

beer_body = """
<div class="stack">
<div class="steprow">
  <div class="step glass done"><div class="st">ПАУЗА 1</div><div class="sv">52.0°</div><div class="sd">ГИДРОЛИЗ · 15 мин</div></div>
  <div class="step glass done"><div class="st">ПАУЗА 2</div><div class="sv">63.0°</div><div class="sd">МАЛЬТОЗА · 25 мин</div></div>
  <div class="step glass now"><div class="st">ПАУЗА 3</div><div class="sv">72.0°</div><div class="sd">ОСАХАРЕНИЕ · 18 мин</div></div>
  <div class="step glass"><div class="st">ЗАТИРАНИЕ</div><div class="sv">78.0°</div><div class="sd">ФИЛЬТРАЦИЯ · 10 мин</div></div>
</div>
<div class="top3" style="margin-top:12px">
  <div class="kcard glass"><h3>НАСОС</h3><div class="kv" style="font-size:34px;color:var(--green)">ВКЛ</div><div class="ku">циркуляция · 2.1 л/мин</div></div>
  <div class="kcard glass"><h3>МЕШАЛКА</h3><div class="kv" style="font-size:34px">42 об/мин</div><div class="ku">перемешивание затора</div></div>
  <div class="kcard glass"><h3>ДАТЧИК</h3><div class="kv" style="font-size:34px">72.4°</div><div class="ku">T1 / T2 · герметичность OK</div></div>
</div>
</div>
"""

help_body = """
<div class="helplay">
  <div class="hnav glass"><div class="on">Безопасность</div><div>Глоссарий</div><div>Протокол RS485</div><div>О проекте</div><div>Справка по экранам</div></div>
  <div class="hart glass">
    <h2>БЕЗОПАСНОСТЬ</h2>
    <p>Контроллер остаётся единственным источником PID, safety и E-STOP. Панель HMI только отображает биты телеметрии и отправляет команды через белый список.</p>
    <p>При потере связи <code>link=false</code> после трёх пропущенных опросов (1.5 с). Последние биты безопасности остаются на экране.</p>
    <p>Команды проходят тот же <code>safety_gate</code>, что и REST API. Обход аварии с панели невозможен.</p>
    <p>Шина: <code>115200 8N1</code>, <code>POLL 500 мс</code>, CRC-16/CCITT-FALSE.</p>
  </div>
</div>
"""

calc_body = """
<div class="calcs">
  <div class="calc glass"><h3>КРЕПОСТЬ</h3><div class="formula">V₂ = V₁ · (C₁ / C₂)</div><div class="res">46.2%</div><div class="hint">1840 мл при 46% → 1730 мл при 49%</div></div>
  <div class="calc glass"><h3>ТЕМПЕРАТУРА</h3><div class="formula">T°F = T°C × 9/5 + 32</div><div class="res">173.1°F</div><div class="hint">78.4°C в градусах Фаренгейта</div></div>
  <div class="calc glass"><h3>ПОТОК</h3><div class="formula">Q = V / (t · 60)</div><div class="res">2.1 л/мин</div><div class="hint">12.6 л за 6 минут</div></div>
  <div class="calc glass"><h3>ЕДИНИЦЫ</h3><div class="formula">1 бар = 1000 мбар</div><div class="res">748 мбар</div><div class="hint">давление в кубе · 0.748 бар</div></div>
</div>
"""

screens = [
    screen("modes", "РЕЖИМЫ", "ВЫБОР РЕЖИМА", modes_body, on=True),
    screen("distill", "ДИСТИЛЛЯЦИЯ · РАБОТА", "ЭТАП: ОТБОР", distill_body),
    screen("beer", "ПИВО · ЗАТОР", "ПАУЗА 3", beer_body),
    screen("help", "СПРАВКА", "БЕЗОПАСНОСТЬ", help_body),
    screen("calc", "КАЛЬКУЛЯТОРЫ", "ИНСТРУМЕНТЫ", calc_body),
]

script = """
<script>
function tickClocks(){
  var d=new Date();
  var s=[d.getHours(),d.getMinutes(),d.getSeconds()]
        .map(function(v){return String(v).padStart(2,'0')}).join(':');
  document.querySelectorAll('[data-clock]').forEach(function(e){e.textContent=s});
}
tickClocks(); setInterval(tickClocks,1000);
var m=location.search.match(/screen=([\\w-]+)/);
var h=m ? m[1] : location.hash.replace('#','');
if(h){
  document.querySelectorAll('.screen').forEach(function(e){e.classList.remove('on')});
  var el=document.getElementById(h);
  if(el) el.classList.add('on');
}
</script>
</body>
</html>
"""

out = prefix + EXTRA_CSS + "\n".join(screens) + script
(PREVIEW / "hmi_more.html").write_text(out, encoding="utf-8")
print("Создан", PREVIEW / "hmi_more.html")
