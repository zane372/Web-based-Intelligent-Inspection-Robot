"""pytest 共享配置：注册 --base-url 参数与会话级 HTTP 客户端。

遵循 SPEC 6.5.3：通过 pytest_addoption 注册 --base-url（默认
http://127.0.0.1:8080）并提供 fixture；所有请求带超时；fixture 在限定
时间内轮询健康接口，不依赖固定 sleep。
"""

import time

import pytest
import requests


def pytest_addoption(parser):
    parser.addoption(
        "--base-url",
        action="store",
        default="http://127.0.0.1:8080",
        help="被测巡检服务器基础地址，例如 http://127.0.0.1:8080",
    )


@pytest.fixture(scope="session")
def base_url(request):
    return request.config.getoption("--base-url")


@pytest.fixture(scope="session")
def client(base_url):
    """等待 /api/v1/health 就绪后，返回会话级 requests.Session。"""
    session = requests.Session()
    deadline = time.monotonic() + 60
    last_error = None
    while time.monotonic() < deadline:
        try:
            resp = session.get(f"{base_url}/api/v1/health", timeout=2)
            if resp.status_code == 200:
                break
        except requests.RequestException as exc:  # 服务尚未就绪，继续等待
            last_error = exc
        time.sleep(1)
    else:
        raise RuntimeError(f"60 秒内服务未就绪：{last_error}")
    yield session
    session.close()
