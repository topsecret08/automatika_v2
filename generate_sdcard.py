#!/usr/bin/env python3
# -*- coding: utf-8 -*-
'''
generate_sdcard.py — создаёт полную SD-карту Automatika V0.9.6
Запуск: python generate_sdcard.py
'''

import os
import json

BASE = "sdcard"

def write(path, content):
    full = os.path.join(BASE, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8") as f:
        f.write(content)

def write_json(path, obj):
    write(path, json.dumps(obj, ensure_ascii=False, indent=2))

# ============================================================
# HELP — 103 статьи
# ============================================================
HELP_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sdcard_src", "help")
HELP = {}
for _name in sorted(os.listdir(HELP_DIR), key=lambda n: int(n.split("_", 1)[0])):
    with open(os.path.join(HELP_DIR, _name), "rb") as _f:
        HELP[_name] = _f.read().decode("utf-8")

# ============================================================
# РЕЦЕПТЫ ПИВА (90)
# ============================================================
def make_beer(id, name, desc, og, fg, abv, ibu, srm, mt, mm, ing):
    return {
        "id": id, "name": name, "category": "beer",
        "description": desc, "og": og, "fg": fg,
        "abv": abv, "ibu": ibu, "srm": srm,
        "mash_temp": mt, "mash_min": mm, "ingredients": ing
    }

BEER = []

# === ЛАГЕРЫ (10) ===
BEER.append(make_beer("beer.helles", "Мюнхенский Хеллес", "Светлый лагер", 1.048, 1.010, 4.9, 18, 4, 66, 60, "Pilsner 4 кг, Munich 1 кг, Hallertau 30 г"))
BEER.append(make_beer("beer.pils", "Пилзнер Урквел", "Чешский классический", 1.050, 1.013, 4.8, 40, 3, 63, 60, "Pilsner 5 кг, Saaz 60 г"))
BEER.append(make_beer("beer.vienna", "Венский лагер", "Янтарный", 1.052, 1.012, 5.2, 22, 10, 66, 60, "Vienna 3, Pilsner 2, Hallertau"))
BEER.append(make_beer("beer.marzen", "Мэрцен (Октоберфест)", "Праздничный", 1.057, 1.014, 5.7, 24, 10, 66, 60, "Munich 3, Vienna 2, Pilsner 1"))
BEER.append(make_beer("beer.dunkel", "Дункель", "Тёмный баварский", 1.050, 1.012, 5.0, 18, 18, 66, 60, "Munich 4, Pilsner 1, Choc 0.2"))
BEER.append(make_beer("beer.bock", "Бок", "Крепкий лагер", 1.066, 1.016, 6.6, 25, 18, 66, 60, "Munich 4, Pilsner 1.5, Crystal 0.5"))
BEER.append(make_beer("beer.doppelbock", "Доппельбок", "Двойной бок", 1.080, 1.020, 7.9, 27, 22, 65, 90, "Munich 6, Pilsner 2, Crystal 0.5"))
BEER.append(make_beer("beer.cc_lager", "California Common", "Паровой", 1.052, 1.012, 5.2, 38, 10, 66, 60, "Pale 4, Crystal 0.7, Northern Brewer"))
BEER.append(make_beer("beer.baltic", "Балтийский портер", "Крепкий тёмный лагер", 1.072, 1.018, 7.0, 35, 30, 66, 60, "Munich 4, Pilsner 2, Choc 0.5"))
BEER.append(make_beer("beer.rauch", "Раухбир", "Дымный", 1.052, 1.012, 5.2, 22, 12, 66, 60, "Smoked 3, Pilsner 2, Munich 1"))

# === ЭЛИ (15) ===
BEER.append(make_beer("beer.bitter", "Английский горький", "Классика", 1.042, 1.010, 4.2, 30, 8, 66, 60, "Maris Otter 3.5, Crystal 0.3"))
BEER.append(make_beer("beer.best", "Best Bitter", "Средней крепости", 1.046, 1.012, 4.5, 35, 10, 66, 60, "Maris Otter 4, Crystal 0.3"))
BEER.append(make_beer("beer.pale", "Американский Пэйл Эль", "Хмелевой", 1.055, 1.012, 5.6, 45, 8, 66, 60, "2-row 4.5, Crystal 20 0.5, Cascade 60"))
BEER.append(make_beer("beer.amber", "Амбер Эль", "Карамельный", 1.052, 1.012, 5.2, 30, 14, 66, 60, "Pale 4, Crystal 60 0.7"))
BEER.append(make_beer("beer.brown", "Браун Эль", "Ореховый", 1.050, 1.012, 5.0, 25, 20, 66, 60, "Pale 4, Brown 0.5, Choc 0.2"))
BEER.append(make_beer("beer.red", "Ирландский Красный", "Красный эль", 1.045, 1.011, 4.5, 25, 15, 66, 60, "Pale 3.5, Crystal 60 0.5, Roasted 0.05"))
BEER.append(make_beer("beer.scotch", "Шотландский Heavy", "Мягкий", 1.045, 1.012, 4.3, 20, 12, 66, 60, "Pale 4, Crystal 40 0.3"))
BEER.append(make_beer("beer.wee", "Wee Heavy", "Крепкий шотландский", 1.075, 1.020, 7.2, 25, 18, 65, 90, "Maris Otter 6, Crystal 0.7"))
BEER.append(make_beer("beer.mild", "Dark Mild", "Лёгкий тёмный", 1.036, 1.010, 3.4, 18, 18, 66, 60, "Pale 3, Crystal 0.3, Choc 0.1"))
BEER.append(make_beer("beer.cream", "Cream Ale", "Светлый американский", 1.048, 1.010, 5.0, 15, 3, 66, 60, "2-row 4, Corn 0.5"))
BEER.append(make_beer("beer.kolsch", "Kölsch", "Кёльнский", 1.048, 1.010, 4.8, 25, 4, 65, 60, "Pilsner 4, Wheat 0.5"))
BEER.append(make_beer("beer.altbier", "Altbier", "Дюссельдорфский", 1.048, 1.012, 4.7, 35, 14, 66, 60, "Pilsner 3, Munich 1.5, Crystal 0.3"))
BEER.append(make_beer("beer.amber_a", "American Amber", "Американский", 1.055, 1.013, 5.5, 35, 12, 66, 60, "2-row 4.5, Crystal 60 0.6"))
BEER.append(make_beer("beer.cc_ale", "California Common", "Паровой эль", 1.052, 1.012, 5.2, 38, 10, 66, 60, "Pale 4.5, Crystal 0.5"))
BEER.append(make_beer("beer.session", "Session Ale", "Лёгкий", 1.040, 1.008, 4.2, 30, 6, 66, 60, "Pale 3.5, Crystal 0.2"))

# === IPA (8) ===
BEER.append(make_beer("beer.ipa", "Американский IPA", "Классика", 1.065, 1.014, 6.7, 65, 8, 65, 60, "2-row 6, Crystal 0.5, Cascade+Cent 100"))
BEER.append(make_beer("beer.eng_ipa", "Английский IPA", "Традиционный", 1.060, 1.014, 6.0, 50, 10, 66, 60, "Maris Otter 5.5, Crystal 0.4, EKG 80"))
BEER.append(make_beer("beer.neipa", "NEIPA (Hazy)", "Мутный", 1.060, 1.014, 6.0, 50, 5, 67, 60, "Pale 5.5, Oat 1, Citra+Mosaic 200"))
BEER.append(make_beer("beer.dipa", "Double IPA", "Двойной", 1.080, 1.016, 8.4, 90, 9, 65, 60, "2-row 8, Crystal 0.6, Hop 250"))
BEER.append(make_beer("beer.session_ipa", "Session IPA", "Лёгкий IPA", 1.045, 1.010, 4.6, 50, 6, 66, 60, "Pale 4, Crystal 0.3, Hop 120"))
BEER.append(make_beer("beer.wc_ipa", "West Coast IPA", "Западное побережье", 1.065, 1.012, 7.0, 70, 8, 65, 60, "2-row 6, Munich 0.5, Columbus+Cascade"))
BEER.append(make_beer("beer.belg_ipa", "Belgian IPA", "Бельгийский", 1.070, 1.014, 7.4, 60, 8, 65, 60, "Pilsner 6, Candi 0.3, Belgian yeast"))
BEER.append(make_beer("beer.black_ipa", "Black IPA", "Тёмный IPA", 1.065, 1.014, 6.8, 65, 30, 65, 60, "2-row 5.5, Choc 0.3, Caraffa 0.2"))

# === СТАУТЫ (6) ===
BEER.append(make_beer("beer.irish_stout", "Ирландский сухой", "Классика", 1.042, 1.010, 4.2, 40, 35, 66, 60, "Pale 3.5, Roasted 0.5, Flaked 0.3"))
BEER.append(make_beer("beer.extra_stout", "Extra Stout", "Крепче", 1.055, 1.014, 5.4, 45, 40, 66, 60, "Pale 4.5, Roasted 0.7, Flaked 0.4"))
BEER.append(make_beer("beer.imperial_stout", "Imperial Stout", "Имперский", 1.090, 1.020, 9.2, 60, 40, 65, 90, "Maris Otter 8, Roasted 1, Choc 0.5"))
BEER.append(make_beer("beer.milk_stout", "Milk Stout", "С лактозой", 1.048, 1.014, 4.5, 30, 30, 66, 60, "Pale 4, Roasted 0.4, Lactose 0.5"))
BEER.append(make_beer("beer.oatmeal_stout", "Oatmeal Stout", "С овсом", 1.050, 1.014, 4.8, 32, 32, 66, 60, "Pale 4, Oat 0.5, Roasted 0.4"))
BEER.append(make_beer("beer.russian_imperial", "Russian Imperial", "Русский имперский", 1.095, 1.024, 9.5, 70, 45, 65, 90, "Maris Otter 9, Roasted 1, Choc 0.7"))

# === ПШЕНИЧНЫЕ (6) ===
BEER.append(make_beer("beer.hefe", "Hefeweizen", "Баварский", 1.050, 1.012, 5.0, 12, 4, 67, 60, "Wheat 3, Pilsner 2, Hallertau 30"))
BEER.append(make_beer("beer.dunkelweizen", "Dunkelweizen", "Тёмный пшеничный", 1.052, 1.014, 5.1, 15, 15, 67, 60, "Wheat 3, Munich 2, Choc 0.1"))
BEER.append(make_beer("beer.wit", "Witbier", "Бельгийский", 1.048, 1.010, 5.0, 15, 3, 66, 60, "Pilsner 2.5, Wheat 2, Апельсин+Кориандр"))
BEER.append(make_beer("beer.weizenbock", "Weizenbock", "Крепкий пшеничный", 1.070, 1.018, 6.8, 20, 16, 66, 60, "Wheat 4, Munich 2, Pilsner 1"))
BEER.append(make_beer("beer.am_wheat", "American Wheat", "Американский", 1.048, 1.010, 5.0, 20, 4, 66, 60, "2-row 2.5, Wheat 2.5"))
BEER.append(make_beer("beer.berliner", "Berliner Weisse", "Кислый пшеничный", 1.032, 1.006, 3.4, 8, 2, 66, 60, "Pilsner 2, Wheat 2, Lactobacillus"))

# === БЕЛЬГИЙСКИЕ (5) ===
BEER.append(make_beer("beer.dubbel", "Dubbel", "Бельгийский", 1.062, 1.012, 6.5, 20, 15, 65, 60, "Pilsner 4, Candi 0.5"))
BEER.append(make_beer("beer.tripel", "Tripel", "Тройной", 1.075, 1.010, 8.5, 30, 6, 65, 90, "Pilsner 6, Candi 1"))
BEER.append(make_beer("beer.quadrupel", "Quadrupel", "Четверной", 1.090, 1.018, 9.5, 25, 20, 65, 90, "Pilsner 8, Candi 1.5"))
BEER.append(make_beer("beer.saison", "Saison", "Фермерский", 1.050, 1.008, 5.5, 30, 6, 65, 60, "Pilsner 5, Wheat 0.5, Saaz 40"))
BEER.append(make_beer("beer.lambic", "Lambic", "Спонтанный", 1.050, 1.010, 5.2, 10, 8, 65, 60, "Pilsner 3, Wheat 2, Спонтанное"))

# === КИСЛЫЕ (5) ===
BEER.append(make_beer("beer.gose", "Gose", "Кисло-солёный", 1.044, 1.008, 4.7, 10, 4, 66, 60, "Pilsner 2.5, Wheat 2.5, Соль+Кориандр"))
BEER.append(make_beer("beer.flanders", "Flanders Red", "Красный кислый", 1.055, 1.012, 5.6, 15, 15, 65, 60, "Munich 3, Vienna 2, Crystal 0.5"))
BEER.append(make_beer("beer.am_sour", "American Sour", "Американский", 1.050, 1.008, 5.5, 10, 4, 66, 60, "Pale 4, Wheat 1, Lactobacillus"))
BEER.append(make_beer("beer.kettle", "Kettle Sour", "Чайник-кислый", 1.048, 1.008, 5.2, 12, 5, 66, 60, "Pale 4, Acidulated 0.3"))
BEER.append(make_beer("beer.oud_bruin", "Oud Bruin", "Старый коричневый", 1.055, 1.012, 5.6, 20, 20, 66, 60, "Munich 3, Vienna 1.5, Crystal 0.3"))

# === СПЕЦИАЛЬНЫЕ (35) ===
BEER.append(make_beer("beer.smoked_porter", "Smoked Porter", "Копчёный портер", 1.058, 1.014, 5.8, 30, 30, 66, 60, "Pale 4.5, Smoked 1, Choc 0.3"))
BEER.append(make_beer("beer.barleywine", "Barleywine", "Ячменное вино", 1.100, 1.020, 10.5, 80, 18, 65, 90, "Maris Otter 10, Crystal 1"))
BEER.append(make_beer("beer.fruit", "Fruit Beer", "Фруктовое", 1.050, 1.012, 5.0, 20, 6, 66, 60, "Pale 4, Фрукты 2 кг"))
BEER.append(make_beer("beer.spiced", "Spiced Winter", "Пряное", 1.060, 1.014, 6.0, 25, 14, 66, 60, "Pale 5, Специи"))
BEER.append(make_beer("beer.pumpkin", "Pumpkin Ale", "Тыквенное", 1.055, 1.014, 5.4, 20, 12, 66, 60, "Pale 4, Тыква 1.5 кг"))
BEER.append(make_beer("beer.christmas", "Christmas Ale", "Рождественское", 1.070, 1.018, 6.8, 30, 20, 66, 60, "Pale 5.5, Специи, Мёд"))
BEER.append(make_beer("beer.coffee_stout", "Coffee Stout", "Кофейный", 1.060, 1.016, 5.8, 30, 35, 66, 60, "Pale 5, Roasted 0.5, Кофе 0.2"))
BEER.append(make_beer("beer.choc_stout", "Chocolate Stout", "Шоколадный", 1.062, 1.018, 5.8, 25, 40, 66, 60, "Pale 5, Choc 0.6, Какао 0.3"))
BEER.append(make_beer("beer.oyster", "Oyster Stout", "С устрицами", 1.055, 1.014, 5.4, 35, 35, 66, 60, "Pale 4.5, Roasted 0.5, Устрицы"))
BEER.append(make_beer("beer.chili", "Chili Beer", "С перцем", 1.052, 1.012, 5.2, 25, 8, 66, 60, "Pale 4, Перец чили"))
BEER.append(make_beer("beer.ginger", "Ginger Beer", "Имбирное", 1.045, 1.010, 4.6, 15, 5, 66, 60, "Pale 3.5, Имбирь 0.2"))
BEER.append(make_beer("beer.honey", "Honey Ale", "Медовый эль", 1.055, 1.014, 5.4, 20, 8, 66, 60, "Pale 4, Мёд 0.7"))
BEER.append(make_beer("beer.maple", "Maple Beer", "Кленовый", 1.058, 1.014, 5.8, 25, 12, 66, 60, "Pale 4, Кленовый сироп 0.5"))
BEER.append(make_beer("beer.coconut", "Coconut Porter", "Кокосовый", 1.058, 1.016, 5.6, 25, 30, 66, 60, "Pale 4, Choc 0.4, Кокос 0.3"))
BEER.append(make_beer("beer.vanilla", "Vanilla Stout", "Ванильный", 1.060, 1.018, 5.8, 25, 40, 66, 60, "Pale 5, Roasted 0.5, Ваниль 0.05"))
BEER.append(make_beer("beer.raspberry", "Raspberry Ale", "Малиновый", 1.048, 1.012, 4.8, 15, 8, 66, 60, "Pale 4, Малина 1.5 кг"))
BEER.append(make_beer("beer.cherry", "Cherry Beer", "Вишнёвый", 1.052, 1.012, 5.2, 18, 12, 66, 60, "Pale 4, Вишня 2 кг"))
BEER.append(make_beer("beer.blueberry", "Blueberry Ale", "Голубичный", 1.050, 1.012, 5.0, 15, 10, 66, 60, "Pale 4, Голубика 1.5 кг"))
BEER.append(make_beer("beer.peach", "Peach Beer", "Персиковый", 1.048, 1.010, 4.9, 15, 6, 66, 60, "Pale 4, Персик 2 кг"))
BEER.append(make_beer("beer.strawberry", "Strawberry Ale", "Клубничный", 1.048, 1.010, 4.8, 15, 6, 66, 60, "Pale 4, Клубника 2 кг"))
BEER.append(make_beer("beer.mango", "Mango IPA", "Манговый IPA", 1.060, 1.014, 6.0, 55, 8, 66, 60, "Pale 5.5, Mango 1 кг, Citra"))
BEER.append(make_beer("beer.passion", "Passion Fruit IPA", "Маракуйя", 1.060, 1.014, 6.0, 55, 8, 66, 60, "Pale 5.5, Маракуйя 1 кг"))
BEER.append(make_beer("beer.grapefruit", "Grapefruit IPA", "Грейпфрутовый", 1.062, 1.014, 6.2, 60, 8, 66, 60, "Pale 5.5, Грейпфрут 0.7"))
BEER.append(make_beer("beer.citrus", "Citrus Pale", "Цитрусовый", 1.052, 1.012, 5.2, 40, 8, 66, 60, "Pale 4.5, Апельсин+Лимон"))
BEER.append(make_beer("beer.pineapple", "Pineapple Ale", "Ананасовый", 1.050, 1.010, 5.0, 20, 6, 66, 60, "Pale 4, Ананас 1.5 кг"))
BEER.append(make_beer("beer.tropical", "Tropical IPA", "Тропический IPA", 1.062, 1.014, 6.2, 60, 8, 66, 60, "Pale 5.5, Манго+Маракуйя"))
BEER.append(make_beer("beer.peanut", "Peanut Butter Stout", "Арахисовый", 1.060, 1.018, 5.8, 25, 35, 66, 60, "Pale 4.5, Roasted 0.5, Арахис"))
BEER.append(make_beer("beer.smores", "S'mores Stout", "Сморов", 1.065, 1.018, 6.2, 25, 38, 66, 60, "Pale 5, Choc 0.5, Зефир, Печенье"))
BEER.append(make_beer("beer.imperial_porter", "Imperial Porter", "Имперский портер", 1.075, 1.018, 7.5, 45, 35, 66, 60, "Maris Otter 6, Choc 0.6, Roasted 0.4"))
BEER.append(make_beer("beer.baltic_porter", "Baltic Porter", "Балтийский", 1.072, 1.018, 7.0, 35, 30, 66, 60, "Munich 4, Pilsner 2, Choc 0.5"))
BEER.append(make_beer("beer.imperial_pumpkin", "Imperial Pumpkin", "Имперское тыквенное", 1.080, 1.020, 8.0, 25, 25, 66, 60, "Pale 6, Тыква 2, Специи"))
BEER.append(make_beer("beer.imperial_choc", "Imperial Chocolate", "Имперский шоколад", 1.090, 1.022, 9.0, 40, 45, 65, 90, "Pale 8, Choc 1, Какао 0.5"))
BEER.append(make_beer("beer.imperial_coffee", "Imperial Coffee", "Имперский кофейный", 1.090, 1.022, 9.2, 40, 45, 65, 90, "Pale 8, Roasted 1, Кофе 0.3"))
BEER.append(make_beer("beer.barrel", "Barrel-Aged", "Бочковое", 1.100, 1.024, 10.0, 50, 25, 65, 90, "Pale 9, Munich 1, Выдержка в бочке"))

# ============================================================
# ДИСТИЛЛЯТЫ (30)
# ============================================================
SPIRITS = [
    {"id":"sp.whisky","name":"Шотландский Single Malt","ing":"Peated barley 5 кг"},
    {"id":"sp.irish","name":"Ирландский виски","ing":"Barley 5 кг, не торф."},
    {"id":"sp.bourbon","name":"Бурбон","ing":"Кукуруза 70%, Рожь 20%, Ячмень 10%"},
    {"id":"sp.rye","name":"Ржаной виски","ing":"Рожь 6 кг, Ячмень 2 кг"},
    {"id":"sp.tennessee","name":"Tennessee Whiskey","ing":"Кукуруза+Рожь, фильтр клён"},
    {"id":"sp.canadian","name":"Canadian Whisky","ing":"Кукуруза+Рожь, лёгкий"},
    {"id":"sp.japanese","name":"Japanese Whisky","ing":"Ячмень, стиль Шотландии"},
    {"id":"sp.wheated","name":"Wheated Bourbon","ing":"Кукуруза+Пшеница"},
    {"id":"sp.cognac","name":"Коньяк","ing":"Виноград (белый, кислый)"},
    {"id":"sp.armagnac","name":"Арманьяк","ing":"Виноград (Уни Блан)"},
    {"id":"sp.calvados","name":"Кальвадос","ing":"Яблоки (сидр)"},
    {"id":"sp.apple_brandy","name":"Apple Brandy","ing":"Яблочный сидр"},
    {"id":"sp.grape_brandy","name":"Grape Brandy","ing":"Виноград"},
    {"id":"sp.white_rum","name":"White Rum","ing":"Меласса, светлый"},
    {"id":"sp.gold_rum","name":"Gold Rum","ing":"Меласса, выдержка"},
    {"id":"sp.dark_rum","name":"Dark Rum","ing":"Меласса, карамель"},
    {"id":"sp.spiced_rum","name":"Spiced Rum","ing":"Ром + специи"},
    {"id":"sp.agricole","name":"Rhum Agricole","ing":"Тростниковый сок"},
    {"id":"sp.vodka_wheat","name":"Wheat Vodka","ing":"Пшеница"},
    {"id":"sp.vodka_potato","name":"Potato Vodka","ing":"Картофель"},
    {"id":"sp.vodka_rye","name":"Rye Vodka","ing":"Рожь"},
    {"id":"sp.vodka_corn","name":"Corn Vodka","ing":"Кукуруза"},
    {"id":"sp.vodka_grape","name":"Grape Vodka","ing":"Виноград"},
    {"id":"sp.gin_london","name":"London Dry Gin","ing":"Можжевельник + 10 трав"},
    {"id":"sp.gin_plymouth","name":"Plymouth Gin","ing":"Меньше можжевельника"},
    {"id":"sp.gin_old_tom","name":"Old Tom Gin","ing":"Сладкий джин"},
    {"id":"sp.gin_contemporary","name":"Contemporary Gin","ing":"Современные ботаникалы"},
    {"id":"sp.gin_navy","name":"Navy Strength Gin","ing":"Крепкий 57%"},
    {"id":"sp.absinthe","name":"Absinthe","ing":"Полынь, анис, фенхель"},
    {"id":"sp.aquavit","name":"Aquavit","ing":"Картофель/зерно + тмин"},
]

# ============================================================
# БРАГА (20)
# ============================================================
MASH = [
    {"id":"mash.sugar.classic","name":"Сахарная классическая","ing":"Сахар 5 кг, Вода 20 л, Дрожжи 100 г"},
    {"id":"mash.sugar.turbo","name":"Сахарная Turbo","ing":"Сахар 6 кг, Вода 25 л, Turbo 100 г"},
    {"id":"mash.sugar.invert","name":"Инвертированный сахар","ing":"Сахар + лимонная кислота"},
    {"id":"mash.sugar.molasses","name":"С мелассой","ing":"Сахар 3 кг + меласса 2 л"},
    {"id":"mash.sugar.honey","name":"С мёдом","ing":"Сахар 3 кг + мёд 2 кг"},
    {"id":"mash.sugar.fruit","name":"С фруктами","ing":"Сахар 4 кг + фрукты 5 кг"},
    {"id":"mash.sugar.tomato","name":"С томатом","ing":"Сахар + томатная паста"},
    {"id":"mash.sugar.citrus","name":"С цитрусом","ing":"Сахар + апельсины"},
    {"id":"mash.grain.barley","name":"Ячменная","ing":"Ячмень 5 кг, Ферменты, Дрожжи"},
    {"id":"mash.grain.corn","name":"Кукурузная","ing":"Кукуруза 5 кг, Ферменты, Дрожжи"},
    {"id":"mash.grain.wheat","name":"Пшеничная","ing":"Пшеница 5 кг, Ферменты, Дрожжи"},
    {"id":"mash.grain.rye","name":"Ржаная","ing":"Рожь 5 кг, Ферменты, Дрожжи"},
    {"id":"mash.grain.mixed","name":"Смешанная","ing":"Ячмень 2 + Кукуруза 2 + Рожь 1"},
    {"id":"mash.grain.malt","name":"Солодовая","ing":"Солод 6 кг, Дрожжи"},
    {"id":"mash.grain.enzyme","name":"На ферментах","ing":"Любое зерно + Амилосубтилин"},
    {"id":"mash.fruit.grape","name":"Виноградная","ing":"Виноград 20 кг, Дрожжи"},
    {"id":"mash.fruit.apple","name":"Яблочная","ing":"Яблоки 20 кг (сидр)"},
    {"id":"mash.fruit.pear","name":"Грушевая","ing":"Груши 20 кг"},
    {"id":"mash.fruit.plum","name":"Сливовая","ing":"Сливы 15 кг + сахар 3 кг"},
    {"id":"mash.fruit.berry","name":"Ягодная","ing":"Ягоды 10 кг + сахар 3 кг"},
]

# ============================================================
# УРОКИ (6 модулей)
# ============================================================
LESSONS = {
    "module1.json": [
        {"id":"l1.1","title":"Что такое перегонка?","theory":"Разделение жидкостей по T кипения. Спирт — 78.4°C, вода — 100°C.","q":"При какой T кипит этанол?","a":["100","78.4","50","120"],"c":1,"e":"78.4°C — ключевая цифра."},
        {"id":"l1.2","title":"Головы, тело, хвосты","theory":"Головы 5-10%, тело основное, хвосты >92°C.","q":"Сколько голов?","a":["1%","5-10%","20%","50%"],"c":1,"e":"5-10% — стандарт."},
        {"id":"l1.3","title":"Температуры","theory":"78°C головы, 78-92°C тело, >92°C хвосты.","q":"Когда хвосты?","a":["78","85","92","100"],"c":2,"e":"92°C — начало хвостов."},
    ],
    "module2.json": [
        {"id":"l2.1","title":"Дистилляция","theory":"Простая перегонка. Сохраняет ароматы.","q":"Что даёт дистилляция?","a":["96%","40-70% с ароматами","Воду","Ничего"],"c":1,"e":"Правильно."},
        {"id":"l2.2","title":"Ректификация","theory":"Глубокая очистка через колонну. 96.6%.","q":"Что НЕ используется в ректификации?","a":["Колонна","PID","Мощность","Термометр"],"c":1,"e":"PID даёт нестабильный поток."},
        {"id":"l2.3","title":"Пивоварня","theory":"Затирание, кипячение, хмель, брожение.","q":"T β-амилазы?","a":["50","63","72","78"],"c":1,"e":"63°C — β-амилаза."},
    ],
    "module3.json": [
        {"id":"l3.1","title":"ПВК","theory":"Пароводяной котёл для густых браг.","q":"Что можно гнать в ПВК?","a":["Только сахарную","Густые зерновые","Только воду","Спирт"],"c":1,"e":"Верно."},
        {"id":"l3.2","title":"Метод Габриэля","theory":"T1 = V × K, K = 0.30 + ABV/100 × 0.25","q":"Сколько частей?","a":["1","2","3","4"],"c":1,"e":"Верно."},
        {"id":"l3.3","title":"Габриэль — расчёт","theory":"Пример расчёта T1 и T2.","q":"V=20, ABV=10. T1?","a":["4","6","6.5","8"],"c":2,"e":"T1 = 20×0.325 = 6.5 л."},
        {"id":"l3.4","title":"НБК","theory":"Непрерывный процесс 24/7.","q":"Что подаётся постоянно?","a":["Продукт","Брага","Вода","Пар"],"c":1,"e":"Верно."},
        {"id":"l3.5","title":"Вакуум","theory":"При 100 мбар T кипения = 40°C.","q":"T при 100 мбар?","a":["30","40","60","78"],"c":1,"e":"40°C."},
    ],
    "module4.json": [
        {"id":"l4.1","title":"Правила безопасности","theory":"Никогда не закрывать выход пара.","q":"Что при запахе спирта?","a":["Закурить","Проветрить, СТОП","Продолжить","Забыть"],"c":1,"e":"Проветрить обязательно."},
        {"id":"l4.2","title":"Аварийные ситуации","theory":"Стоп при T>130, P>3 бар.","q":"Макс. P для ПВК?","a":["1","3","5","10"],"c":1,"e":"3 бар."},
    ],
    "module5.json": [
        {"id":"l5.1","title":"Что такое PID","theory":"Пропорционально-интегрально-дифференциальный.","q":"Где PID не подходит?","a":["Брожение","Ректификация","Затирание","ПВК"],"c":1,"e":"Ректификация — постоянная мощность."},
        {"id":"l5.2","title":"Автотюнинг","theory":"Ziegler-Nichols релейный метод.","q":"Метод автотюнинга?","a":["Z-N","Newton","Euler","RK"],"c":0,"e":"Ziegler-Nichols."},
        {"id":"l5.3","title":"Предекремент","theory":"Упреждающее снижение отбора.","q":"Зачем предекремент?","a":["Быстрее","Сглаживает скачок T","Меньше спирта","Красивее"],"c":1,"e":"Верно."},
        {"id":"l5.4","title":"Авто-92+","theory":"V = Vmax × (6.04 − 0.06×T).","q":"При 90°C множитель?","a":["0.64","0.5","0.8","0.9"],"c":0,"e":"0.64."},
    ],
    "module6.json": [
        {"id":"l6.1","title":"Экспорт/импорт","theory":"Все настройки в файл.","q":"Формат экспорта?","a":["JSON","BIN+CRC","CSV","XML"],"c":1,"e":"BIN+CRC."},
        {"id":"l6.2","title":"Схемы подключения","theory":"Смотрите /schemes/.","q":"Сколько режимов?","a":["4","6","8","10"],"c":2,"e":"8 режимов."},
        {"id":"l6.3","title":"Демо-режим","theory":"6 сценариев аварий.","q":"Демо нужен для?","a":["Работы","Обучения","Отдыха","Ничего"],"c":1,"e":"Обучения."},
        {"id":"l6.4","title":"Что дальше","theory":"Практика! Начните с сахарной браги.","q":"С чего начать?","a":["Ректификация","Сахарная брага","ПВК","НБК"],"c":1,"e":"Сахарная брага — для новичка."},
    ],
}

# ============================================================
# СХЕМЫ (7)
# ============================================================
SCHEMES = {
    "distill.json": {
        "title": "Дистилляция",
        "shapes": [
            {"t":"rect","x":80,"y":150,"w":120,"h":100,"c":"#555","label":"Куб"},
            {"t":"rect","x":120,"y":50,"w":40,"h":100,"c":"#888","label":"Царга"},
            {"t":"rect","x":115,"y":10,"w":50,"h":40,"c":"#aaa","label":"Дефл."},
            {"t":"line","x1":140,"y1":250,"x2":140,"y2":290,"c":"#0cf"},
            {"t":"text","x":150,"y":300,"label":"Отбор"},
        ]
    },
    "rectify.json": {"title":"Ректификация","shapes":[
        {"t":"rect","x":80,"y":150,"w":120,"h":100,"c":"#555","label":"Куб"},
        {"t":"rect","x":120,"y":20,"w":40,"h":130,"c":"#888","label":"Колонна"},
    ]},
    "beer.json":    {"title":"Пивоварня","shapes":[]},
    "pvk.json":     {"title":"ПВК","shapes":[]},
    "nbk.json":     {"title":"НБК","shapes":[]},
    "vacuum.json":  {"title":"Вакуум","shapes":[]},
    "double.json":  {"title":"Двойная","shapes":[]},
}

# ============================================================
# КАЛЬКУЛЯТОРЫ (8)
# ============================================================
CALCS = {
    "dilution.json": {
        "title": "Разбавление спирта",
        "inputs": [
            {"n":"vol","label":"Объём","min":0.1,"max":50,"def":1,"unit":"л"},
            {"n":"a1","label":"Исходная","min":20,"max":96,"def":96,"unit":"%"},
            {"n":"a2","label":"Целевая","min":5,"max":90,"def":40,"unit":"%"},
        ],
        "outputs": [
            {"n":"water","label":"Вода","formula":"vol*(a1/a2-1)","unit":"л"},
            {"n":"total","label":"Итого","formula":"vol*(a1/a2)","unit":"л"},
        ]
    },
    "sugar.json": {
        "title": "Сахарная брага",
        "inputs": [{"n":"vol","label":"Объём","min":5,"max":100,"def":20,"unit":"л"}],
        "outputs": [
            {"n":"sugar","label":"Сахар","formula":"vol*0.25","unit":"кг"},
            {"n":"yeast","label":"Дрожжи","formula":"vol*0.005","unit":"кг"},
            {"n":"yield","label":"Выход","formula":"vol*0.15","unit":"л"},
        ]
    },
    "grain.json": {
        "title": "Зерновая брага",
        "inputs": [
            {"n":"kg","label":"Зерно","min":1,"max":50,"def":5,"unit":"кг"},
            {"n":"hm","label":"Гидромодуль","min":2,"max":6,"def":4,"unit":"л/кг"},
        ],
        "outputs": [
            {"n":"water","label":"Вода","formula":"kg*hm","unit":"л"},
            {"n":"fermA","label":"Ферменты А","formula":"kg*3","unit":"г"},
            {"n":"fermG","label":"Ферменты Г","formula":"kg*2","unit":"г"},
        ]
    },
    "yield.json": {
        "title": "Выход продукта",
        "inputs": [
            {"n":"vol","label":"Объём","min":5,"max":100,"def":20,"unit":"л"},
            {"n":"abv","label":"Крепость","min":5,"max":20,"def":12,"unit":"%"},
            {"n":"target","label":"Целевая","min":20,"max":96,"def":40,"unit":"%"},
        ],
        "outputs": [
            {"n":"pure","label":"Спирт","formula":"vol*abv/100","unit":"л"},
            {"n":"out","label":"Итого","formula":"vol*abv/target","unit":"л"},
        ]
    },
    "abv.json": {
        "title": "ABV / IBU / SRM",
        "inputs": [
            {"n":"og","label":"OG","min":1.020,"max":1.100,"def":1.050},
            {"n":"fg","label":"FG","min":1.000,"max":1.030,"def":1.010},
        ],
        "outputs": [{"n":"abv","label":"ABV","formula":"(og-fg)*131.25","unit":"%"}]
    },
    "temp.json": {
        "title": "Крепость по T",
        "inputs": [{"n":"t","label":"T","min":78,"max":100,"def":85,"unit":"°C"}],
        "outputs": [{"n":"abv","label":"Крепость","formula":"table_lookup(t)","unit":"%"}]
    },
    "pressure.json": {
        "title": "Коррекция давления",
        "inputs": [{"n":"p","label":"Давление","min":950,"max":1050,"def":1013,"unit":"гПа"}],
        "outputs": [{"n":"boil","label":"T кипения","formula":"100+(p-1013)*0.028","unit":"°C"}]
    },
    "gabriel.json": {
        "title": "Метод Габриэля",
        "inputs": [
            {"n":"v","label":"Объём","min":5,"max":100,"def":20,"unit":"л"},
            {"n":"abv","label":"Крепость","min":5,"max":20,"def":10,"unit":"%"},
        ],
        "outputs": [
            {"n":"t1","label":"T1","formula":"v*(0.30+abv*0.0025)","unit":"л"},
            {"n":"t2","label":"T2","formula":"v*(1-(0.30+abv*0.0025))","unit":"л"},
            {"n":"yield","label":"Выход","formula":"v*(0.30+abv*0.0025)*abv*0.85/65","unit":"л"},
        ]
    },
}

# ============================================================
# ЯЗЫКИ (3)
# ============================================================
LANG = {
    "ru.json": {
        "app_title": "Automatika V0.9.6",
        "menu": {
            "modes": "Режимы работы", "monitoring": "Мониторинг",
            "recipes": "Рецепты", "calcs": "Калькуляторы",
            "ferment": "Брожение", "settings": "Настройки",
            "diag": "Диагностика", "learning": "Обучение",
            "help": "Справка", "roles": "Роли"
        },
        "actions": {
            "start": "СТАРТ", "stop": "СТОП", "pause": "ПАУЗА",
            "save": "Сохранить", "cancel": "Отмена", "back": "Назад",
            "yes": "Да", "no": "Нет", "ok": "OK"
        }
    },
    "en.json": {
        "app_title": "Automatika V0.9.6",
        "menu": {
            "modes": "Modes", "monitoring": "Monitoring",
            "recipes": "Recipes", "calcs": "Calculators",
            "ferment": "Fermentation", "settings": "Settings",
            "diag": "Diagnostics", "learning": "Learning",
            "help": "Help", "roles": "Roles"
        },
        "actions": {
            "start": "START", "stop": "STOP", "pause": "PAUSE",
            "save": "Save", "cancel": "Cancel", "back": "Back",
            "yes": "Yes", "no": "No", "ok": "OK"
        }
    },
    "de.json": {
        "app_title": "Automatika V0.9.6",
        "menu": {
            "modes": "Betriebsarten", "monitoring": "Überwachung",
            "recipes": "Rezepte", "calcs": "Rechner",
            "ferment": "Gärung", "settings": "Einstellungen",
            "diag": "Diagnose", "learning": "Lernen",
            "help": "Hilfe", "roles": "Rollen"
        },
        "actions": {
            "start": "START", "stop": "STOP", "pause": "PAUSE",
            "save": "Speichern", "cancel": "Abbrechen", "back": "Zurück",
            "yes": "Ja", "no": "Nein", "ok": "OK"
        }
    },
}

# ============================================================
# ГЛАВНЫЙ БЛОК
# ============================================================
def main():
    print("SD-карта Automatika V0.9.6")
    print()

    print("Справка:")
    for name, content in HELP.items():
        write(f"help/{name}", content)
    write("help/.generated", "1")
    print(f"  Всего: {len(HELP)} статей")
    print()

    print("Рецепты:")
    write_json("recipes/beer.json", BEER)
    write_json("recipes/spirits.json", SPIRITS)
    write_json("recipes/mash.json", MASH)
    print(f"  beer: {len(BEER)}, spirits: {len(SPIRITS)}, mash: {len(MASH)}")
    print()

    print("Уроки:")
    for name, lessons in LESSONS.items():
        write_json(f"lessons/{name}", lessons)
    total = sum(len(v) for v in LESSONS.values())
    print(f"  Модулей: {len(LESSONS)}, уроков: {total}")
    print()

    print("Схемы:")
    for name, data in SCHEMES.items():
        write_json(f"schemes/{name}", data)
    print(f"  Всего: {len(SCHEMES)}")
    print()

    print("Калькуляторы:")
    for name, data in CALCS.items():
        write_json(f"calc/{name}", data)
    print(f"  Всего: {len(CALCS)}")
    print()

    print("Языки:")
    for name, data in LANG.items():
        write_json(f"lang/{name}", data)
    print(f"  Всего: {len(LANG)}")
    print()

    for d in ["logs", "backups", "firmware", "screenshots", "ai", "profiles", "images"]:
        os.makedirs(os.path.join(BASE, d), exist_ok=True)
        write(f"{d}/.gitkeep", "")

    print(f"Созданы пустые папки: logs, backups, firmware, screenshots, ai, profiles, images")
    print()
    print(f"SD-карта создана в папке ./{BASE}/")
    print()

    total_files = sum(len(files) for _, _, files in os.walk(BASE))
    print(f"Итого файлов: {total_files}")
    print()
    print("Следующие шаги:")
    print("  1. Скопируйте папку sdcard/ на SD-карту (FAT32)")
    print("  2. Вставьте SD в устройство")
    print("  3. Первый запуск создаст automatika.db")


if __name__ == "__main__":
    main()
