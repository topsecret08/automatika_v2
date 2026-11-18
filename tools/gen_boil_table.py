"""Генератор таблицы температуры насыщения по IAPWS-IF97 (region 4).

Запуск: python tools/gen_boil_table.py
"""
import math


N = [0,
     0.11670521452767e4, -0.72421316703206e6, -0.17073846940092e2,
     0.12020824702470e5, -0.32325550322333e7, 0.14915108613530e2,
     -0.48232657361591e4, 0.40511340542057e6, -0.23855557567849,
     0.65017534844798e3]


def tsat(p_mpa):
    b = p_mpa ** 0.25
    e = b * b + N[3] * b + N[6]
    f = N[1] * b * b + N[4] * b + N[7]
    g = N[2] * b * b + N[5] * b + N[8]
    d = 2 * g / (-f - math.sqrt(f * f - 4 * e * g))
    return (N[10] + d - math.sqrt((N[10] + d) ** 2 -
                                  4 * (N[9] + N[10] * d))) / 2.0


def main():
    rows = [tsat(p / 10000.0) - 273.15 for p in range(300, 1101, 10)]
    with open('main/core/boil_table.c', 'w', encoding='utf-8') as f:
        f.write('// Сгенерировано tools/gen_boil_table.py по IAPWS-IF97 '
                '(region 4), P=300..1100 гПа.\n')
        f.write('#include "boil_table.h"\n\n')
        f.write('const float boil_table[BT_SIZE] = {\n')
        for i, r in enumerate(rows):
            f.write('    %.4ff,' % r)
            if (i + 1) % 8 == 0:
                f.write('\n')
        f.write('\n};\n')


if __name__ == '__main__':
    main()
