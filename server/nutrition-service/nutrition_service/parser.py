from __future__ import annotations

import re

from .models import DEFAULT_WATER_RATIO, DoughRequest, FoodItem, ParseResult, UserProfile
from .food_lexicon import COMPOSITE_FOOD_DICTIONARY, FOOD_DICTIONARY, TARGET_FOODS

NUMBER_WORDS = {
    "一": 1,
    "1": 1,
    "两": 2,
    "二": 2,
    "2": 2,
    "三": 3,
    "3": 3,
    "四": 4,
    "4": 4,
}

MEAL_KEYWORDS = {
    "breakfast": ("早上", "早餐", "早饭"),
    "lunch": ("中午", "午餐", "午饭"),
    "dinner": ("晚上", "晚餐", "晚饭"),
}

DIGESTIVE_RISK_WORDS = ("胃不舒服", "消化不好", "老人", "儿童", "小孩")


def parse_intake_text(text: str, profile: UserProfile | None = None) -> ParseResult:
    profile = profile or UserProfile()
    items: list[FoodItem] = []
    consumed_spans: list[tuple[int, int]] = []
    for display_name, (
        food_name,
        category,
        unit_weight,
        default_unit,
        base_food,
        modifier,
    ) in sorted(COMPOSITE_FOOD_DICTIONARY.items(), key=lambda item: len(item[0]), reverse=True):
        index = _find_available_span(text, display_name, consumed_spans, skip_target_context=True)
        if index < 0:
            continue
        meal = _detect_meal(text, display_name)
        amount, unit = _detect_amount(text, display_name, default_unit)
        items.append(
            FoodItem(
                meal=meal,
                food_name=food_name,
                display_name=display_name,
                amount_text=f"{amount}{unit}",
                estimated_weight_g=amount * unit_weight,
                category=category,
                base_food=base_food,
                modifier=modifier,
            )
        )
        consumed_spans.append((index, index + len(display_name)))

    for display_name, (food_name, category, unit_weight, default_unit) in sorted(
        FOOD_DICTIONARY.items(), key=lambda item: len(item[0]), reverse=True
    ):
        index = _find_available_span(text, display_name, consumed_spans, skip_target_context=True)
        if index < 0:
            continue
        meal = _detect_meal(text, display_name)
        amount, unit = _detect_amount(text, display_name, default_unit)
        items.append(
            FoodItem(
                meal=meal,
                food_name=food_name,
                display_name=display_name,
                amount_text=f"{amount}{unit}",
                estimated_weight_g=amount * unit_weight,
                category=category,
            )
        )
        consumed_spans.append((index, index + len(display_name)))

    if not items:
        unknown = _detect_unknown_food(text)
        if unknown:
            amount, unit = _detect_amount(text, unknown, "个")
            items.append(
                FoodItem(
                    meal=_detect_meal(text, unknown),
                    food_name=unknown,
                    display_name=unknown,
                    amount_text=f"{amount}{unit}",
                    estimated_weight_g=0,
                    category="unknown",
                    confidence=0.4,
                )
            )

    target_food, target_food_display = _detect_target_food(text)
    explicit_dough_weight = _detect_dough_weight(text)
    if explicit_dough_weight is None:
        dough_request = DoughRequest.estimated_for(target_food, profile)
    else:
        dough_request = DoughRequest(
            dough_total_weight_g=explicit_dough_weight,
            water_ratio=DEFAULT_WATER_RATIO,
            target_food=target_food,
            target_food_display=target_food_display,
            weight_source="explicit",
        )

    return ParseResult(
        items=items,
        dough_request=dough_request,
        needs_confirmation=True,
        digestive_risk=profile.digestive_sensitivity
        or any(word in text for word in DIGESTIVE_RISK_WORDS),
    )


def _detect_dough_weight(text: str) -> int | None:
    match = re.search(r"(\d+)\s*(?:g|克)\s*(?:面团)?", text, flags=re.IGNORECASE)
    return int(match.group(1)) if match else None


def _detect_target_food(text: str) -> tuple[str, str]:
    for display_name, (target_food, target_display) in TARGET_FOODS.items():
        if display_name in text and any(word in text for word in ("想吃", "准备", "晚上", "做")):
            return target_food, target_display
    return "unknown", "未知"


def _detect_meal(text: str, food: str) -> str:
    food_index = text.find(food)
    prefix = text[max(0, food_index - 12): food_index + len(food)]
    for meal, keywords in MEAL_KEYWORDS.items():
        if any(keyword in prefix for keyword in keywords):
            return meal
    return "unknown"


def _detect_amount(text: str, food: str, default_unit: str) -> tuple[int, str]:
    index = text.find(food)
    prefix = text[max(0, index - 6): index]
    match = re.search(r"([一两二三四1234])\s*([个碗杯份根])?$", prefix)
    if not match:
        return 1, default_unit
    amount = NUMBER_WORDS.get(match.group(1), 1)
    unit = match.group(2) or default_unit
    return amount, unit


def _detect_unknown_food(text: str) -> str | None:
    match = re.search(r"吃了(?:一|两|二|三|四|1|2|3|4)?(?:个|碗|杯|份|根)?([\u4e00-\u9fa5]{2,4})", text)
    if not match:
        return None
    candidate = match.group(1)
    if candidate in FOOD_DICTIONARY:
        return None
    return candidate


def _overlaps(start: int, end: int, spans: list[tuple[int, int]]) -> bool:
    return any(start < span_end and end > span_start for span_start, span_end in spans)


def _find_available_span(
    text: str,
    display_name: str,
    consumed_spans: list[tuple[int, int]],
    skip_target_context: bool = False,
) -> int:
    start = 0
    while True:
        index = text.find(display_name, start)
        if index < 0:
            return -1
        end = index + len(display_name)
        if (
            not _overlaps(index, end, consumed_spans)
            and not (skip_target_context and _is_target_context(text, index))
        ):
            return index
        start = end


def _is_target_context(text: str, index: int) -> bool:
    prefix = text[max(0, index - 8): index]
    return any(word in prefix for word in ("想吃", "准备", "想做", "做", "想包"))
