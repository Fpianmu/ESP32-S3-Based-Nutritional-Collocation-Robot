from __future__ import annotations

from datetime import date
from typing import Any

from .llm_parser import LLMClient, LLMParseConfig, parse_with_llm
from .models import DoughRequest, FoodItem, UserProfile
from .parser import parse_intake_text
from .recommender import recommend_coarse_grain
from .storage import SQLiteFoodRecordStore


class NutritionService:
    def __init__(
        self,
        store: SQLiteFoodRecordStore,
        llm_client: LLMClient | None = None,
        llm_config: LLMParseConfig | None = None,
    ):
        self.store = store
        self.llm_client = llm_client
        self.llm_config = llm_config

    def parse_intake(
        self,
        user_id: str,
        text: str,
        profile: dict[str, Any] | UserProfile | None = None,
    ) -> dict[str, Any]:
        _ = user_id
        user_profile = profile if isinstance(profile, UserProfile) else UserProfile.from_dict(profile)
        llm_result = parse_with_llm(
            text,
            profile=user_profile,
            client=self.llm_client,
            config=self.llm_config,
        )
        if llm_result is not None:
            return llm_result.to_dict()
        return parse_intake_text(text, user_profile).to_dict()

    def confirm_and_recommend(
        self,
        user_id: str,
        date: str,
        confirmed_items: list[dict[str, Any]],
        dough_request: dict[str, Any],
    ) -> dict[str, Any]:
        items = [FoodItem.from_dict(item) for item in confirmed_items]
        request = DoughRequest.from_dict(dough_request)
        self.store.save_records(user_id, date, items)
        return recommend_coarse_grain(items, request).to_dict()

    def get_today_records(self, user_id: str, date: str | None = None) -> dict[str, Any]:
        record_date = date or self.today()
        return {
            "user_id": user_id,
            "date": record_date,
            "items": [item.to_dict() for item in self.store.get_records(user_id, record_date)],
        }

    @staticmethod
    def today() -> str:
        return date.today().isoformat()
