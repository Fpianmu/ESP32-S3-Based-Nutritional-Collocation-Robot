from __future__ import annotations

from pathlib import Path
from typing import Any

from fastapi import FastAPI, Query
from pydantic import BaseModel, Field

from .llm_parser import load_llm_parse_config
from .service import NutritionService
from .storage import SQLiteFoodRecordStore


class ParseRequest(BaseModel):
    user_id: str
    text: str
    profile: dict[str, Any] | None = None


class RecommendRequest(BaseModel):
    user_id: str
    date: str
    confirmed_items: list[dict[str, Any]] = Field(default_factory=list)
    dough_request: dict[str, Any]


def create_app(service: NutritionService | None = None) -> FastAPI:
    app = FastAPI(title="Nutrition Service MVP")
    active_service = service or NutritionService(
        SQLiteFoodRecordStore(Path(__file__).resolve().parents[1] / "data" / "nutrition.db"),
        llm_config=load_llm_parse_config(),
    )

    @app.post("/api/v1/intake/parse")
    def parse_intake(request: ParseRequest):
        return active_service.parse_intake(request.user_id, request.text, request.profile)

    @app.post("/api/v1/recommendations/coarse-grain")
    def recommend(request: RecommendRequest):
        return active_service.confirm_and_recommend(
            user_id=request.user_id,
            date=request.date,
            confirmed_items=request.confirmed_items,
            dough_request=request.dough_request,
        )

    @app.get("/api/v1/intake/today")
    def today(user_id: str = Query(...), date: str | None = Query(None)):
        return active_service.get_today_records(user_id, date=date)

    return app


app = create_app()
