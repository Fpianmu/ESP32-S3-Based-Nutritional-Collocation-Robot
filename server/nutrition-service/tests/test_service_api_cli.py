import json
import tempfile
import unittest
from pathlib import Path

from fastapi.testclient import TestClient

from nutrition_cli import run_once
from nutrition_service.api import create_app
from nutrition_service.llm_parser import LLMParseConfig
from nutrition_service.service import NutritionService
from nutrition_service.storage import SQLiteFoodRecordStore


class FakeLLMClient:
    def __init__(self, content):
        self.content = content

    def parse(self, _prompt):
        return self.content


class ServiceApiCliTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        db_path = Path(self.temp_dir.name) / "nutrition.db"
        self.store = SQLiteFoodRecordStore(db_path)
        self.service = NutritionService(self.store)

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_service_parse_confirm_recommend_and_today_records(self):
        parsed = self.service.parse_intake(
            "popkik", "早上吃了两个包子，中午一碗米饭，晚上想吃500g面团"
        )
        self.assertEqual(parsed["dough_request"]["dough_total_weight_g"], 500)

        recommendation = self.service.confirm_and_recommend(
            user_id="popkik",
            date="2026-07-05",
            confirmed_items=parsed["items"],
            dough_request=parsed["dough_request"],
        )

        self.assertEqual(recommendation["water_weight_g"], 120)
        self.assertEqual(recommendation["flour_weight_g"], 380)
        self.assertEqual(recommendation["coarse_grain_weight_g"], 133)

        today = self.service.get_today_records("popkik", date="2026-07-05")
        self.assertEqual(len(today["items"]), 2)

    def test_fastapi_endpoints_return_json(self):
        client = TestClient(create_app(self.service))

        parse_response = client.post(
            "/api/v1/intake/parse",
            json={
                "user_id": "popkik",
                "text": "早上吃了两个包子，中午一碗米饭，晚上想吃500g面团",
            },
        )
        self.assertEqual(parse_response.status_code, 200)
        parsed = parse_response.json()
        self.assertEqual(parsed["dough_request"]["water_ratio"], 0.24)

        recommendation_response = client.post(
            "/api/v1/recommendations/coarse-grain",
            json={
                "user_id": "popkik",
                "date": "2026-07-05",
                "confirmed_items": parsed["items"],
                "dough_request": parsed["dough_request"],
            },
        )
        self.assertEqual(recommendation_response.status_code, 200)
        recommendation = recommendation_response.json()
        self.assertEqual(recommendation["coarse_grain_weight_g"], 133)

        today_response = client.get(
            "/api/v1/intake/today",
            params={"user_id": "popkik", "date": "2026-07-05"},
        )
        self.assertEqual(today_response.status_code, 200)
        self.assertEqual(len(today_response.json()["items"]), 2)

    def test_fastapi_parse_accepts_user_profile_and_target_food(self):
        client = TestClient(create_app(self.service))

        response = client.post(
            "/api/v1/intake/parse",
            json={
                "user_id": "popkik",
                "text": "中午吃了一碗荞麦面条，晚上想吃饺子",
                "profile": {
                    "age_group": "child",
                    "appetite_level": "light",
                    "digestive_sensitivity": False,
                },
            },
        )

        self.assertEqual(response.status_code, 200)
        payload = response.json()
        self.assertEqual(payload["items"][0]["display_name"], "荞麦面条")
        self.assertEqual(payload["items"][0]["category"], "coarse_grain")
        self.assertEqual(payload["dough_request"]["target_food"], "dumpling")
        self.assertEqual(payload["dough_request"]["target_food_display"], "饺子")
        self.assertEqual(payload["dough_request"]["dough_total_weight_g"], 180)
        self.assertEqual(
            payload["dough_request"]["weight_source"],
            "estimated_by_profile_and_food_type",
        )

    def test_cli_payload_can_be_serialized_as_json(self):
        payload = run_once(
            self.service,
            user_id="popkik",
            text="早上吃了两个包子，中午一碗米饭，晚上想吃500g面团",
            record_date="2026-07-05",
        )

        self.assertEqual(payload["recommendation"]["water_weight_g"], 120)
        self.assertEqual(payload["recommendation"]["coarse_grain_weight_g"], 133)

        serialized = json.dumps(payload["recommendation"], ensure_ascii=False)
        self.assertIn('"water_weight_g": 120', serialized)
        self.assertIn('"coarse_grain_weight_g": 133', serialized)

    def test_cli_run_once_returns_structured_payload(self):
        payload = run_once(
            self.service,
            user_id="popkik",
            text="早上吃了两个包子，中午一碗米饭，晚上想吃500g面团",
            record_date="2026-07-05",
        )

        self.assertEqual(payload["parsed"]["items"][0]["display_name"], "包子")
        self.assertEqual(payload["recommendation"]["flour_weight_g"], 380)
        self.assertEqual(payload["recommendation"]["coarse_grain_ratio"], 0.35)

    def test_service_payload_can_be_serialized_as_json(self):
        parsed = self.service.parse_intake(
            "popkik", "早上吃了两个包子，中午一碗米饭，晚上想吃500g面团"
        )
        recommendation = self.service.confirm_and_recommend(
            "popkik", "2026-07-05", parsed["items"], parsed["dough_request"]
        )

        payload = json.dumps(recommendation, ensure_ascii=False)

        self.assertIn('"water_weight_g": 120', payload)
        self.assertIn('"coarse_grain_weight_g": 133', payload)

    def test_service_can_use_llm_then_fallback_rules_remain_available(self):
        service = NutritionService(
            self.store,
            llm_client=FakeLLMClient(
                '{"items":[{"meal":"breakfast","food_name":"whole_wheat_toast","display_name":"全麦吐司","amount_text":"2片","estimated_weight_g":80,"category":"coarse_grain","confidence":0.9}],"dough_request":{"target_food":"dumpling","target_food_display":"饺子"},"digestive_risk":false}'
            ),
            llm_config=LLMParseConfig(enabled=True),
        )

        parsed = service.parse_intake("popkik", "早上吃了两片全麦吐司，晚上想吃饺子")

        self.assertEqual(parsed["items"][0]["display_name"], "全麦吐司")
        self.assertEqual(parsed["dough_request"]["target_food"], "dumpling")


if __name__ == "__main__":
    unittest.main()
