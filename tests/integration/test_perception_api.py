"""虚拟感知黑盒集成测试：latest/events 契约、异常检测、接触事件与非法查询。

前置条件：被测服务已启动且启用感知（PERCEPTION_CONFIG 指向
config/perception.json：rangeCells=5、hazardRangeCells=2、异常源
smoke-01=(3,3)、temp-01=(12,12)、gas-01=(18,18)）。
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
    _reset(client, base_url)
    # 从 (0,0) 到 (3,3)：smoke-01 位于 (3,3)，曼哈顿半径 2。
    for _ in range(3):
        _post(client, base_url, {"type": "MOVE", "direction": "RIGHT"})
    for _ in range(3):
        _post(client, base_url, {"type": "MOVE", "direction": "DOWN"})
    events = client.get(
        f"{base_url}/api/v1/perception/events?after=0&limit=100", timeout=2
    ).json()
    detected = [
        e
        for e in events["events"]
        if e["type"] == "ANOMALY_DETECTED"
        and (e.get("hazard") or {}).get("hazardId") == "smoke-01"
    ]
    assert detected, "未发现 smoke-01 的 ANOMALY_DETECTED 事件"
    # 最新观察的活动检测包含 smoke-01。
    latest = client.get(
        f"{base_url}/api/v1/perception/latest", timeout=2
    ).json()
    active_ids = {d["hazardId"] for d in latest["activeDetections"]}
    assert "smoke-01" in active_ids


def test_perception_contact_event(client, base_url):
    _reset(client, base_url)
    # 移动到 (4,3)，再向下撞 (4,4) 障碍。
    for _ in range(4):
        _post(client, base_url, {"type": "MOVE", "direction": "RIGHT"})
    for _ in range(3):
        _post(client, base_url, {"type": "MOVE", "direction": "DOWN"})
    resp = _post(client, base_url, {"type": "MOVE", "direction": "DOWN"})
    assert resp.json()["result"] == "BLOCKED_OBSTACLE"
    events = client.get(
        f"{base_url}/api/v1/perception/events?after=0&limit=100", timeout=2
    ).json()
    contact = [
        e for e in events["events"] if e["type"] == "CONTACT_OBSTACLE"
    ]
    assert contact, "未发现 CONTACT_OBSTACLE 事件"
    assert contact[-1].get("contactResult") == "BLOCKED_OBSTACLE"
