package main

import (
	"bytes"
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"syscall"
	"testing"
	"time"
)

type fakeTuya struct {
	mu                     sync.Mutex
	posts                  []any
	online, drop, reported bool
	kind                   string
}

func (f *fakeTuya) call(_ context.Context, method, path string, body any) (any, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if method == "POST" {
		f.posts = append(f.posts, body)
		if f.drop {
			return nil, errors.New("private upstream error")
		}
		return true, nil
	}
	if strings.HasSuffix(path, "/functions") {
		limits := `{}`
		if f.kind == "Integer" {
			limits = `{"min":0,"max":100,"step":5}`
		}
		if f.kind == "Enum" {
			limits = `{"range":["off","on"]}`
		}
		return map[string]any{"functions": []any{map[string]any{"code": "switch", "type": f.kind, "values": limits}}}, nil
	}
	return map[string]any{"id": "lamp", "name": "fixture", "online": f.online, "status": []any{map[string]any{"code": "switch", "value": f.reported}}}, nil
}
func fixture(t *testing.T) (config, *fakeTuya, *gateway) {
	t.Helper()
	root := t.TempDir()
	if err := os.Chmod(root, 0700); err != nil {
		t.Fatal(err)
	}
	token := filepath.Join(root, "token")
	if err := os.WriteFile(token, []byte("test-restricted-session-0000000000000000"), 0600); err != nil {
		t.Fatal(err)
	}
	c := config{DesktopTokenFile: token, CommandDatabase: filepath.Join(root, "commands.db"), Devices: map[string][]string{"lamp": {"switch"}}}
	f := &fakeTuya{online: true, kind: "boolean"}
	g, err := newGateway(c, f)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(g.store.close)
	return c, f, g
}
func request(g *gateway, method, path, key, body string) *httptest.ResponseRecorder {
	r := httptest.NewRequest(method, path, strings.NewReader(body))
	r.Header.Set("Authorization", "Bearer "+g.token)
	r.Header.Set("Content-Type", "application/json")
	if key != "" {
		r.Header.Set("Idempotency-Key", key)
	}
	w := httptest.NewRecorder()
	g.ServeHTTP(w, r)
	return w
}
func TestSignatureCanonicalBodyTokenPath(t *testing.T) {
	body := []byte(`{"x":1}`)
	sum := sha256.Sum256(body)
	canonical := "POST\n" + hex.EncodeToString(sum[:]) + "\n\n/v1.0/devices/lamp/commands"
	mac := hmac.New(sha256.New, []byte("secret"))
	mac.Write([]byte("clienttoken1700000000000" + canonical))
	expected := strings.ToUpper(hex.EncodeToString(mac.Sum(nil)))
	if signature("client", "secret", "token", "1700000000000", "POST", "/v1.0/devices/lamp/commands", body) != expected {
		t.Fatal("signature mismatch")
	}
}
func TestCommandsEnvelopeIdempotencyAndRestart(t *testing.T) {
	c, f, g := fixture(t)
	body := `{"code":"switch","value":true}`
	key := "0123456789abcdef0123456789abcdef"
	if w := request(g, "GET", "/v1/devices", "", ""); w.Code != 200 || !strings.Contains(w.Body.String(), "lamp") {
		t.Fatal(w.Code, w.Body.String())
	}
	if w := request(g, "GET", "/v1/devices/lamp/functions", "", ""); w.Code != 200 || !strings.Contains(w.Body.String(), `"type":"Boolean"`) {
		t.Fatal(w.Code, w.Body.String())
	}
	if w := request(g, "POST", "/v1/devices/lamp/commands", key, body); w.Code != 200 || !strings.Contains(w.Body.String(), `"state_confirmed":false`) {
		t.Fatal(w.Code, w.Body.String())
	}
	g.store.close()
	restarted, err := newGateway(c, f)
	if err != nil {
		t.Fatal(err)
	}
	defer restarted.store.close()
	if w := request(restarted, "POST", "/v1/devices/lamp/commands", key, body); w.Code != 409 {
		t.Fatal(w.Code, w.Body.String())
	}
	encoded, _ := json.Marshal(f.posts)
	if string(encoded) != `[{"commands":[{"code":"switch","value":true}]}]` {
		t.Fatal(string(encoded))
	}
}
func TestUnknownOutcomeLegacyRecordAndConcurrentReservation(t *testing.T) {
	c, f, g := fixture(t)
	// This is exactly the Python gateway's existing schema/unknown record.
	if err := g.store.exec("INSERT INTO commands VALUES (?,?,?)", "legacy-key-0000000000", "old-digest", "unknown"); err != nil {
		t.Fatal(err)
	}
	g.store.close()
	g, err := newGateway(c, f)
	if err != nil {
		t.Fatal(err)
	}
	defer g.store.close()
	if w := request(g, "POST", "/v1/devices/lamp/commands", "legacy-key-0000000000", `{"code":"switch","value":true}`); w.Code != 409 {
		t.Fatal(w.Code)
	}
	f.drop = true
	var wg sync.WaitGroup
	codes := make(chan int, 12)
	for i := 0; i < 12; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			codes <- request(g, "POST", "/v1/devices/lamp/commands", "concurrent-key-0000000000", `{"code":"switch","value":true}`).Code
		}()
	}
	wg.Wait()
	close(codes)
	failed, duplicates := 0, 0
	for code := range codes {
		if code == 502 {
			failed++
		} else if code == 409 {
			duplicates++
		} else {
			t.Fatal(code)
		}
	}
	if failed != 1 || duplicates != 11 || len(f.posts) != 1 {
		t.Fatal(failed, duplicates, len(f.posts))
	}
	g.store.close()
	g, err = newGateway(c, f)
	if err != nil {
		t.Fatal(err)
	}
	defer g.store.close()
	if w := request(g, "POST", "/v1/devices/lamp/commands", "concurrent-key-0000000000", `{"code":"switch","value":true}`); w.Code != 409 {
		t.Fatal(w.Code)
	}
}
func TestAuthorizationTypesLimitsAndStrictJSON(t *testing.T) {
	_, f, g := fixture(t)
	w := httptest.NewRecorder()
	g.ServeHTTP(w, httptest.NewRequest("GET", "/v1/devices", nil))
	if w.Code != 403 {
		t.Fatal(w.Code)
	}
	cases := []struct {
		body, kind string
		status     int
	}{
		{`{"code":"switch","value":"true"}`, "Boolean", 400},
		{`{"commands":[]}`, "Boolean", 400},
		{`{"code":"switch","code":"switch","value":true}`, "Boolean", 400},
		{`{"code":"unauthorized","value":true}`, "Boolean", 403},
		{`{"code":"switch","value":7}`, "Integer", 400},
		{`{"code":"switch","value":5}`, "Integer", 200},
		{`{"code":"switch","value":true}`, "Integer", 400},
		{`{"code":"switch","value":101}`, "Integer", 400},
		{`{"code":"switch","value":"wrong"}`, "Enum", 400},
		{`{"code":"switch","value":"on"}`, "Enum", 200},
	}
	for i, c := range cases {
		f.kind = c.kind
		w := request(g, "POST", "/v1/devices/lamp/commands", fmt.Sprintf("boundary-key-%016d", i), c.body)
		if w.Code != c.status {
			t.Fatalf("case %d: %d %s", i, w.Code, w.Body.String())
		}
	}
	f.online = false
	if w := request(g, "POST", "/v1/devices/lamp/commands", "offline-key-0000000000", `{"code":"switch","value":true}`); w.Code != 409 {
		t.Fatal(w.Code)
	}
	if len(f.posts) != 2 {
		t.Fatal(f.posts)
	}
}
func TestPrivateFilesRefuseLinksAndWorldReadableCredentials(t *testing.T) {
	root := t.TempDir()
	secret := filepath.Join(root, "secret")
	if err := os.WriteFile(secret, []byte("sensitive"), 0644); err != nil {
		t.Fatal(err)
	}
	if _, err := readPrivate(secret); err == nil {
		t.Fatal("public secret accepted")
	}
	if err := os.Chmod(secret, 0600); err != nil {
		t.Fatal(err)
	}
	link := filepath.Join(root, "linked")
	if err := os.Symlink(secret, link); err != nil {
		t.Fatal(err)
	}
	if _, err := readPrivate(link); err == nil {
		t.Fatal("linked secret accepted")
	}
}
func TestRealTLSUpstreamSigningTokenRefreshAndNoRedirect(t *testing.T) {
	var mu sync.Mutex
	tokens, posts := 0, 0
	upstream := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		raw, _ := io.ReadAll(r.Body)
		token := r.Header.Get("access_token")
		if r.Header.Get("sign") != signature("client", "secret", token, r.Header.Get("t"), r.Method, r.URL.RequestURI(), raw) {
			t.Error("upstream signature invalid")
			w.WriteHeader(403)
			return
		}
		mu.Lock()
		defer mu.Unlock()
		if r.URL.Path == "/v1.0/token" {
			tokens++
			writeJSON(w, 200, map[string]any{"success": true, "result": map[string]any{"access_token": "fixture-token", "expire_time": 3600}})
			return
		}
		if token != "fixture-token" {
			t.Error("missing token")
		}
		if r.Method == "POST" {
			posts++
			writeJSON(w, 200, map[string]any{"success": true, "result": true})
			return
		}
		http.Redirect(w, r, "https://example.invalid/private", 302)
	}))
	defer upstream.Close()
	client := upstream.Client()
	client.CheckRedirect = func(*http.Request, []*http.Request) error { return errors.New("redirect refused") }
	api := &tuya{endpoint: upstream.URL, clientID: "client", secret: "secret", http: client}
	for i := 0; i < 2; i++ {
		if _, err := api.call(context.Background(), "POST", "/v1.0/devices/lamp/commands", map[string]any{"commands": []any{map[string]any{"code": "switch", "value": true}}}); err != nil {
			t.Fatal(err)
		}
	}
	if tokens != 1 || posts != 2 {
		t.Fatal(tokens, posts)
	}
	api.expires = time.Time{}
	if _, err := api.call(context.Background(), "GET", "/redirect", nil); err == nil {
		t.Fatal("redirect accepted")
	}
	if tokens != 2 || posts != 2 {
		t.Fatal(tokens, posts)
	}
}
func TestRealQtDesktopHTTPSGatewayReportedState(t *testing.T) {
	binary := os.Getenv("DSHANCONTROL_DESKTOP_TEST_BINARY")
	if binary == "" {
		t.Skip("native desktop executable required")
	}
	c, f, g := fixture(t)
	server := httptest.NewTLSServer(g)
	defer server.Close()
	root := filepath.Dir(c.CommandDatabase)
	cert := filepath.Join(root, "fixture-ca.pem")
	if err := os.WriteFile(cert, pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: server.Certificate().Raw}), 0600); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(root, "desktop.json")
	sock := filepath.Join(root, "ipc", "desktop.sock")
	config, _ := json.Marshal(map[string]any{"https_ca_file": cert, "home": map[string]any{"endpoint": server.URL, "token_file": c.DesktopTokenFile}})
	if err := os.WriteFile(path, config, 0600); err != nil {
		t.Fatal(err)
	}
	var output bytes.Buffer
	process := exec.Command(binary, "--socket", sock, "--config", path)
	process.Stderr = &output
	if err := process.Start(); err != nil {
		t.Fatal(err)
	}
	defer func() { _ = process.Process.Signal(syscall.SIGTERM); _ = process.Wait() }()
	eventually := func(condition func() bool) {
		t.Helper()
		deadline := time.Now().Add(8 * time.Second)
		for time.Now().Before(deadline) {
			if condition() {
				return
			}
			time.Sleep(30 * time.Millisecond)
		}
		t.Fatal("desktop timeout", output.String())
	}
	rpc := func(method string, params map[string]any) map[string]any {
		t.Helper()
		peer, err := net.DialTimeout("unix", sock, time.Second)
		if err != nil {
			t.Fatal(err)
		}
		defer peer.Close()
		_ = peer.SetDeadline(time.Now().Add(3 * time.Second))
		if err = json.NewEncoder(peer).Encode(map[string]any{"protocol": 1, "method": method, "params": params}); err != nil {
			t.Fatal(err)
		}
		var result map[string]any
		d := json.NewDecoder(peer)
		d.UseNumber()
		if err = d.Decode(&result); err != nil {
			t.Fatal(err)
		}
		if result["ok"] != true {
			t.Fatal(result)
		}
		return result["result"].(map[string]any)
	}
	eventually(func() bool { _, err := os.Stat(sock); return err == nil })
	state := func() map[string]any { return rpc("desktop.snapshot", map[string]any{})["home"].(map[string]any) }
	rpc("home.refresh", map[string]any{})
	eventually(func() bool { s := state(); return s["fresh"] == true && s["busy"] == false })
	rpc("home.functions", map[string]any{"device_id": "lamp"})
	eventually(func() bool { s := state(); return s["functions"].(map[string]any)["lamp"] != nil && s["busy"] == false })
	rpc("home.command", map[string]any{"device_id": "lamp", "code": "switch", "value": true, "command_id": strings.Repeat("b", 32), "revision": state()["revision"], "confirmed": true})
	eventually(func() bool {
		f.mu.Lock()
		count := len(f.posts)
		f.mu.Unlock()
		return count == 1 && state()["busy"] == false
	})
	pending := state()["pending"].(map[string]any)["lamp/switch"].(map[string]any)
	if pending["state"] != "awaiting_confirmation" {
		t.Fatal(pending)
	}
	f.mu.Lock()
	f.reported = true
	f.mu.Unlock()
	rpc("home.refresh", map[string]any{})
	eventually(func() bool { return len(state()["pending"].(map[string]any)) == 0 })
	f.mu.Lock()
	defer f.mu.Unlock()
	if len(f.posts) != 1 {
		t.Fatal("command retried")
	}
}
