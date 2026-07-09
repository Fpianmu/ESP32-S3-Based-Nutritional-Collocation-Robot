import unittest

from nutrition_service.parser import parse_intake_text


class ParserTests(unittest.TestCase):
    def test_parses_common_foods_and_dough_weight(self):
        result = parse_intake_text(
            "早上吃了两个包子，中午一碗米饭和一个鸡蛋，晚上想吃500g面团"
        )

        self.assertEqual(result.dough_request.dough_total_weight_g, 500)
        self.assertEqual(result.dough_request.water_ratio, 0.24)
        self.assertTrue(result.needs_confirmation)
        self.assertEqual(len(result.items), 3)
        self.assertEqual(result.items[0].meal, "breakfast")
        self.assertEqual(result.items[0].food_name, "baozi")
        self.assertEqual(result.items[0].display_name, "包子")
        self.assertEqual(result.items[0].amount_text, "2个")
        self.assertEqual(result.items[0].estimated_weight_g, 160)
        self.assertEqual(result.items[0].category, "refined_staple")
        self.assertEqual(result.items[1].meal, "lunch")
        self.assertEqual(result.items[1].food_name, "rice")
        self.assertEqual(result.items[1].amount_text, "1碗")
        self.assertEqual(result.items[2].food_name, "egg")
        self.assertEqual(result.items[2].category, "protein")

    def test_marks_unknown_food_for_confirmation(self):
        result = parse_intake_text("早上吃了一个榴莲，晚上想吃300g面团")

        self.assertEqual(result.dough_request.dough_total_weight_g, 300)
        self.assertEqual(len(result.items), 1)
        self.assertEqual(result.items[0].display_name, "榴莲")
        self.assertEqual(result.items[0].category, "unknown")

    def test_parses_coarse_grain_modifier_before_base_food(self):
        result = parse_intake_text("中午吃了一碗荞麦面条，晚上想吃饺子")

        self.assertEqual(len(result.items), 1)
        self.assertEqual(result.items[0].display_name, "荞麦面条")
        self.assertEqual(result.items[0].food_name, "buckwheat_noodles")
        self.assertEqual(result.items[0].base_food, "面条")
        self.assertEqual(result.items[0].modifier, "荞麦")
        self.assertEqual(result.items[0].category, "coarse_grain")
        self.assertEqual(result.dough_request.target_food, "dumpling")
        self.assertEqual(result.dough_request.target_food_display, "饺子")
        self.assertEqual(result.dough_request.weight_source, "estimated_by_profile_and_food_type")

    def test_explicit_dough_weight_overrides_food_type_estimate(self):
        result = parse_intake_text("晚上想吃饺子，准备600g面团")

        self.assertEqual(result.dough_request.target_food, "dumpling")
        self.assertEqual(result.dough_request.dough_total_weight_g, 600)
        self.assertEqual(result.dough_request.weight_source, "explicit")

    def test_parses_whole_wheat_bread_as_breakfast_coarse_grain(self):
        result = parse_intake_text("早上吃了全麦面包，中午吃了一碗面条，晚上想吃面条")

        self.assertEqual(len(result.items), 2)
        self.assertEqual(result.items[0].meal, "breakfast")
        self.assertEqual(result.items[0].food_name, "whole_wheat_bread")
        self.assertEqual(result.items[0].display_name, "全麦面包")
        self.assertEqual(result.items[0].category, "coarse_grain")
        self.assertEqual(result.items[0].base_food, "面包")
        self.assertEqual(result.items[0].modifier, "全麦")
        self.assertEqual(result.items[1].food_name, "noodles")
        self.assertEqual(result.dough_request.target_food, "noodles")

    def test_parses_common_mixed_daily_foods(self):
        result = parse_intake_text("早上喝了一杯牛奶吃了一个鸡蛋，中午吃了鸡胸肉和青菜，晚上想吃馒头")

        food_names = {item.food_name for item in result.items}
        categories = {item.food_name: item.category for item in result.items}

        self.assertIn("milk", food_names)
        self.assertIn("egg", food_names)
        self.assertIn("chicken_breast", food_names)
        self.assertIn("greens", food_names)
        self.assertEqual(categories["chicken_breast"], "protein")
        self.assertEqual(categories["greens"], "vegetable")
        self.assertEqual(result.dough_request.target_food, "mantou")

    def test_parses_common_coarse_grain_foods(self):
        result = parse_intake_text("早上吃了一碗燕麦粥，中午吃了玉米和红薯，晚上想吃烙饼")

        food_names = {item.food_name for item in result.items}

        self.assertIn("oatmeal", food_names)
        self.assertIn("corn", food_names)
        self.assertIn("sweet_potato", food_names)
        self.assertNotIn("oats", food_names)
        self.assertEqual(len(result.items), 3)
        self.assertTrue(all(item.category == "coarse_grain" for item in result.items))
        self.assertEqual(result.dough_request.target_food, "pancake")


if __name__ == "__main__":
    unittest.main()
