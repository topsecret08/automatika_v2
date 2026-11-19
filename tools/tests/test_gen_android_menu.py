from __future__ import annotations

import unittest

from tools import gen_android_menu as gen

SAMPLE = r'''
static const menu_item_t FLAGS[] = {
    { "fl.a", "☑", "Тест \"кавычки\" {скобка}", MENU_ITEM_TOGGLE, .nvs_key="k", .required_role=2 },
};
static const menu_item_t SLIDERS[] = {
    { "f.t", "⚙", "Целевая T", MENU_ITEM_SLIDER, .nvs_key="ferm.t", .min=15, .max=30, .step=0.5, .unit="°C", .required_role=1 },
};
static const menu_item_t ROOT_ITEMS[] = {
    { "menu.s", "🎛", "Настройки", MENU_ITEM_SUBMENU, .children=FLAGS,
      .children_count=sizeof(FLAGS)/sizeof(*FLAGS), .required_role=2, .color_category=5 },
    { "menu.f", "🌡", "Брожение", MENU_ITEM_SUBMENU, .children=SLIDERS,
      .children_count=sizeof(SLIDERS)/sizeof(*SLIDERS), .color_category=4 },
};
'''


class GenerateTest(unittest.TestCase):
    def test_children_arrays_are_emitted_before_root(self):
        out = gen.generate(SAMPLE)
        self.assertIn("private val MENU_FLAGS: List<MenuItem> = listOf(", out)
        self.assertIn("val ROOT_MENU: List<MenuItem> = listOf(", out)
        self.assertLess(out.index("MENU_FLAGS"), out.index("ROOT_MENU"))

    def test_options_are_translated(self):
        out = gen.generate(SAMPLE)
        self.assertIn("requiredRole = 2", out)
        self.assertIn("colorCategory = 5", out)
        self.assertIn("children = MENU_FLAGS", out)
        self.assertIn("nvsKey = \"ferm.t\"", out)
        self.assertIn("min = 15.0", out)
        self.assertIn("step = 0.5", out)
        self.assertIn("unit = \"°C\"", out)
        self.assertIn("type = MenuType.TOGGLE", out)

    def test_quotes_and_braces_in_strings_are_handled(self):
        out = gen.generate(SAMPLE)
        self.assertIn('title = "Тест \\"кавычки\\" {скобка}"', out)

    def test_dollar_sign_is_escaped_for_kotlin(self):
        escaped = gen._kotlin_string("цена $5")
        self.assertEqual(escaped, '"цена \\$5"')

    def test_unknown_child_list_is_rejected(self):
        broken = SAMPLE.replace(".children=FLAGS", ".children=NOPE", 1)
        with self.assertRaises(ValueError):
            gen.generate(broken)

    def test_generation_is_deterministic(self):
        self.assertEqual(gen.generate(SAMPLE), gen.generate(SAMPLE))

    def test_real_menu_tree_generates(self):
        source = gen.SOURCE.read_text(encoding="utf-8")
        out = gen.generate(source)
        self.assertIn("val ROOT_MENU: List<MenuItem> = listOf(", out)
        self.assertEqual(out.count('MenuItem(id = "menu.'), 25)


if __name__ == "__main__":
    unittest.main()
