from __future__ import annotations

from dataclasses import asdict, dataclass
from typing import Any


DEFAULT_WATER_RATIO = 0.24
CONTROLLED_CATEGORIES = {
    "refined_staple",
    "coarse_grain",
    "protein",
    "vegetable",
    "fruit",
    "fat_sugar",
    "unknown",
}


@dataclass
class FoodItem:
    meal: str
    food_name: str
    display_name: str
    amount_text: str
    estimated_weight_g: int
    category: str
    confidence: float = 1.0
    source: str = "rule"
    base_food: str = ""
    modifier: str = ""

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "FoodItem":
        return cls(
            meal=str(data.get("meal", "unknown")),
            food_name=str(data.get("food_name", data.get("display_name", "unknown"))),
            display_name=str(data.get("display_name", data.get("food_name", "未知"))),
            amount_text=str(data.get("amount_text", "")),
            estimated_weight_g=int(data.get("estimated_weight_g") or 0),
            category=str(data.get("category", "unknown")),
            confidence=float(data.get("confidence", 1.0)),
            source=str(data.get("source", "manual")),
            base_food=str(data.get("base_food", "")),
            modifier=str(data.get("modifier", "")),
        )


@dataclass
class UserProfile:
    age_group: str = "adult"
    appetite_level: str = "normal"
    digestive_sensitivity: bool = False
    health_goal: str = "balanced"

    @classmethod
    def from_dict(cls, data: dict[str, Any] | None) -> "UserProfile":
        data = data or {}
        return cls(
            age_group=str(data.get("age_group", "adult")),
            appetite_level=str(data.get("appetite_level", "normal")),
            digestive_sensitivity=bool(data.get("digestive_sensitivity", False)),
            health_goal=str(data.get("health_goal", "balanced")),
        )

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass
class DoughRequest:
    dough_total_weight_g: int
    water_ratio: float = DEFAULT_WATER_RATIO
    target_food: str = "unknown"
    target_food_display: str = "未知"
    weight_source: str = "explicit"

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "DoughRequest":
        return cls(
            dough_total_weight_g=int(data.get("dough_total_weight_g") or 0),
            water_ratio=float(data.get("water_ratio", DEFAULT_WATER_RATIO)),
            target_food=str(data.get("target_food", "unknown")),
            target_food_display=str(data.get("target_food_display", "未知")),
            weight_source=str(data.get("weight_source", "explicit")),
        )

    @classmethod
    def estimated_for(cls, target_food: str, profile: UserProfile) -> "DoughRequest":
        base_weights = {
            "noodles": 280,
            "dumpling": 300,
            "baozi": 320,
            "mantou": 300,
            "pancake": 260,
            "unknown": 300,
        }
        display_names = {
            "noodles": "面条",
            "dumpling": "饺子",
            "baozi": "包子",
            "mantou": "馒头",
            "pancake": "烙饼",
            "unknown": "未知",
        }
        multiplier = 1.0
        if profile.age_group == "child":
            multiplier *= 0.6
        elif profile.age_group == "elderly":
            multiplier *= 0.7

        if profile.appetite_level == "light":
            multiplier *= 1.0
        elif profile.appetite_level == "large":
            multiplier *= 1.3

        base_weight = base_weights.get(target_food, base_weights["unknown"])
        return cls(
            dough_total_weight_g=round(base_weight * multiplier),
            water_ratio=DEFAULT_WATER_RATIO,
            target_food=target_food,
            target_food_display=display_names.get(target_food, "未知"),
            weight_source="estimated_by_profile_and_food_type",
        )


@dataclass
class ParseResult:
    items: list[FoodItem]
    dough_request: DoughRequest
    needs_confirmation: bool = True
    digestive_risk: bool = False

    def to_dict(self) -> dict[str, Any]:
        return {
            "items": [item.to_dict() for item in self.items],
            "dough_request": self.dough_request.to_dict(),
            "needs_confirmation": self.needs_confirmation,
            "digestive_risk": self.digestive_risk,
        }


@dataclass
class Recommendation:
    dough_total_weight_g: int
    flour_weight_g: int
    water_weight_g: int
    coarse_grain_ratio: float
    coarse_grain_weight_g: int
    reason: str

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)
