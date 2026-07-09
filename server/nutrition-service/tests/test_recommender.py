import unittest

from nutrition_service.models import DoughRequest, FoodItem, UserProfile
from nutrition_service.recommender import recommend_coarse_grain


class RecommenderTests(unittest.TestCase):
    def test_recommends_35_percent_for_high_refined_staple_day(self):
        items = [
            FoodItem(
                meal="breakfast",
                food_name="baozi",
                display_name="包子",
                amount_text="2个",
                estimated_weight_g=160,
                category="refined_staple",
            ),
            FoodItem(
                meal="lunch",
                food_name="rice",
                display_name="米饭",
                amount_text="1碗",
                estimated_weight_g=150,
                category="refined_staple",
            ),
        ]

        result = recommend_coarse_grain(items, DoughRequest(dough_total_weight_g=500))

        self.assertEqual(result.dough_total_weight_g, 500)
        self.assertEqual(result.water_weight_g, 120)
        self.assertEqual(result.flour_weight_g, 380)
        self.assertEqual(result.coarse_grain_ratio, 0.35)
        self.assertEqual(result.coarse_grain_weight_g, 133)

    def test_caps_ratio_for_digestive_risk(self):
        items = [
            FoodItem(
                meal="breakfast",
                food_name="baozi",
                display_name="包子",
                amount_text="4个",
                estimated_weight_g=320,
                category="refined_staple",
            )
        ]

        result = recommend_coarse_grain(
            items, DoughRequest(dough_total_weight_g=500), digestive_risk=True
        )

        self.assertEqual(result.coarse_grain_ratio, 0.20)
        self.assertEqual(result.coarse_grain_weight_g, 76)

    def test_estimates_dough_weight_from_target_food_and_profile(self):
        adult = UserProfile(age_group="adult", appetite_level="normal")
        child = UserProfile(age_group="child", appetite_level="light")
        elderly = UserProfile(age_group="elderly", appetite_level="light")

        adult_request = DoughRequest.estimated_for("dumpling", adult)
        child_request = DoughRequest.estimated_for("dumpling", child)
        elderly_request = DoughRequest.estimated_for("dumpling", elderly)

        self.assertEqual(adult_request.dough_total_weight_g, 300)
        self.assertEqual(child_request.dough_total_weight_g, 180)
        self.assertEqual(elderly_request.dough_total_weight_g, 210)

    def test_estimates_single_meal_dough_weight_by_food_type(self):
        adult = UserProfile(age_group="adult", appetite_level="normal")

        self.assertEqual(DoughRequest.estimated_for("noodles", adult).dough_total_weight_g, 280)
        self.assertEqual(DoughRequest.estimated_for("baozi", adult).dough_total_weight_g, 320)
        self.assertEqual(DoughRequest.estimated_for("mantou", adult).dough_total_weight_g, 300)
        self.assertEqual(DoughRequest.estimated_for("pancake", adult).dough_total_weight_g, 260)

    def test_keeps_base_ratio_when_today_already_had_coarse_grain_staple(self):
        items = [
            FoodItem(
                meal="lunch",
                food_name="buckwheat_noodles",
                display_name="荞麦面条",
                amount_text="1碗",
                estimated_weight_g=200,
                category="coarse_grain",
                base_food="面条",
                modifier="荞麦",
            )
        ]

        result = recommend_coarse_grain(items, DoughRequest(dough_total_weight_g=500))

        self.assertEqual(result.coarse_grain_ratio, 0.15)
        self.assertEqual(result.coarse_grain_weight_g, 57)


if __name__ == "__main__":
    unittest.main()
