"""基础版黑盒集成测试：健康检查、地图/状态契约、移动、边界、重置与错误处理。

前置条件：被测服务已启动（由 Compose 健康检查或本地进程提供）。
每个需要独立初始位姿的用例通过公开的 RESET 命令建立基线，并从 RESET
响应读取 sequence；不依赖进程启动初态。
"""

import time


def _post(client, base_url, payload, timeout=2):
    return client.post(
        f"{base_url}/api/v1/robot/commands", json=payload, timeout=timeout
    )


def _reset(client, base_url):
    resp = _post(client, base_url, {"type": "RESET"})
    assert resp.status_code == 201, resp.text
    return resp.json()


# ---- 基础契约 ----


def test_health_ok(client, base_url):
    resp = client.get(f"{base_url}/api/v1/health", timeout=2)
    assert resp.status_code == 200
    body = resp.json()
    assert body["status"] == "ok"
    assert isinstance(body["instanceId"], str) and body["instanceId"]


def test_map_contract(client, base_url):
    resp = client.get(f"{base_url}/api/v1/map", timeout=2)
    assert resp.status_code == 200
    body = resp.json()
    assert body["mapId"]
    assert body["width"] == 30
    assert body["height"] == 30
    assert body["origin"] == "TOP_LEFT"
    assert body["xAxis"] == "RIGHT"
    assert body["yAxis"] == "DOWN"
    assert isinstance(body["obstacles"], list)
    assert all(set(o) == {"x", "y"} for o in body["obstacles"])
    assert len(body["obstacles"]) >= 30


def test_robot_contract(client, base_url):
    resp = client.get(f"{base_url}/api/v1/robot", timeout=2)
    assert resp.status_code == 200
    body = resp.json()
    for key in (
        "robotId",
        "instanceId",
        "position",
        "heading",
        "motionStatus",
        "lastResult",
        "sequence",
        "patrolActive",
    ):
        assert key in body
    assert set(body["position"]) == {"x", "y"}
    assert isinstance(body["patrolActive"], bool)


# ---- 移动与边界 ----


def test_move_right_from_reset(client, base_url):
    _reset(client, base_url)
    resp = _post(client, base_url, {"type": "MOVE", "direction": "RIGHT"})
    assert resp.status_code == 201
    body = resp.json()
    assert body["type"] == "MOVE"
    assert body["result"] == "MOVED"
    assert body["commandId"]
    assert body["robot"]["position"] == {"x": 1, "y": 0}
    assert body["robot"]["heading"] == "RIGHT"
    assert body["robot"]["motionStatus"] == "IDLE"
    assert body["robot"]["lastResult"] == "MOVED"


def test_four_directions_delta(client, base_url):
    _reset(client, base_url)
    # 从 (0,0) 出发：RIGHT -> (1,0)，DOWN -> (1,1)，LEFT -> (0,1)，UP -> (0,0)
    expected = {
        "RIGHT": (1, 0),
        "DOWN": (1, 1),
        "LEFT": (0, 1),
        "UP": (0, 0),
    }
    for direction, (x, y) in expected.items():
        resp = _post(client, base_url, {"type": "MOVE", "direction": direction})
        assert resp.status_code == 201
        assert resp.json()["robot"]["position"] == {"x": x, "y": y}


def test_boundary_block_and_recover(client, base_url):
    _reset(client, base_url)
    # (0,0) 向上越界：位置不变、朝向变 UP、BLOCKED。
    resp = _post(client, base_url, {"type": "MOVE", "direction": "UP"})
    assert resp.status_code == 201
    body = resp.json()
    assert body["result"] == "BLOCKED_BOUNDARY"
    assert body["robot"]["position"] == {"x": 0, "y": 0}
    assert body["robot"]["heading"] == "UP"
    assert body["robot"]["motionStatus"] == "BLOCKED"
    assert body["robot"]["lastResult"] == "BLOCKED_BOUNDARY"
    # 随后向右移动应恢复 IDLE，证明边界状态不会锁死。
    body = _post(client, base_url, {"type": "MOVE", "direction": "RIGHT"}).json()
    assert body["result"] == "MOVED"
    assert body["robot"]["position"] == {"x": 1, "y": 0}
    assert body["robot"]["motionStatus"] == "IDLE"


def test_obstacle_block_and_recover(client, base_url):
    _reset(client, base_url)
    # 移动到 (4,3)，下方 (4,4) 是默认障碍物。
    for _ in range(4):
        _post(client, base_url, {"type": "MOVE", "direction": "RIGHT"})
    for _ in range(3):
        _post(client, base_url, {"type": "MOVE", "direction": "DOWN"})
    resp = _post(client, base_url, {"type": "MOVE", "direction": "DOWN"})
    assert resp.status_code == 201
    body = resp.json()
    assert body["result"] == "BLOCKED_OBSTACLE"
    assert body["robot"]["position"] == {"x": 4, "y": 3}
    assert body["robot"]["heading"] == "DOWN"
    assert body["robot"]["motionStatus"] == "BLOCKED"
    assert body["robot"]["lastResult"] == "BLOCKED_OBSTACLE"
    # 向右移动应恢复 IDLE。
    body = _post(client, base_url, {"type": "MOVE", "direction": "RIGHT"}).json()
    assert body["result"] == "MOVED"
    assert body["robot"]["motionStatus"] == "IDLE"


def test_auto_patrol_start_and_stop(client, base_url):
    _reset(client, base_url)
    resp = _post(client, base_url, {"type": "START_PATROL"})
    assert resp.status_code == 201
    body = resp.json()
    assert body["result"] == "PATROL_STARTED"
    assert body["robot"]["patrolActive"] is True
    start_seq = body["robot"]["sequence"]
    # 后台线程每 250ms 推进一格；轮询等待 sequence 增长。
    deadline = time.time() + 10
    got = None
    while time.time() < deadline:
        got = client.get(f"{base_url}/api/v1/robot", timeout=2).json()
        if got["sequence"] >= start_seq + 3:
            break
        time.sleep(0.5)
    assert got is not None and got["sequence"] >= start_seq + 3
    assert got["patrolActive"] is True
    # 停止巡检。
    stop = _post(client, base_url, {"type": "STOP_PATROL"})
    assert stop.status_code == 201
    assert stop.json()["result"] == "PATROL_STOPPED"
    assert stop.json()["robot"]["patrolActive"] is False


# ---- 重置与序列号 ----


def test_reset_restores_initial(client, base_url):
    _reset(client, base_url)
    _post(client, base_url, {"type": "MOVE", "direction": "RIGHT"})
    _post(client, base_url, {"type": "MOVE", "direction": "DOWN"})
    body = _reset(client, base_url)
    assert body["result"] == "RESET"
    assert body["robot"]["position"] == {"x": 0, "y": 0}
    assert body["robot"]["heading"] == "RIGHT"
    assert body["robot"]["motionStatus"] == "IDLE"
    assert body["robot"]["lastResult"] == "RESET"


def test_sequence_monotonic_and_get_no_change(client, base_url):
    baseline = _reset(client, base_url)["robot"]["sequence"]
    moved = _post(client, base_url, {"type": "MOVE", "direction": "RIGHT"}).json()
    assert moved["robot"]["sequence"] == baseline + 1
    # GET 不改变 sequence。
    got = client.get(f"{base_url}/api/v1/robot", timeout=2).json()
    assert got["sequence"] == moved["robot"]["sequence"]


# ---- 错误处理 ----


def test_invalid_direction_no_state_change(client, base_url):
    _reset(client, base_url)
    before = client.get(f"{base_url}/api/v1/robot", timeout=2).json()
    resp = _post(client, base_url, {"type": "MOVE", "direction": "DIAGONAL"})
    assert resp.status_code == 400
    error = resp.json()["error"]
    assert error["code"] == "INVALID_DIRECTION"
    assert error["details"]["field"] == "direction"
    assert error["details"]["allowed"] == ["UP", "DOWN", "LEFT", "RIGHT"]
    after = client.get(f"{base_url}/api/v1/robot", timeout=2).json()
    assert after["position"] == before["position"]
    assert after["heading"] == before["heading"]
    assert after["sequence"] == before["sequence"]


def test_invalid_command_type(client, base_url):
    resp = _post(client, base_url, {"type": "FLY"})
    assert resp.status_code == 400
    assert resp.json()["error"]["code"] == "INVALID_COMMAND_TYPE"


def test_missing_type(client, base_url):
    resp = _post(client, base_url, {"direction": "RIGHT"})
    assert resp.status_code == 400
    assert resp.json()["error"]["code"] == "MISSING_FIELD"


def test_missing_direction(client, base_url):
    resp = _post(client, base_url, {"type": "MOVE"})
    assert resp.status_code == 400
    assert resp.json()["error"]["code"] == "MISSING_FIELD"


def test_unknown_field(client, base_url):
    resp = _post(client, base_url, {"type": "MOVE", "direction": "RIGHT", "speed": 9})
    assert resp.status_code == 400
    assert resp.json()["error"]["code"] == "UNKNOWN_FIELD"


def test_reset_with_direction_rejected(client, base_url):
    # SPEC 4.2.7：RESET 不允许携带 direction，应判为 UNKNOWN_FIELD。
    resp = _post(client, base_url, {"type": "RESET", "direction": "LEFT"})
    assert resp.status_code == 400
    body = resp.json()["error"]
    assert body["code"] == "UNKNOWN_FIELD"
    assert body["details"]["field"] == "direction"


def test_malformed_json(client, base_url):
    resp = client.post(
        f"{base_url}/api/v1/robot/commands",
        data="{not json",
        headers={"Content-Type": "application/json"},
        timeout=2,
    )
    assert resp.status_code == 400
    assert resp.json()["error"]["code"] == "MALFORMED_JSON"


def test_unsupported_media_type(client, base_url):
    resp = client.post(
        f"{base_url}/api/v1/robot/commands",
        data='{"type": "MOVE"}',
        headers={"Content-Type": "text/plain"},
        timeout=2,
    )
    assert resp.status_code == 415
    assert resp.json()["error"]["code"] == "UNSUPPORTED_MEDIA_TYPE"


def test_method_not_allowed(client, base_url):
    resp = client.delete(f"{base_url}/api/v1/robot", timeout=2)
    assert resp.status_code == 405
    assert "Allow" in resp.headers
    assert resp.json()["error"]["code"] == "METHOD_NOT_ALLOWED"


def test_unknown_api_route_json_404(client, base_url):
    resp = client.get(f"{base_url}/api/v1/nonexistent", timeout=2)
    assert resp.status_code == 404
    assert resp.json()["error"]["code"] == "ROUTE_NOT_FOUND"
