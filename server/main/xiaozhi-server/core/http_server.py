import asyncio

from aiohttp import web

from config.logger import setup_logging
from core.api.nutrition_handler import NutritionHandler
from core.api.ota_handler import OTAHandler
from core.api.vision_handler import VisionHandler

TAG = __name__


class SimpleHttpServer:
    def __init__(self, config: dict, nutrition_service=None):
        self.config = config
        self.logger = setup_logging()
        self.ota_handler = OTAHandler(config)
        self.vision_handler = VisionHandler(config)
        self.nutrition_handler = NutritionHandler(config, service=nutrition_service)

    def _get_websocket_url(self, local_ip: str, port: int) -> str:
        server_config = self.config["server"]
        websocket_config = server_config.get("websocket")

        if websocket_config and "你" not in websocket_config:
            return websocket_config
        return f"ws://{local_ip}:{port}/xiaozhi/v1/"

    def create_app(self):
        app = web.Application()
        read_config_from_api = self.config.get("read_config_from_api", False)

        if not read_config_from_api:
            app.add_routes(
                [
                    web.get("/xiaozhi/ota/", self.ota_handler.handle_get),
                    web.post("/xiaozhi/ota/", self.ota_handler.handle_post),
                    web.options("/xiaozhi/ota/", self.ota_handler.handle_options),
                    web.get(
                        "/xiaozhi/ota/download/{filename}",
                        self.ota_handler.handle_download,
                    ),
                    web.options(
                        "/xiaozhi/ota/download/{filename}",
                        self.ota_handler.handle_options,
                    ),
                ]
            )

        app.add_routes(
            [
                web.get("/mcp/vision/explain", self.vision_handler.handle_get),
                web.post("/mcp/vision/explain", self.vision_handler.handle_post),
                web.options("/mcp/vision/explain", self.vision_handler.handle_options),
                web.post(
                    "/api/v1/nutrition/intake/parse",
                    self.nutrition_handler.handle_parse,
                ),
                web.options(
                    "/api/v1/nutrition/intake/parse",
                    self.nutrition_handler.handle_options,
                ),
                web.post(
                    "/api/v1/nutrition/recommendations/coarse-grain",
                    self.nutrition_handler.handle_recommend,
                ),
                web.options(
                    "/api/v1/nutrition/recommendations/coarse-grain",
                    self.nutrition_handler.handle_options,
                ),
                web.get(
                    "/api/v1/nutrition/intake/today",
                    self.nutrition_handler.handle_today,
                ),
                web.options(
                    "/api/v1/nutrition/intake/today",
                    self.nutrition_handler.handle_options,
                ),
            ]
        )
        return app

    async def start(self):
        try:
            server_config = self.config["server"]
            host = server_config.get("ip", "0.0.0.0")
            port = int(server_config.get("http_port", 8003))

            if port:
                runner = web.AppRunner(self.create_app())
                await runner.setup()
                site = web.TCPSite(runner, host, port)
                await site.start()

                while True:
                    await asyncio.sleep(3600)
        except Exception as e:
            self.logger.bind(tag=TAG).error(f"HTTP服务器启动失败: {e}")
            import traceback

            self.logger.bind(tag=TAG).error(f"错误堆栈: {traceback.format_exc()}")
            raise
