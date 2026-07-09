import sys
import unittest
from pathlib import Path

from aiohttp.test_utils import TestClient, TestServer


ROOT = Path(__file__).resolve().parents[2]
MAIN_SERVER = ROOT / "main" / "xiaozhi-server"
NUTRITION_SERVICE = ROOT / "nutrition-service"
for path in (str(MAIN_SERVER), str(NUTRITION_SERVICE)):
    if path not in sys.path:
        sys.path.insert(0, path)

from core.api.nutrition_handler import _ensure_nutrition_service_path  # noqa: E402
from core.http_server import SimpleHttpServer  # noqa: E402


class StubNutritionService:
    def parse_intake(self, user_id, text, profile=None):
        return {
            "user_id": user_id,
            "text": text,
            "profile": profile,
            "items": [],
            "dough_request": {
                "dough_total_weight_g": 300,
                "water_ratio": 0.24,
                "target_food": "dumpling",
                "target_food_display": "饺子",
                "weight_source": "estimated_by_profile_and_food_type",
            },
            "needs_confirmation": True,
            "digestive_risk": False,
        }

    def confirm_and_recommend(self, user_id, date, confirmed_items, dough_request):
        return {
            "user_id": user_id,
            "date": date,
            "dough_total_weight_g": dough_request["dough_total_weight_g"],
            "flour_weight_g": 228,
            "water_weight_g": 72,
            "coarse_grain_ratio": 0.35,
            "coarse_grain_weight_g": 80,
            "reason": "测试推荐",
        }

    def get_today_records(self, user_id, date=None):
        return {"user_id": user_id, "date": date, "items": []}


class MainHttpNutritionIntegrationTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        config = {
            "server": {
                "auth_key": "test-auth-key",
                "ip": "127.0.0.1",
                "http_port": 0,
            },
            "read_config_from_api": False,
        }
        server = SimpleHttpServer(config, nutrition_service=StubNutritionService())
        self.client = TestClient(TestServer(server.create_app()))
        await self.client.start_server()

    async def asyncTearDown(self):
        await self.client.close()

    async def test_main_http_server_exposes_nutrition_parse_route(self):
        response = await self.client.post(
            "/api/v1/nutrition/intake/parse",
            json={
                "user_id": "popkik",
                "text": "早上吃了全麦吐司，晚上想包饺子",
                "profile": {"age_group": "adult"},
            },
        )

        self.assertEqual(response.status, 200)
        payload = await response.json()
        self.assertEqual(payload["dough_request"]["target_food"], "dumpling")
        self.assertEqual(payload["dough_request"]["water_ratio"], 0.24)

    async def test_main_http_server_exposes_nutrition_recommend_route(self):
        response = await self.client.post(
            "/api/v1/nutrition/recommendations/coarse-grain",
            json={
                "user_id": "popkik",
                "date": "2026-07-05",
                "confirmed_items": [],
                "dough_request": {"dough_total_weight_g": 300},
            },
        )

        self.assertEqual(response.status, 200)
        payload = await response.json()
        self.assertEqual(payload["water_weight_g"], 72)
        self.assertEqual(payload["coarse_grain_weight_g"], 80)

    async def test_default_nutrition_handler_adds_service_package_path(self):
        expected_path = str(NUTRITION_SERVICE)
        original_path = list(sys.path)
        try:
            sys.path = [path for path in sys.path if path != expected_path]

            _ensure_nutrition_service_path()

            self.assertIn(expected_path, sys.path)
        finally:
            sys.path = original_path


if __name__ == "__main__":
    unittest.main()
