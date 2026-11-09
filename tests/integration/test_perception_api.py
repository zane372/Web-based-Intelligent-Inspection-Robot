"""虚拟感知黑盒集成测试：latest/events 契约、异常检测、接触事件与非法查询。

前置条件：被测服务已启动且启用感知（PERCEPTION_CONFIG 指向
config/perception.json：rangeCells=5、hazardRangeCells=2；异常源
smoke-01/temp-01/gas-01/oil-01 的初始位置仅为配置初值，每次 RESET
后会在界内可通行格中重新随机化，真值不对外暴露）。
"""


def _post(client, base_url, payload, timeout=2):
    return client.post(
        f"{base_url}/api/v1/robot/commands", json=payload, timeout=timeout
    )


def _reset(client, base_url):
    resp = _post(client, base_url, {"type": "RESET"})
    assert resp.status_code == 201, resp.text
    return resp.json()


# ---- 契约 ----

def test_perception_latest_contract(client, base_url):
    resp = client.get(f"{base_url}/api/v1/perception/latest", timeout=2)
    assert resp.status_code == 200
    body = resp.json()
    assert body["enabled"] is True
    assert isinstance(body["instanceId"], str) and body["instanceId"]
    assert body["observation"] is not None
    obs = body["observation"]
    assert isinstance(obs["sequence"], int)
    assert obs["rangeCells"] >= 1
    ranges = obs["ranges"]
    assert len(ranges) == 3
    assert {r["sensorId"] for r in ranges} == {
        "range-front",
        "range-left",
        "range-right",
    }
    for r in ranges:
        assert r["relativeDirection"] in ("FRONT", "LEFT", "RIGHT")
        assert r["absoluteDirection"] in ("UP", "DOWN", "LEFT", "RIGHT")
        assert r["hitType"] in ("NONE", "BOUNDARY", "OBSTACLE")
        if r["hitType"] == "NONE":
            assert r["distanceCells"] is None
            assert r["cell"] is None
        else:
            assert isinstance(r["distanceCells"], int)
    assert isinstance(obs["detections"], list)
    assert isinstance(body["activeDetections"], list)
    assert isinstance(body["latestEventId"], int)


def test_perception_events_contract(client, base_url):
    resp = client.get(f"{base_url}/api/v1/perception/events", timeout=2)
    assert resp.status_code == 200
    body = resp.json()
    assert isinstance(body["instanceId"], str)
    assert isinstance(body["events"], list)
    assert body["latestEventId"] >= 0
    assert isinstance(body["hasMore"], bool)
    assert isinstance(body["truncated"], bool)
    assert body["oldestEventId"] is None or isinstance(body["oldestEventId"], int)
    # 事件按 eventId 升序。
    ids = [e["eventId"] for e in body["events"]]
    assert ids == sorted(ids)


def test_perception_head_support(client, base_url):
    for path in ("/api/v1/perception/latest", "/api/v1/perception/events"):
        resp = client.head(f"{base_url}{path}", timeout=2)
        assert resp.status_code == 200
        assert resp.content == b""


# ---- 非法查询参数 ----

def test_perception_invalid_after(client, base_url):
    for bad in ("abc", "-1", "1.5"):
        resp = client.get(
            f"{base_url}/api/v1/perception/events?after={bad}", timeout=2
        )
        assert resp.status_code == 400, bad
        err = resp.json()["error"]
        assert err["code"] == "INVALID_QUERY_PARAMETER"
        assert err["details"]["field"] == "after"


def test_perception_invalid_limit(client, base_url):
    for bad in ("0", "101", "-3", "abc"):
        resp = client.get(
            f"{base_url}/api/v1/perception/events?limit={bad}", timeout=2
        )
        assert resp.status_code == 400, bad
        err = resp.json()["error"]
        assert err["code"] == "INVALID_QUERY_PARAMETER"
        assert err["details"]["field"] == "limit"


def test_perception_rejects_unknown_and_duplicate_params(client, base_url):
    # 未知字段（审查 M1）。
    resp = client.get(
        f"{base_url}/api/v1/perception/events?foo=1", timeout=2
    )
    assert resp.status_code == 400
    err = resp.json()["error"]
    assert err["code"] == "INVALID_QUERY_PARAMETER"
    assert err["details"]["field"] == "foo"
    # 重复字段。
    resp = client.get(
        f"{base_url}/api/v1/perception/events?after=1&after=2", timeout=2
    )
    assert resp.status_code == 400
    err = resp.json()["error"]
    assert err["code"] == "INVALID_QUERY_PARAMETER"
    assert err["details"]["field"] == "after"
    # 前导正号（parseStrictInt 拒绝）。
    resp = client.get(
        f"{base_url}/api/v1/perception/events?after=%2B5", timeout=2
    )
    assert resp.status_code == 400
    err = resp.json()["error"]
    assert err["code"] == "INVALID_QUERY_PARAMETER"
    assert err["details"]["field"] == "after"


# ---- 异常检测与接触事件 ----

def test_perception_anomaly_detection(client, base_url):
    """RESET 后异常源位置随机（真值不对外暴露，SPEC 6.8）。用 DFS 遍历全图
    所有可通行格，保证进入每个异常源的曼哈顿半径，断言四个异常源
    （含新增 oil-01）均产生 ANOMALY_DETECTED 事件。"""
    # 基线游标：只统计本次 RESET 之后的事件，避免共享服务实例下
    # 读取到其它测试遗留的历史事件。
    cursor = client.get(
        f"{base_url}/api/v1/perception/latest", timeout=2
    ).json()["latestEventId"]

    reset_body = _reset(client, base_url)
    start = (
        reset_body["robot"]["position"]["x"],
        reset_body["robot"]["position"]["y"],
    )

    map_info = client.get(f"{base_url}/api/v1/map", timeout=2).json()
    width = map_info["width"]
    height = map_info["height"]
    obstacles = {(o["x"], o["y"]) for o in map_info["obstacles"]}

    deltas = {
        "RIGHT": (1, 0),
        "DOWN": (0, 1),
        "LEFT": (-1, 0),
        "UP": (0, -1),
    }
    back = {(1, 0): "RIGHT", (-1, 0): "LEFT", (0, 1): "DOWN", (0, -1): "UP"}

    detected = set()

    def collect():
        nonlocal cursor
        while True:
            body = client.get(
                f"{base_url}/api/v1/perception/events?after={cursor}&limit=100",
                timeout=2,
            ).json()
            items = body["events"]
            for e in items:
                if e["type"] == "ANOMALY_DETECTED":
                    detected.add((e.get("hazard") or {}).get("hazardId"))
            if items:
                cursor = items[-1]["eventId"]
            if not body["hasMore"]:
                break

    visited = {start}
    stack = [start]
    moves = 0

    while stack:
        x, y = stack[-1]
        advanced = False
        for direction, (dx, dy) in deltas.items():
            nxt = (x + dx, y + dy)
            if not (0 <= nxt[0] < width and 0 <= nxt[1] < height):
                continue
            if nxt in obstacles or nxt in visited:
                continue
            resp = _post(client, base_url, {"type": "MOVE", "direction": direction})
            assert resp.status_code == 201, resp.text
            new_pos = resp.json()["robot"]["position"]
            assert (new_pos["x"], new_pos["y"]) == nxt, (direction, new_pos, nxt)
            visited.add(nxt)
            stack.append(nxt)
            advanced = True
            break
        if not advanced:
            stack.pop()
            if stack:
                parent = stack[-1]
                direction = back[(parent[0] - x, parent[1] - y)]
                resp = _post(client, base_url, {"type": "MOVE", "direction": direction})
                assert resp.status_code == 201, resp.text
                new_pos = resp.json()["robot"]["position"]
                assert (new_pos["x"], new_pos["y"]) == parent
        moves += 1
        # 事件环形缓冲容量 200，分页增量收集避免早期事件被淘汰。
        if moves % 25 == 0:
            collect()

    collect()
    assert detected == {"smoke-01", "temp-01", "gas-01", "oil-01"}, detected


def test_perception_contact_event(client, base_url):
    # 基线游标：隔离本次测试事件，避免读到其它测试（如 DFS 遍历）的异常事件。
    cursor = client.get(
        f"{base_url}/api/v1/perception/latest", timeout=2
    ).json()["latestEventId"]
    _reset(client, base_url)
    # 移动到 (4,3)，再向下撞 (4,4) 障碍。
    for _ in range(4):
        _post(client, base_url, {"type": "MOVE", "direction": "RIGHT"})
    for _ in range(3):
        _post(client, base_url, {"type": "MOVE", "direction": "DOWN"})
    resp = _post(client, base_url, {"type": "MOVE", "direction": "DOWN"})
    assert resp.json()["result"] == "BLOCKED_OBSTACLE"
    events = client.get(
        f"{base_url}/api/v1/perception/events?after={cursor}&limit=100", timeout=2
    ).json()
    contact = [
        e for e in events["events"] if e["type"] == "CONTACT_OBSTACLE"
    ]
    assert contact, "未发现 CONTACT_OBSTACLE 事件"
    assert contact[-1].get("contactResult") == "BLOCKED_OBSTACLE"
