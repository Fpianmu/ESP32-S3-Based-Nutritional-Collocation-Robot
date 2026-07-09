import unittest

from nutrition_service.llm_parser import LLMParseConfig, parse_with_llm


class FakeLLMClient:
    def __init__(self, content):
        self.content = content

    def parse(self, _prompt):
        return self.content


class LLMParserTests(unittest.TestCase):
    def test_uses_valid_llm_json(self):
        content = """
        {
          "items": [
            {
              "meal": "breakfast",
              "food_name": "whole_wheat_toast",
              "display_name": "全麦吐司",
              "amount_text": "2片",
              "estimated_weight_g": 80,
              "category": "coarse_grain",
              "confidence": 0.9,
              "base_food": "吐司",
              "modifier": "全麦"
            }
          ],
          "dough_request": {
            "target_food": "dumpling",
            "target_food_display": "饺子"
          },
          "digestive_risk": false
        }
        """

        result = parse_with_llm(
            "早上吃了两片全麦吐司，晚上想吃饺子",
            client=FakeLLMClient(content),
            config=LLMParseConfig(enabled=True),
        )

        self.assertIsNotNone(result)
        self.assertEqual(result.items[0].display_name, "全麦吐司")
        self.assertEqual(result.items[0].category, "coarse_grain")
        self.assertEqual(result.dough_request.target_food, "dumpling")
        self.assertEqual(result.dough_request.dough_total_weight_g, 300)

    def test_returns_none_for_invalid_llm_json(self):
        result = parse_with_llm(
            "早上随便吃了点东西",
            client=FakeLLMClient("not json"),
            config=LLMParseConfig(enabled=True),
        )

        self.assertIsNone(result)

    def test_filters_target_food_from_consumed_items(self):
        content = """
        {
          "items": [
            {
              "meal": "breakfast",
              "food_name": "whole_wheat_toast",
              "display_name": "全麦吐司",
              "amount_text": "一点",
              "estimated_weight_g": 50,
              "category": "coarse_grain",
              "confidence": 0.8
            },
            {
              "meal": "dinner",
              "food_name": "dumpling",
              "display_name": "饺子",
              "amount_text": "想包点",
              "estimated_weight_g": 200,
              "category": "refined_staple",
              "confidence": 0.7
            }
          ],
          "dough_request": {
            "dough_total_weight_g": 200,
            "target_food": "dumpling",
            "target_food_display": "饺子"
          },
          "digestive_risk": false
        }
        """

        result = parse_with_llm(
            "早饭随便啃了点全麦吐司，中午外面吃了碗牛肉面，晚上想包点饺子",
            client=FakeLLMClient(content),
            config=LLMParseConfig(enabled=True),
        )

        self.assertIsNotNone(result)
        self.assertEqual(len(result.items), 1)
        self.assertEqual(result.items[0].display_name, "全麦吐司")
        self.assertEqual(result.dough_request.dough_total_weight_g, 300)
        self.assertEqual(result.dough_request.weight_source, "estimated_by_profile_and_food_type")

    def test_accepts_llm_explicit_weight_only_when_text_contains_weight(self):
        content = """
        {
          "items": [],
          "dough_request": {
            "dough_total_weight_g": 600,
            "target_food": "dumpling",
            "target_food_display": "饺子"
          },
          "digestive_risk": false
        }
        """

        result = parse_with_llm(
            "晚上想包饺子，准备600g面团",
            client=FakeLLMClient(content),
            config=LLMParseConfig(enabled=True),
        )

        self.assertIsNotNone(result)
        self.assertEqual(result.dough_request.dough_total_weight_g, 600)
        self.assertEqual(result.dough_request.weight_source, "explicit")


if __name__ == "__main__":
    unittest.main()
