import json
import sys
from pathlib import Path

from aiohttp import web

from core.api.base_handler import BaseHandler

TAG = __name__


def _ensure_nutrition_service_path():
    repo_root = Path(__file__).resolve().parents[4]
    service_path = repo_root / "nutrition-service"
    if service_path.exists() and str(service_path) not in sys.path:
        sys.path.insert(0, str(service_path))


class NutritionHandler(BaseHandler):
    def __init__(self, config: dict, service=None):
        super().__init__(config)
        self.service = service or self._create_default_service()

    def _create_default_service(self):
        _ensure_nutrition_service_path()
        from nutrition_service.llm_parser import load_llm_parse_config
        from nutrition_service.service import NutritionService
        from nutrition_service.storage import SQLiteFoodRecordStore

        repo_root = Path(__file__).resolve().parents[4]
        db_path = repo_root / "nutrition-service" / "data" / "nutrition.db"
        llm_config = load_llm_parse_config()
        if self.config.get("nutrition", {}).get("llm_enabled", True):
            llm_config.enabled = True
        return NutritionService(SQLiteFoodRecordStore(db_path), llm_config=llm_config)

    def _json_response(self, payload, status=200):
        response = web.Response(
            text=json.dumps(payload, ensure_ascii=False, separators=(",", ":")),
            content_type="application/json",
            status=status,
        )
        self._add_cors_headers(response)
        return response

    async def handle_parse(self, request):
        try:
            payload = await request.json()
            result = self.service.parse_intake(
                user_id=str(payload.get("user_id", "")),
                text=str(payload.get("text", "")),
                profile=payload.get("profile"),
            )
            return self._json_response(result)
        except Exception as e:
            self.logger.bind(tag=TAG).error(f"Nutrition parse request failed: {e}")
            return self._json_response({"success": False, "message": str(e)}, status=400)

    async def handle_recommend(self, request):
        try:
            payload = await request.json()
            result = self.service.confirm_and_recommend(
                user_id=str(payload.get("user_id", "")),
                date=str(payload.get("date", "")),
                confirmed_items=payload.get("confirmed_items", []),
                dough_request=payload.get("dough_request", {}),
            )
            return self._json_response(result)
        except Exception as e:
            self.logger.bind(tag=TAG).error(f"Nutrition recommend request failed: {e}")
            return self._json_response({"success": False, "message": str(e)}, status=400)

    async def handle_today(self, request):
        try:
            user_id = str(request.query.get("user_id", ""))
            record_date = request.query.get("date")
            result = self.service.get_today_records(user_id, date=record_date)
            return self._json_response(result)
        except Exception as e:
            self.logger.bind(tag=TAG).error(f"Nutrition today request failed: {e}")
            return self._json_response({"success": False, "message": str(e)}, status=400)
