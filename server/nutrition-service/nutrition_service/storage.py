from __future__ import annotations

import sqlite3
import uuid
from datetime import datetime
from pathlib import Path

from .models import FoodItem


class SQLiteFoodRecordStore:
    def __init__(self, db_path: str | Path):
        self.db_path = Path(db_path)
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def save_records(self, user_id: str, record_date: str, items: list[FoodItem]) -> None:
        with sqlite3.connect(self.db_path) as conn:
            conn.execute(
                "DELETE FROM food_records WHERE user_id = ? AND record_date = ?",
                (user_id, record_date),
            )
            conn.executemany(
                """
                INSERT INTO food_records (
                    id, user_id, record_date, meal, food_name, display_name,
                    amount_text, estimated_weight_g, category, source, created_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                [
                    (
                        uuid.uuid4().hex,
                        user_id,
                        record_date,
                        item.meal,
                        item.food_name,
                        item.display_name,
                        item.amount_text,
                        item.estimated_weight_g,
                        item.category,
                        item.source,
                        datetime.utcnow().isoformat(timespec="seconds"),
                    )
                    for item in items
                ],
            )

    def get_records(self, user_id: str, record_date: str) -> list[FoodItem]:
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            rows = conn.execute(
                """
                SELECT meal, food_name, display_name, amount_text,
                       estimated_weight_g, category, source
                FROM food_records
                WHERE user_id = ? AND record_date = ?
                ORDER BY created_at, rowid
                """,
                (user_id, record_date),
            ).fetchall()
        return [
            FoodItem(
                meal=row["meal"],
                food_name=row["food_name"],
                display_name=row["display_name"],
                amount_text=row["amount_text"],
                estimated_weight_g=row["estimated_weight_g"] or 0,
                category=row["category"],
                source=row["source"],
            )
            for row in rows
        ]

    def _init_db(self) -> None:
        with sqlite3.connect(self.db_path) as conn:
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS food_records (
                  id TEXT PRIMARY KEY,
                  user_id TEXT NOT NULL,
                  record_date TEXT NOT NULL,
                  meal TEXT NOT NULL,
                  food_name TEXT NOT NULL,
                  display_name TEXT NOT NULL,
                  amount_text TEXT NOT NULL,
                  estimated_weight_g INTEGER,
                  category TEXT NOT NULL,
                  source TEXT NOT NULL,
                  created_at TEXT NOT NULL
                )
                """
            )
