import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch


ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "components/core/src"
WEB = ROOT / "components/web"


class WebApplicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        base = Path(cls.temporary.name)
        assets = base / "assets"
        assets.mkdir()
        (assets / "index.html").write_text("<!doctype html><title>test</title>",
                                           encoding="utf-8")
        manifest = {
            "schema_version": 1, "board_id": "test-board", "soc": "test-soc",
            "capabilities": ["canopen", "can-fd"],
            "runtime_profile": "lite", "channels": {
                "can0": {"kind": "can", "device": "can0", "role": "fieldbus"},
                "management0": {"kind": "ethernet", "device": "eth0",
                                "role": "management"},
                "display0": {"kind": "display", "device": "/dev/fb0",
                             "role": "local-hmi"},
            },
        }
        manifest_path = base / "platform.json"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        password = "host-test-password"
        salt = "host-test-salt"
        password_hash = hashlib.pbkdf2_hmac(
            "sha256", password.encode(), salt.encode(), 120000).hex()
        config = {"auth": {"username": "admin", "role": "administrator",
                           "salt": salt, "password_hash": password_hash,
                           "must_change": True}, "can": {}, "canopen": {},
                  "ec20": {"profiles": {}}}
        config_path = base / "config.json"
        config_path.write_text(json.dumps(config), encoding="utf-8")
        hmi_settings = base / "hmi.json"
        hmi_settings.write_text(json.dumps({"demo_mode": False}), encoding="utf-8")
        values = {
            "OMNIGATE_CORE_PATH": str(CORE),
            "OMNIGATE_PLATFORM_MANIFEST": str(manifest_path),
            "OMNIGATE_WEB_ASSETS": str(assets),
            "OMNIGATE_WEB_CONFIG": str(config_path),
            "OMNIGATE_WEB_SECRET": str(base / "secret.key"),
            "OMNIGATE_WEB_LOG": str(base / "web.log"),
            "OMNIGATE_EDGE_DB": str(base / "edge.db"),
            "OMNIGATE_HMI_DB": str(base / "hmi.db"),
            "OMNIGATE_HMI_SETTINGS": str(hmi_settings),
            "OMNIGATE_MODBUS_CONFIG": str(base / "modbus.json"),
            "OMNIGATE_CONFIG": str(config_path),
            "OMNIGATE_TB_CONFIG": str(base / "thingsboard.json"),
            "OMNIGATE_EDS_DIR": str(base / "eds"),
        }
        os.environ.update(values)
        sys.path[:0] = [str(CORE), str(WEB)]
        spec = importlib.util.spec_from_file_location("omnigate_web_test_app",
                                                      WEB / "app.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.app.config.update(TESTING=True)
        cls.app = module.app
        cls.password = password

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_hmi_settings_validation_persistence_and_failure(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as temporary, \
             patch.object(hmi, "SETTINGS_PATH", Path(temporary) / "settings.json"):
            client = self.app.test_client()
            initial = dict(hmi.DEFAULT_SETTINGS, page_cycle_seconds=20)
            hmi._atomic_json(hmi.SETTINGS_PATH, initial)
            for body in ([1], {"demo_mode": "false"}, {"demo_mode": 0},
                         {"brightness": 256}, {"refresh_ms": 499}):
                self.assertEqual(client.put("/api/hmi/v1/settings", json=body).status_code, 400)
                self.assertEqual(hmi._settings(), initial)
            body = {"brightness": 120, "demo_mode": False, "refresh_ms": 2000}
            with patch.object(hmi, "_atomic_json", side_effect=OSError("fixture storage failure")):
                self.assertEqual(client.put("/api/hmi/v1/settings", json=body).status_code, 500)
            self.assertEqual(hmi._settings(), initial)
            self.assertEqual(client.put("/api/hmi/v1/settings", json=body).get_json()["settings"],
                             dict(initial, **body))
            # Saving is separate from applying backlight; failed hardware write is not success.
            with patch.object(Path, "write_text", side_effect=OSError("fixture backlight missing")):
                result = client.post("/api/hmi/v1/actions", json={"action": "brightness",
                    "params": {"value": 120}, "confirmed": True})
            self.assertEqual(result.status_code, 500)
            self.assertEqual(hmi._settings()["brightness"], 120)
            with patch.object(Path, "write_text", return_value=3) as write, \
                 patch.object(hmi, "_atomic_json"):
                self.assertEqual(hmi._execute_action("brightness", {"value": 120}), {"brightness": 120})
                self.assertEqual(write.call_args_list[0].args, ("120",))

    def test_hmi_service_status_unknown_unavailable_and_recovery(self):
        import hmi_api as hmi
        with patch.object(hmi, "_service_available", return_value=False), patch.object(hmi, "_run") as run:
            self.assertEqual(hmi._service("thingsboard")["running"], None)
            run.assert_not_called()

        with patch.object(hmi, "_service_available", return_value=True), patch.object(hmi, "_run") as run:
            for code, expected in [(0, True), (1, False), (127, None), (2, None)]:
                run.return_value = {"returncode": code, "output": "host fixture"}
                self.assertIs(hmi._service("thingsboard")["running"], expected)
            run.side_effect = ValueError("操作超时")
            self.assertIsNone(hmi._service("thingsboard")["running"])
            run.reset_mock(); run.side_effect = None
            for name in ["ntp", "bluetooth"]:
                self.assertIsNone(hmi._service(name)["running"])
            run.assert_not_called()
            run.return_value = {"returncode": 0, "output": "running"}
            self.assertTrue(hmi._service("thingsboard")["running"])
        with patch.object(hmi.os.path, "isfile", return_value=True), \
             patch.object(hmi.os, "access", return_value=True), \
             patch.object(hmi, "PLATFORM", {"capabilities": []}):
            self.assertFalse(hmi._service_available("bluetooth"))
            self.assertTrue(hmi._service_available("thingsboard"))

    def test_native_brightness_routes_to_platform_and_never_falls_back(self):
        import hmi_api as hmi
        with patch.dict(os.environ, {'DSHANCONTROL_NATIVE_PLATFORM_SOCKET': '/run/test-native.sock'}), \
             patch.object(hmi, '_platform_request') as rpc, \
             patch.object(Path, 'write_text') as direct, \
             patch.object(hmi, '_atomic_json') as save:
            rpc.return_value = {'channel': 'display0', 'brightness': 122}
            self.assertEqual({'brightness': 122}, hmi._execute_action('brightness', {'value': 120}))
            rpc.assert_called_once_with('display.brightness.set', {
                'channel': 'display0', 'value': 120, 'confirmed': True})
            self.assertEqual(122, save.call_args.args[1]['brightness'])
            direct.assert_not_called()
            save.reset_mock()
            rpc.side_effect = RuntimeError('resource-lock/resource-busy: maintenance')
            with self.assertRaisesRegex(RuntimeError, 'resource-busy'):
                hmi._execute_action('brightness', {'value': 120})
            direct.assert_not_called(); save.assert_not_called()
            with self.assertRaises(ValueError):
                hmi._execute_action('brightness', {'value': 120, 'path': '/tmp/other'})

    def test_hmi_service_command_failure_audit_and_recovery(self):
        import hmi_api as hmi
        client = self.app.test_client()
        body = {"action": "service", "params": {"service": "thingsboard", "operation": "restart"},
                "confirmed": True}
        with patch.object(hmi, "_service_available", return_value=False), \
             patch.object(hmi.subprocess, "run") as run:
            self.assertEqual(client.post("/api/hmi/v1/actions", json=body).status_code, 400)
            run.assert_not_called()
        with patch.object(hmi, "_service_available", return_value=True), \
             patch.object(hmi.subprocess, "run") as run:
            for outcome in [SimpleNamespace(returncode=1, stdout="fixture failed"),
                            OSError("missing"), hmi.subprocess.TimeoutExpired("fixture", 25),
                            SimpleNamespace(returncode=0, stdout="command accepted")]:
                run.side_effect = outcome if isinstance(outcome, Exception) else None
                run.return_value = outcome
                response = client.post("/api/hmi/v1/actions", json=body)
                success = not isinstance(outcome, Exception) and outcome.returncode == 0
                self.assertEqual(response.status_code, 200 if success else 400)
                with hmi._db() as conn:
                    self.assertEqual(conn.execute("SELECT result FROM audit ORDER BY id DESC LIMIT 1").fetchone()[0],
                                     "success" if success else "failed")
            body["params"]["service"] = "ntp"
            run.return_value = SimpleNamespace(returncode=0, stdout="Starting ntpd: FAIL\n")
            self.assertEqual(client.post("/api/hmi/v1/actions", json=body).status_code, 400)

    def login(self, client):
        response = client.post("/api/login", json={
            "username": "admin", "password": self.password})
        self.assertEqual(response.status_code, 200)
        return response.get_json()["csrf"]

    @staticmethod
    def cellular_config():
        return {"ec20": {"profiles": {"ec20a": {"device": "/dev/cdc-wdm0", "interface": "ethCELL",
                "apn": "test-only", "username": "fixture", "password": "fixture-secret"}}}}

    def test_alarm_ack_identity_idempotency_and_transaction_recovery(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as folder, patch.object(hmi, "DB_PATH", Path(folder) / "hmi.db"), \
             patch.object(hmi, "_settings", return_value={"demo_mode": False}):
            hmi._init_db()
            client = self.app.test_client()
            with hmi._db() as conn:
                for alarm_id, state, source in [(2147483655, "active", "real"), (22, "active", "real"),
                                               (23, "cleared", "real"), (24, "active", "demo")]:
                    conn.execute("INSERT INTO alarms(id,ts,severity,source,code,message,state) VALUES(?,1,'warning',?,'x','message',?)",
                                 (alarm_id, source, state))
            for value, status in [(0, 400), (9007199254740992, 400), (999, 404), (23, 409), (24, 404)]:
                response = client.post("/api/hmi/v1/alarms/%d/ack" % value)
                self.assertEqual(response.status_code, status)
                self.assertTrue(response.get_json()["error"])
            with patch.object(hmi.time, "time", return_value=100):
                response = client.post("/api/hmi/v1/alarms/2147483655/ack")
            self.assertEqual(response.get_json(), {"ok": True, "id": 2147483655})
            with patch.object(hmi.time, "time", return_value=200):
                self.assertTrue(client.post("/api/hmi/v1/alarms/2147483655/ack").get_json()["already_acknowledged"])
            with hmi._db() as conn:
                row = dict(conn.execute("SELECT * FROM alarms WHERE id=2147483655").fetchone())
                self.assertEqual((row["state"], row["acknowledged_at"], row["acknowledged_by"]),
                                 ("acknowledged", 100, "local-hmi"))
                self.assertEqual(conn.execute("SELECT COUNT(*) FROM audit").fetchone()[0], 1)
                self.assertEqual(conn.execute("SELECT state FROM alarms WHERE id=22").fetchone()[0], "active")
                conn.execute("CREATE TRIGGER fail_ack_audit BEFORE INSERT ON audit BEGIN SELECT RAISE(ABORT, 'fixture audit storage failure'); END")
            failed = client.post("/api/hmi/v1/alarms/22/ack")
            self.assertEqual(failed.status_code, 500)
            self.assertEqual(failed.get_json()["error"], "内部服务错误")
            with hmi._db() as conn:
                row = conn.execute("SELECT state,acknowledged_at FROM alarms WHERE id=22").fetchone()
                self.assertEqual(tuple(row), ("active", None))
                conn.execute("DROP TRIGGER fail_ack_audit")
            self.assertTrue(client.post("/api/hmi/v1/alarms/22/ack").get_json()["ok"])
            rows = client.get("/api/hmi/v1/audit").get_json()["audit"]
            self.assertEqual({row["target"] for row in rows}, {"22", "2147483655"})
            self.assertTrue(all(row["action"] == "alarm_ack" and row["result"] == "success" for row in rows))

    def test_alarm_history_limits_and_demo_visibility(self):
        import hmi_api as hmi
        settings = {"demo_mode": False}
        with tempfile.TemporaryDirectory() as folder, patch.object(hmi, "DB_PATH", Path(folder) / "hmi.db"), \
             patch.object(hmi, "_settings", return_value=settings):
            hmi._init_db()
            with hmi._db() as conn:
                conn.executemany("INSERT INTO alarms(ts,severity,source,code,message) VALUES(?,'warning','real','x',?)",
                                 [(n, 'quoted, "温度"\r\nline') for n in range(505)])
                demo_id = conn.execute("INSERT INTO alarms(ts,severity,source,code,message) VALUES(999,'info','demo','x','demo')").lastrowid
            client = self.app.test_client()
            rows = client.get("/api/hmi/v1/alarms?limit=500").get_json()["alarms"]
            self.assertEqual(len(rows), 500)
            self.assertEqual([r["ts"] for r in rows], list(range(504, 4, -1)))
            self.assertEqual(rows[0]["message"], 'quoted, "温度"\r\nline')
            for limit in ["0", "501", "bad", "1.5"]:
                self.assertEqual(client.get("/api/hmi/v1/alarms?limit=" + limit).status_code, 400)
            settings["demo_mode"] = True
            self.assertEqual(client.get("/api/hmi/v1/alarms?limit=1").get_json()["alarms"][0]["id"], demo_id)
            self.assertTrue(client.post("/api/hmi/v1/alarms/%d/ack" % demo_id).get_json()["ok"])

    def test_audit_history_preserves_unknown_result_semantics(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as folder, patch.object(hmi, "DB_PATH", Path(folder) / "hmi.db"):
            hmi._init_db()
            with hmi._db() as conn:
                conn.executemany("INSERT INTO audit(ts,action,target,request_json,result,detail) VALUES(?,'custom','target','{}',?,'detail')",
                                 [(n, ["success", "failed", "pending", "unknown"][n % 4]) for n in range(205)])
            rows = self.app.test_client().get("/api/hmi/v1/audit").get_json()["audit"]
            self.assertEqual(len(rows), 200)
            self.assertEqual([r["ts"] for r in rows], list(range(204, 4, -1)))
            self.assertEqual({r["result"] for r in rows}, {"success", "failed", "pending", "unknown"})

    def test_cellular_mapping_gates_and_snapshot_no_secrets(self):
        import hmi_api as hmi
        platform = json.loads((ROOT / "deploy/t153-lite/platform.json").read_text())
        config = self.cellular_config()
        with tempfile.TemporaryDirectory() as folder, patch.object(hmi, "OMNIGATE_CONFIG", Path(folder) / "config.json"), \
             patch.object(hmi, "PLATFORM", platform):
            hmi.OMNIGATE_CONFIG.write_text(json.dumps(config))
            self.assertEqual(hmi._cellular_profile("ec20a"), config["ec20"]["profiles"]["ec20a"])
            with patch.object(hmi, "_interface", side_effect=lambda name: {
                    "name": name, "state": "down", "online": False, "address": ""}):
                data = hmi._snapshot()
            self.assertEqual(data["cellular"], {"ec20a": {"device": "/dev/cdc-wdm0", "interface": "ethCELL"}})
            self.assertNotIn("fixture-secret", json.dumps(data))
            kinds = {item["name"]: item["kind"] for item in data["networks"]}
            self.assertEqual(kinds, {"eth0": "ethernet", "eth1": "ethernet", "wlan0": "wifi", "ethCELL": "cellular"})
            self.assertTrue(all(item["source"] == "real" for item in data["networks"]))
            s4 = json.loads((ROOT / "boards/t113s4-sdnand/platform.json").read_text())
            for bad in (s4, dict(platform, capabilities=[]), dict(platform, channels={})):
                with patch.object(hmi, "PLATFORM", bad), patch.object(hmi, "_run") as run:
                    for op in ("connect", "disconnect"):
                        with self.assertRaises(ValueError):
                            hmi._execute_action("ec20", {"profile": "ec20a", "operation": op})
                    run.assert_not_called()
            for key, value in (("device", ""), ("device", "/dev/unmapped"), ("interface", ""),
                               ("interface", 42), ("interface", "../eth0"), ("interface", "eth1")):
                invalid = self.cellular_config(); invalid["ec20"]["profiles"]["ec20a"][key] = value
                hmi.OMNIGATE_CONFIG.write_text(json.dumps(invalid))
                with patch.object(hmi, "_run") as run:
                    with self.assertRaises(ValueError): hmi._cellular_profile("ec20a")
                    run.assert_not_called()
            duplicate = self.cellular_config()
            duplicate["ec20"]["profiles"]["ec20b"] = dict(duplicate["ec20"]["profiles"]["ec20a"])
            hmi.OMNIGATE_CONFIG.write_text(json.dumps(duplicate))
            with self.assertRaises(ValueError): hmi._cellular_profile("ec20a")

    def test_cellular_disconnect_requires_matching_session_and_reports_failure(self):
        import hmi_api as hmi
        cfg = self.cellular_config()["ec20"]["profiles"]["ec20a"]
        params = {"profile": "ec20a", "operation": "disconnect", "device": cfg["device"], "interface": cfg["interface"]}
        runtime = {"device": cfg["device"], "interface": cfg["interface"], "handle": "123"}
        with patch.object(hmi, "_cellular_profile", return_value=cfg):
            for invalid in ({}, dict(runtime, handle="0"), dict(runtime, handle="bad"),
                            dict(runtime, device="/dev/other"), dict(runtime, interface="other")):
                with patch.object(hmi, "_read_json", return_value=invalid), patch.object(hmi, "_run") as run:
                    with self.assertRaises(ValueError): hmi._execute_action("ec20", params)
                    run.assert_not_called()
            with patch.object(hmi, "_read_json", return_value=runtime):
                with patch.object(hmi, "_run", side_effect=RuntimeError("QMI failed")) as run:
                    with self.assertRaisesRegex(RuntimeError, "QMI failed"): hmi._execute_action("ec20", params)
                    self.assertEqual(run.call_count, 1)
                    self.assertTrue(run.call_args.kwargs["check"])
                with patch.object(hmi, "_run", return_value={"output": "", "returncode": 0}) as run:
                    self.assertEqual(hmi._execute_action("ec20", params)["state"], "disconnected")
                    self.assertEqual(run.call_count, 2)
                    self.assertTrue(all(call.kwargs["check"] for call in run.call_args_list))
                with patch.object(hmi, "_run") as run:
                    with self.assertRaises(ValueError): hmi._execute_action("ec20", dict(params, interface="changed"))
                    run.assert_not_called()

    def test_cellular_connect_missing_handle_dhcp_failure_and_recovery(self):
        import hmi_api as hmi
        cfg = self.cellular_config()["ec20"]["profiles"]["ec20a"]
        params = {"profile": "ec20a", "operation": "connect"}
        mode = {"value": "missing"}
        def command(args, **kwargs):
            if args[0] == "qmicli":
                return {"returncode": 0, "output": "Packet data handle: '123'" if mode["value"] != "missing" else "no handle"}
            if args[0] == "udhcpc" and mode["value"] == "dhcp":
                return {"returncode": 1, "output": "fixture DHCP failed"}
            return {"returncode": 0, "output": ""}
        with patch.object(hmi, "_cellular_profile", return_value=cfg), \
             patch.object(Path, "exists", return_value=True), patch.object(Path, "write_text"), \
             patch.object(hmi, "_run", side_effect=command), patch.object(hmi, "_atomic_json") as write:
            with self.assertRaisesRegex(RuntimeError, "会话句柄"): hmi._execute_action("ec20", params)
            write.assert_not_called()
            mode["value"] = "dhcp"
            with self.assertRaisesRegex(RuntimeError, "DHCP failed"): hmi._execute_action("ec20", params)
            # Preserve the handle so the established QMI session can still be disconnected.
            self.assertEqual(write.call_args.args[1]["handle"], "123")
            mode["value"] = "ok"
            self.assertEqual(hmi._execute_action("ec20", params)["state"], "connected")

    @staticmethod
    def serial_ethercat_platform():
        return {"capabilities": ["modbus-rtu", "ethercat"], "channels": {
            "rs": {"kind": "serial", "role": "modbus-rtu", "device": "/dev/ttyAS5"},
            "ec": {"kind": "ethernet", "role": "ethercat", "device": "ethTEST"}}}

    def test_serial_ethercat_require_capability_and_mapped_role(self):
        import hmi_api as hmi
        for action, role, channel, params in (
            ("rs485_config", "modbus-rtu", "rs", {"port": "/dev/ttyAS5"}),
            ("ethercat_cycle", "ethercat", "ec", {})):
            for fault in ("capability", "missing", "empty", "blank", "type", "role", "kind"):
                platform = self.serial_ethercat_platform()
                if fault == "capability": platform["capabilities"].remove(role)
                elif fault == "missing": del platform["channels"][channel]
                else:
                    key, value = {"empty": ("device", ""), "blank": ("device", "  "),
                                  "type": ("device", 42), "role": ("role", "management"),
                                  "kind": ("kind", "display")}[fault]
                    platform["channels"][channel][key] = value
                with self.subTest(action=action, fault=fault), patch.object(hmi, "PLATFORM", platform), \
                     patch.object(hmi, "_run") as run, patch.object(hmi, "_atomic_json") as write:
                    with self.assertRaises(ValueError): hmi._execute_action(action, params)
                    run.assert_not_called(); write.assert_not_called()
        # Actual T153 mapping remains usable; S4's undeclared fieldbus fails closed.
        for relative, allowed in (("deploy/t153-lite/platform.json", True),
                                  ("boards/t113s4-sdnand/platform.json", False)):
            with patch.object(hmi, "PLATFORM", json.loads((ROOT / relative).read_text())), \
                 patch.object(hmi, "_run", return_value={"returncode": 0}) as run:
                if allowed:
                    hmi._execute_action("ethercat_cycle", {})
                    self.assertEqual(run.call_args.args[0][1], "eth0")
                    self.assertEqual(hmi._control_devices("serial", "modbus-rtu"), ["/dev/ttyAS5"])
                else:
                    with self.assertRaises(ValueError): hmi._execute_action("ethercat_cycle", {})
                    with self.assertRaises(ValueError): hmi._control_devices("serial", "modbus-rtu")
                    run.assert_not_called()

    def test_rs485_validation_preserves_config_and_other_slaves(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as folder, \
             patch.object(hmi, "PLATFORM", self.serial_ethercat_platform()), \
             patch.object(hmi, "MODBUS_CONFIG", Path(folder) / "modbus.json"):
            cfg = {"master": {"slaves": [{"parity": "E", "timeseries": [{"address": 7}]},
                                          {"port": "/dev/ttyAS9", "unitId": 2}]}, "custom": True}
            hmi.MODBUS_CONFIG.write_text(json.dumps(cfg))
            original = hmi.MODBUS_CONFIG.read_bytes()
            params = dict(port="/dev/ttyAS5", baudrate=300, unit_id=1, poll_ms=50)
            for invalid in (dict(port="/dev/ttyAS9"), dict(port=""), dict(port="/dev/ttyAS5;reboot"),
                            dict(baudrate=299), dict(baudrate=4000001), dict(baudrate=True),
                            dict(unit_id=0), dict(unit_id=248), dict(poll_ms=49),
                            dict(poll_ms=3600001), dict(poll_ms=1.5)):
                with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                    hmi._execute_action("rs485_config", dict(params, **invalid))
                self.assertEqual(hmi.MODBUS_CONFIG.read_bytes(), original)
            with patch.object(hmi, "_run") as run:
                hmi._execute_action("rs485_config", params)
                hmi._execute_action("rs485_config", dict(params, baudrate=4000000, unit_id=247, poll_ms=3600000))
                run.assert_not_called()  # Saving configuration never restarts live collection.
            saved = json.loads(hmi.MODBUS_CONFIG.read_text())
            self.assertEqual(saved["master"]["slaves"][1], cfg["master"]["slaves"][1])
            self.assertEqual(saved["master"]["slaves"][0]["timeseries"], [{"address": 7}])
            self.assertEqual(saved["master"]["slaves"][0]["parity"], "E")
            self.assertEqual(saved["master"]["slaves"][0]["unitId"], 247)
            self.assertTrue(saved["custom"])
            before = hmi.MODBUS_CONFIG.read_bytes()
            with patch.object(hmi.os, "replace", side_effect=OSError("disk failure")):
                with self.assertRaises(OSError): hmi._execute_action("rs485_config", params)
            self.assertEqual(hmi.MODBUS_CONFIG.read_bytes(), before)
            hmi._execute_action("rs485_config", params)
            hmi.MODBUS_CONFIG.write_text('{"master":{"slaves":[]}}')
            with self.assertRaisesRegex(ValueError, "没有从站"):
                hmi._execute_action("rs485_config", params)

    def test_ethercat_arguments_boundaries_and_failures(self):
        import hmi_api as hmi
        with patch.object(hmi, "PLATFORM", self.serial_ethercat_platform()):
            params = dict(slave=65535, index=65535, subindex=255, value="ff " * 31 + "00")
            for invalid in (dict(slave=0), dict(slave=65536), dict(index=-1), dict(index=65536),
                            dict(subindex=256), dict(value=""), dict(value="0"), dict(value="GG"),
                            dict(value="ff " * 33), dict(value="01;reboot"), dict(interface="eth1")):
                with self.subTest(invalid=invalid), patch.object(hmi, "_run") as run:
                    with self.assertRaises(ValueError):
                        hmi._execute_action("ethercat_sdo_write", dict(params, **invalid))
                    run.assert_not_called()
            with patch.object(hmi, "_run", return_value={"returncode": 0}) as run:
                hmi._execute_action("ethercat_sdo_write", params)
                self.assertEqual(run.call_args.args[0], ["/usr/bin/omnigate-ethercat", "ethTEST", "sdo-write",
                                                       "65535", "0xffff", "255", params["value"]])
                self.assertTrue(run.call_args.kwargs["check"])
                hmi._execute_action("ethercat_cycle", dict(count=10000, period_us=1000))
                self.assertEqual(run.call_args.args[0][-2:], ["10000", "1000"])
            for params in (dict(count=0), dict(count=10001), dict(period_us=99),
                           dict(period_us=1000001), dict(count=10000, period_us=1000000)):
                with patch.object(hmi, "_run") as run:
                    with self.assertRaises(ValueError): hmi._execute_action("ethercat_cycle", params)
                    run.assert_not_called()
            for error in (ValueError("操作超时"), ValueError("从站未连接")):
                with patch.object(hmi, "_run", side_effect=error):
                    with self.assertRaises(ValueError): hmi._execute_action("ethercat_cycle", {})
            platform = self.serial_ethercat_platform()
            platform["channels"]["ec2"] = dict(platform["channels"]["ec"], device="eth2")
            with patch.object(hmi, "PLATFORM", platform), patch.object(hmi, "_run") as run:
                with self.assertRaises(ValueError): hmi._execute_action("ethercat_cycle", {})
                run.assert_not_called()

    def test_can_configuration_fd_and_failures(self):
        import hmi_api as hmi
        params = dict(interface="can0", bitrate=500000, fd=False, data_bitrate="ignored")
        with patch.object(hmi, "_run", return_value={"returncode": 0}) as run:
            hmi._execute_action("can_apply", params)
            self.assertEqual(run.call_args_list[1].args[0][-2:], ["fd", "off"])
            self.assertTrue(all(c.kwargs["check"] for c in run.call_args_list))
        with patch.object(hmi, "_run", return_value={"returncode": 0}) as run:
            hmi._execute_action("can_apply", dict(params, fd=True, data_bitrate=8000000))
            self.assertEqual(run.call_args_list[1].args[0][-4:], ["dbitrate", "8000000", "fd", "on"])
        for invalid in (dict(bitrate=9999), dict(bitrate=1000001), dict(bitrate=True),
                        dict(fd="false"), dict(fd=True, data_bitrate=499999),
                        dict(fd=True, data_bitrate=8000001), dict(interface="can9"),
                        dict(restart_ms=60001)):
            with self.subTest(invalid=invalid), patch.object(hmi, "_run") as run:
                with self.assertRaises(ValueError):
                    hmi._execute_action("can_apply", dict(params, **invalid))
                run.assert_not_called()
        with patch.object(hmi, "_run", side_effect=ValueError("down failed")) as run:
            with self.assertRaisesRegex(ValueError, "down failed"):
                hmi._execute_action("can_apply", params)
            self.assertEqual(run.call_count, 1)

    def test_canopen_rejects_invalid_before_connect(self):
        import hmi_api as hmi
        params = dict(interface="can0", node_id=1, index=0x2000, subindex=0, value=0, size=4)
        for invalid in (dict(interface=""), dict(interface="can9"), dict(node_id=0),
                        dict(node_id=128), dict(index=-1), dict(index=65536),
                        dict(index="bad"), dict(subindex=-1), dict(subindex=256),
                        dict(value=-2147483649), dict(value=4294967296),
                        dict(value=1.5), dict(size=0), dict(size=5),
                        dict(size=1, value=256), dict(size=1, value=-129)):
            with self.subTest(invalid=invalid), patch.object(hmi, "_can_connect") as connect:
                with self.assertRaises(ValueError):
                    hmi._execute_action("canopen_sdo_write", dict(params, **invalid))
                connect.assert_not_called()
        for invalid in (dict(node_id=-1), dict(node_id=128), dict(state="INVALID")):
            with patch.object(hmi, "_can_connect") as connect:
                with self.assertRaises(ValueError):
                    hmi._execute_action("canopen_nmt", dict(params, **invalid))
                connect.assert_not_called()
        with patch.object(hmi, "PLATFORM", {"channels": {}}), patch.object(hmi, "_can_connect") as connect:
            with self.assertRaises(ValueError):
                hmi._execute_action("canopen_nmt", params)
            connect.assert_not_called()

    def test_can_capabilities_fail_closed_for_missing_board_features(self):
        import hmi_api as hmi
        for capabilities in (None, [], ["display"]):
            platform = dict(hmi.PLATFORM)
            if capabilities is None:
                platform.pop("capabilities", None)
            else:
                platform["capabilities"] = capabilities
            with patch.object(hmi, "PLATFORM", platform), patch.object(hmi, "_run") as run, \
                 patch.object(hmi, "_can_connect") as connect:
                for action, params in (("can_apply", dict(interface="can0", bitrate=500000, fd=True)),
                                       ("canopen_nmt", dict(interface="can0", node_id=1, state="RESET")),
                                       ("canopen_sdo_write", dict(interface="can0", node_id=1, index=0, value=0))):
                    with self.assertRaises(ValueError):
                        hmi._execute_action(action, params)
                run.assert_not_called()
                connect.assert_not_called()

    def test_canopen_raw_sdo_bytes_nmt_and_disconnect(self):
        import hmi_api as hmi
        for value, size, payload in ((-2147483648, 4, b"\x00\x00\x00\x80"),
                                     (4294967295, 4, b"\xff" * 4),
                                     (0x1234, 2, b"\x34\x12"), (-1, 1, b"\xff")):
            network = Mock()
            with patch.object(hmi, "_can_connect", return_value=network):
                hmi._execute_action("canopen_sdo_write", dict(
                    interface="can0", node_id=127, index=65535, subindex=255, value=value, size=size))
            network.add_node.assert_called_once_with(127)
            network.add_node.return_value.sdo.download.assert_called_once_with(65535, 255, payload)
            network.disconnect.assert_called_once_with()
        for node in (0, 127):
            network = Mock()
            with patch.object(hmi, "_can_connect", return_value=network):
                hmi._execute_action("canopen_nmt", dict(interface="can0", node_id=node, state="RESET"))
            if node == 0:
                network.add_node.assert_not_called()
                self.assertEqual(network.nmt.state, "RESET")
            else:
                network.add_node.assert_called_once_with(node)
                self.assertEqual(network.add_node.return_value.nmt.state, "RESET")
            network.disconnect.assert_called_once_with()
        network = Mock()
        network.add_node.return_value.sdo.download.side_effect = RuntimeError("SDO aborted")
        with patch.object(hmi, "_can_connect", return_value=network):
            with self.assertRaisesRegex(RuntimeError, "SDO aborted"):
                hmi._execute_action("canopen_sdo_write", dict(
                    interface="can0", node_id=1, index=0, value=0))
        network.disconnect.assert_called_once_with()

    def test_can_http_confirmation_and_failed_audit(self):
        import hmi_api as hmi
        client = self.app.test_client()
        body = dict(action="canopen_nmt", params=dict(interface="can0", node_id=0, state="RESET"))
        for confirmed in (False, None, "true", "false", 1):
            with patch.object(hmi, "_can_connect") as connect:
                response = client.post("/api/hmi/v1/actions", json=dict(body, confirmed=confirmed))
                self.assertEqual(response.status_code, 400)
                connect.assert_not_called()
        with patch.object(hmi, "_can_connect", side_effect=ValueError("host test disconnected")):
            response = client.post("/api/hmi/v1/actions", json=dict(body, confirmed=True))
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.get_json()["error"], "host test disconnected")
        with hmi._db() as conn:
            row = conn.execute("SELECT result,detail FROM audit WHERE detail=?", ("host test disconnected",)).fetchone()
        self.assertEqual(row["result"], "failed")

    def test_security_login_and_standard_errors(self):
        client = self.app.test_client()
        self.assertEqual(client.get("/api/v1/platform").status_code, 401)
        health = client.get("/api/health")
        self.assertEqual(health.status_code, 200)
        self.assertEqual(health.headers["X-Frame-Options"], "DENY")
        self.login(client)
        self.assertEqual(client.get("/does-not-exist").status_code, 404)
        self.assertEqual(client.post(
            "/api/login", data="not-json", content_type="application/json"
        ).status_code, 400)

    def test_device_point_crud_csrf_and_audit(self):
        client = self.app.test_client()
        csrf = self.login(client)
        headers = {"X-CSRF-Token": csrf}
        device = {"name": "Meter", "template_id": "meter.v1",
                  "channel": "can0", "address": "7"}
        response = client.put("/api/v1/devices/meter-1", json=device,
                              headers=headers)
        self.assertEqual(response.get_json()["revision"], 1)
        point = {"device_id": "meter-1", "name": "Temperature",
                 "data_type": "float", "unit": "C", "access": "r",
                 "address": "0x1000:1", "scale": 0.1}
        self.assertEqual(client.put(
            "/api/v1/points/meter-1.temperature", json=point,
            headers=headers).status_code, 200)
        self.assertEqual(client.get(
            "/api/v1/points/meter-1.temperature").get_json()["point"]["unit"], "C")
        self.assertEqual(client.put(
            "/api/v1/points/no-csrf", json=point).status_code, 403)
        audit = client.get("/api/v1/audit").get_json()["audit"]
        self.assertIn("point.put", [row["action"] for row in audit])

    def test_hmi_platform_comes_from_board_manifest(self):
        value = self.app.test_client().get("/api/hmi/v1/snapshot").get_json()
        self.assertEqual(value["platform"]["board_id"], "test-board")
        self.assertEqual(value["platform"]["channels"]["can0"]["device"], "can0")
        self.assertNotIn("can1", value["platform"]["channels"])

    def test_hmi_demo_is_off_by_default(self):
        response = self.app.test_client().get("/api/hmi/v1/snapshot")
        self.assertEqual(response.status_code, 200)
        value = response.get_json()
        self.assertFalse(value["settings"]["demo_mode"])
        self.assertEqual(value["sensors"], [])
        self.assertNotIn("演示", json.dumps(value, ensure_ascii=False))

    def test_hmi_no_alarms_does_not_certify_system_health(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as temporary, \
                patch.object(hmi, "DB_PATH", Path(temporary) / "hmi.db"), \
                patch.object(hmi, "_settings", return_value=dict(hmi.DEFAULT_SETTINGS)), \
                patch.object(hmi, "_run", return_value={"returncode": 127, "output": "unavailable"}), \
                patch.object(hmi, "_interface") as interface, \
                patch.object(hmi, "_can") as can, \
                patch.object(hmi, "_cpu_percent", return_value=0.0), \
                patch.object(hmi, "_memory_percent", return_value=25.0), \
                patch.object(hmi, "_temperature", return_value=42.5), \
                patch.object(hmi, "_storage_percent", return_value=30.0), \
                patch.object(hmi, "_uptime_seconds", return_value=100):
            hmi._init_db()
            client = self.app.test_client()
            for state, online in (("unknown", None), ("DOWN", False), ("UP", True)):
                link = {"name": "fixture", "state": state, "online": online, "source": "real"}
                interface.return_value = can.return_value = link
                with self.subTest(state=state):
                    response = client.get("/api/hmi/v1/snapshot")
                    self.assertEqual(response.status_code, 200)
                    value = response.get_json()
                    self.assertEqual(value["alarms"]["active"], 0)
                    self.assertEqual(value["summary"]["status"], "未验证")
                    self.assertEqual(value["sensors"], [])
            # An empty board configuration is not evidence of healthy devices.
            with patch.dict(hmi.PLATFORM, {"channels": {}}):
                value = client.get("/api/hmi/v1/snapshot").get_json()
                self.assertEqual(value["summary"]["online_devices"], 0)
                self.assertEqual(value["summary"]["status"], "未验证")

    def test_hmi_summary_alarm_ack_and_demo_transitions(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as temporary, \
                patch.object(hmi, "DB_PATH", Path(temporary) / "hmi.db"), \
                patch.object(hmi, "SETTINGS_PATH", Path(temporary) / "settings.json"), \
                patch.object(hmi, "_run", return_value={"returncode": 127, "output": "unavailable"}):
            hmi._init_db()
            client = self.app.test_client()

            def check(status, count):
                response = client.get("/api/hmi/v1/snapshot")
                self.assertEqual(response.status_code, 200)
                value = response.get_json()
                self.assertEqual(value["summary"]["status"], status)
                self.assertEqual(value["alarms"]["active"], count)

            check("未验证", 0)
            with hmi._db() as conn:
                alarm_id = conn.execute(
                    "INSERT INTO alarms(ts,severity,source,code,message) VALUES(?,?,?,?,?)",
                    (int(hmi.time.time()), "warning", "can0", "LINK_LOST", "link lost")
                ).lastrowid
            check("有告警", 1)
            self.assertTrue(client.post("/api/hmi/v1/alarms/%d/ack" % alarm_id).get_json()["ok"])
            # Acknowledgement does not prove that the fault recovered.
            check("未验证", 0)
            self.assertEqual(client.put("/api/hmi/v1/settings", json={"demo_mode": True}).status_code, 200)
            check("有告警", 1)
            demo = client.get("/api/hmi/v1/alarms").get_json()["alarms"]
            for alarm in demo:
                if alarm["source"] == "demo" and alarm["state"] == "active":
                    self.assertTrue(client.post("/api/hmi/v1/alarms/%d/ack" % alarm["id"]).get_json()["ok"])
            check("演示", 0)
            # Real alarms retain priority even when the samples are simulated.
            with hmi._db() as conn:
                conn.execute("UPDATE alarms SET state='active' WHERE id=?", (alarm_id,))
            check("有告警", 1)
            self.assertTrue(client.post("/api/hmi/v1/alarms/%d/ack" % alarm_id).get_json()["ok"])
            self.assertEqual(client.put("/api/hmi/v1/settings", json={"demo_mode": False}).status_code, 200)
            check("未验证", 0)

    def test_hmi_cpu_requires_a_measured_interval(self):
        import hmi_api as hmi
        samples = [
            "cpu 100 20 30 400 10 5 5 0 50 10",
            # 20 busy + 80 idle ticks; guest ticks must not be counted twice.
            "cpu 120 20 30 480 10 5 5 0 70 10",
            "cpu 120 20 30 580 10 5 5 0 70 10",
            "cpu 220 20 30 580 10 5 5 0 70 10",
        ]
        with patch.object(hmi, "CPU_PREVIOUS", None), \
                patch.object(hmi, "_read", side_effect=samples), \
                patch.object(hmi.os, "getloadavg", side_effect=AssertionError("load is not CPU utilization")):
            self.assertIsNone(hmi._cpu_percent())
            self.assertEqual(hmi._cpu_percent(), 20.0)
            self.assertEqual(hmi._cpu_percent(), 0.0)
            self.assertEqual(hmi._cpu_percent(), 100.0)

    def test_hmi_cpu_invalid_samples_reset_baseline_and_recover(self):
        import hmi_api as hmi
        for invalid in ("", "\n", "cpu", "cpu 1 2 3", "cpu0 1 2 3 4",
                        "cpu 1 x 3 4", "cpu 1 2 3 -1", "cpu 1 2 nan 4"):
            with self.subTest(sample=invalid), patch.object(hmi, "CPU_PREVIOUS", None), \
                    patch.object(hmi, "_read", side_effect=[
                        "cpu 10 0 0 90", invalid, "cpu 50 0 0 150", "cpu 75 0 0 225"]):
                self.assertIsNone(hmi._cpu_percent())
                self.assertIsNone(hmi._cpu_percent())
                self.assertIsNone(hmi._cpu_percent())
                self.assertEqual(hmi._cpu_percent(), 25.0)
        # Repeated counters, a counter going backwards despite a growing total,
        # a reset, and a changed format all invalidate that interval.
        for baseline, following in (
                ("cpu 10 0 0 90", "cpu 10 0 0 90"),
                ("cpu 10 0 0 90", "cpu 9 0 0 191"),
                ("cpu 100 0 0 900", "cpu 10 0 0 90"),
                ("cpu 10 0 0 90 0", "cpu 10 0 0 90")):
            with self.subTest(baseline=baseline, following=following), \
                    patch.object(hmi, "CPU_PREVIOUS", None), \
                    patch.object(hmi, "_read", side_effect=[baseline, following, "cpu 20 0 0 220"]):
                self.assertIsNone(hmi._cpu_percent())
                self.assertIsNone(hmi._cpu_percent())
                self.assertGreaterEqual(hmi._cpu_percent(), 0.0)

    def test_hmi_memory_missing_or_invalid_is_not_zero(self):
        import hmi_api as hmi
        for sample in ("", "MemAvailable: 50 kB", "MemTotal: 100 kB",
                       "MemTotal: 100 kB\nMemFree: 50 kB",
                       "MemTotal: 0 kB\nMemAvailable: 0 kB",
                       "MemTotal: 100 kB\nMemAvailable: 101 kB",
                       "MemTotal: -100 kB\nMemAvailable: 0 kB",
                       "MemTotal: 100 kB\nMemAvailable: -1 kB",
                       "MemTotal: 100 kB\nMemAvailable: nan kB",
                       "MemTotal: 100 kB\nMemAvailable:",
                       "MemTotal: 100 MB\nMemAvailable: 50 kB"):
            with self.subTest(sample=sample), patch.object(hmi, "_read", return_value=sample):
                self.assertIsNone(hmi._memory_percent())
        for available, expected in ((100, 0.0), (0, 100.0), (75, 25.0)):
            with patch.object(hmi, "_read", return_value="MemTotal: 100 kB\nMemAvailable: %s kB" % available):
                self.assertEqual(hmi._memory_percent(), expected)

    def test_hmi_uptime_missing_nonfinite_and_real_zero(self):
        import hmi_api as hmi
        with patch.object(hmi.Path, "read_text", side_effect=PermissionError("proc unavailable")), \
                patch.object(hmi, "CPU_PREVIOUS", None):
            self.assertIsNone(hmi._cpu_percent())
            self.assertIsNone(hmi._memory_percent())
            self.assertIsNone(hmi._uptime_seconds())
        for sample in ("", "garbage", "nan 0", "inf 0", "-inf 0", "-1 0"):
            with self.subTest(sample=sample), patch.object(hmi, "_read", return_value=sample):
                self.assertIsNone(hmi._uptime_seconds())
        for sample, expected in (("0.0 0.0", 0), ("123.75 456.0", 123)):
            with patch.object(hmi, "_read", return_value=sample):
                self.assertEqual(hmi._uptime_seconds(), expected)

    def test_hmi_storage_failure_and_real_zero(self):
        import hmi_api as hmi
        with patch.object(hmi.shutil, "disk_usage", side_effect=OSError("unavailable")):
            self.assertIsNone(hmi._storage_percent())
        for total, used, expected in ((0, 0, None), (100, -1, None), (100, 101, None),
                                      (100, 0, 0.0), (100, 25, 25.0)):
            with self.subTest(total=total, used=used), patch.object(
                    hmi.shutil, "disk_usage", return_value=SimpleNamespace(total=total, used=used)):
                self.assertEqual(hmi._storage_percent(), expected)

    def test_hmi_temperature_uses_sysfs_millidegrees_at_all_magnitudes(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as temporary:
            sensor = Path(temporary) / "temp"
            with patch.object(hmi.Path, "glob", return_value=[sensor]):
                for raw, expected in (("42500\n", 42.5), ("500", 0.5),
                                      ("1000", 1.0), ("1001", 1.0),
                                      ("-1000", -1.0), ("-500", -0.5),
                                      ("0", 0.0), ("100", 0.1),
                                      ("-19900", -19.9), ("149900", 149.9)):
                    with self.subTest(raw=raw):
                        sensor.write_text(raw)
                        self.assertEqual(hmi._temperature(), expected)

    def test_hmi_temperature_invalid_missing_and_recovery(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as temporary:
            sensor = Path(temporary) / "temp"
            with patch.object(hmi.Path, "glob", return_value=[]):
                self.assertIsNone(hmi._temperature())
            with patch.object(hmi.Path, "glob", return_value=[sensor]):
                self.assertIsNone(hmi._temperature())  # Disappeared sysfs node.
                for raw in ("", "broken", "nan", "inf", "-inf", "500.0",
                            "500 C", "-20000", "150000", "9" * 400):
                    with self.subTest(raw=raw):
                        sensor.write_text(raw)
                        self.assertIsNone(hmi._temperature())
                        sensor.write_text("500\n")
                        self.assertEqual(hmi._temperature(), 0.5)
                with patch.object(hmi.Path, "read_text", side_effect=PermissionError("denied")):
                    self.assertIsNone(hmi._temperature())
                self.assertEqual(hmi._temperature(), 0.5)
            invalid = Path(temporary) / "a-invalid"
            invalid.write_text("nan")
            # A bad zone must not hide another valid zone.
            with patch.object(hmi.Path, "glob", return_value=[sensor, invalid]):
                self.assertEqual(hmi._temperature(), 0.5)

    def test_hmi_temperature_snapshot_and_history_use_converted_readings(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as temporary, \
                patch.object(hmi, "DB_PATH", Path(temporary) / "hmi.db"), \
                patch.object(hmi, "LAST_TELEMETRY_BUCKET", None), \
                patch.object(hmi.time, "time", return_value=1800000000) as now, \
                patch.object(hmi, "_run", return_value={"returncode": 127, "output": "unavailable"}):
            hmi._init_db()
            sensor = Path(temporary) / "temp"
            client = self.app.test_client()
            points = []
            with patch.object(hmi.Path, "glob", return_value=[sensor]):
                for raw, expected in (("500", 0.5), ("nan", None),
                                      ("-1000", -1.0), ("0", 0.0), ("42500", 42.5)):
                    sensor.write_text(raw)
                    response = client.get("/api/hmi/v1/snapshot")
                    self.assertEqual(response.status_code, 200)
                    system = response.get_json()["system"]
                    self.assertEqual(system["temperature"], expected)
                    self.assertEqual(system["temperature_source"],
                                     "unavailable" if expected is None else "real")
                    points.append({"bucket": int(now.return_value), "value": expected})
                    now.return_value += 60
            with hmi._db() as conn:
                rows = conn.execute("SELECT bucket,temperature FROM telemetry ORDER BY bucket").fetchall()
                self.assertEqual([tuple(row) for row in rows],
                                 [(point["bucket"], point["value"]) for point in points])
            history = client.get("/api/hmi/v1/history?metric=temperature")
            self.assertEqual(history.status_code, 200)
            self.assertEqual(history.get_json()["points"], points)

    def test_hmi_snapshot_and_history_preserve_missing_metrics(self):
        import hmi_api as hmi
        with tempfile.TemporaryDirectory() as temporary, \
                patch.object(hmi, "DB_PATH", Path(temporary) / "hmi.db"), \
                patch.object(hmi, "CPU_PREVIOUS", None), \
                patch.object(hmi, "LAST_TELEMETRY_BUCKET", None), \
                patch.object(hmi, "_read", return_value=""), \
                patch.object(hmi, "_temperature", return_value=None) as temperature, \
                patch.object(hmi.shutil, "disk_usage", side_effect=OSError("unavailable")) as disk, \
                patch.object(hmi.time, "time", return_value=1800000000) as now, \
                patch.object(hmi, "_run", return_value={"returncode": 127, "output": "unavailable"}):
            hmi._init_db()
            client = self.app.test_client()
            response = client.get("/api/hmi/v1/snapshot")
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.get_json()["summary"]["status"], "未验证")
            system = response.get_json()["system"]
            for metric in ("cpu", "memory", "temperature", "storage", "uptime_seconds"):
                self.assertIsNone(system[metric], metric)
            self.assertEqual(system["temperature_source"], "unavailable")
            with hmi._db() as conn:
                row = conn.execute("SELECT cpu,memory,temperature,storage FROM telemetry").fetchone()
                self.assertEqual(tuple(row), (None, None, None, None))

            # Recover real samples, including genuine zero values. CPU still
            # needs two valid readings after the failed read.
            reads = {"/proc/stat": "cpu 10 0 0 90",
                     "/proc/meminfo": "MemTotal: 100 kB\nMemAvailable: 100 kB",
                     "/proc/uptime": "0.0 0.0"}
            disk.side_effect = None
            disk.return_value = SimpleNamespace(total=100, used=0)
            temperature.return_value = 0.0
            with patch.object(hmi, "_read", side_effect=lambda path, default="": reads.get(str(path), default)):
                self.assertIsNone(client.get("/api/hmi/v1/snapshot").get_json()["system"]["cpu"])
                reads["/proc/stat"] = "cpu 10 0 0 190"
                now.return_value += 60
                recovered = client.get("/api/hmi/v1/snapshot").get_json()
                self.assertEqual(recovered["summary"]["status"], "未验证")
                system = recovered["system"]
                for metric in ("cpu", "memory", "temperature", "storage", "uptime_seconds"):
                    self.assertEqual(system[metric], 0, metric)
                self.assertEqual(system["temperature_source"], "real")
            for metric in ("cpu", "memory", "temperature", "storage"):
                history = client.get("/api/hmi/v1/history?metric=" + metric)
                self.assertEqual(history.status_code, 200)
                self.assertEqual(history.get_json()["points"], [
                    {"bucket": 1800000000, "value": None},
                    {"bucket": 1800000060, "value": 0.0}])


if __name__ == "__main__":
    unittest.main()
