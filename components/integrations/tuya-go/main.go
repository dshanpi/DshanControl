package main

import (
	"bytes"
	"context"
	"crypto/sha256"
	"crypto/subtle"
	"crypto/tls"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"mime"
	"net/http"
	"os"
	"os/signal"
	"regexp"
	"sort"
	"strings"
	"syscall"
	"time"
)

var identifier = regexp.MustCompile(`^[A-Za-z0-9_-]{1,128}$`)
var tokenPattern = regexp.MustCompile(`^[A-Za-z0-9._~-]+$`)
var keyPattern = regexp.MustCompile(`^[A-Za-z0-9-]{16,64}$`)

type config struct {
	Region           string              `json:"region"`
	AccessID         string              `json:"access_id"`
	AccessSecretFile string              `json:"access_secret_file"`
	DesktopTokenFile string              `json:"desktop_token_file"`
	CommandDatabase  string              `json:"command_database"`
	Devices          map[string][]string `json:"devices"`
	Listen           string              `json:"listen"`
	TLSCertificate   string              `json:"tls_certificate"`
	TLSKey           string              `json:"tls_key"`
}

func readPrivate(path string) ([]byte, error) {
	fd, err := syscall.Open(path, syscall.O_RDONLY|syscall.O_NOFOLLOW|syscall.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	f := os.NewFile(uintptr(fd), path)
	defer f.Close()
	st, err := f.Stat()
	if err != nil {
		return nil, err
	}
	unix, ok := st.Sys().(*syscall.Stat_t)
	if !ok || int(unix.Uid) != os.Geteuid() || !st.Mode().IsRegular() || st.Mode().Perm()&0077 != 0 || st.Size() > 32768 {
		return nil, errors.New("owner-only regular configuration required")
	}
	raw, err := io.ReadAll(io.LimitReader(f, 32769))
	if err != nil || len(raw) > 32768 {
		return nil, errors.New("private configuration too large")
	}
	return bytes.TrimSpace(raw), nil
}
func strictJSON(data []byte, dest any) error {
	// Reject duplicate members before ordinary decoding into structs/maps.
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.UseNumber()
	var visit func(int) error
	visit = func(depth int) error {
		if depth > 32 {
			return errors.New("JSON nesting limit")
		}
		token, err := decoder.Token()
		if err != nil {
			return err
		}
		delimiter, ok := token.(json.Delim)
		if !ok {
			return nil
		}
		if delimiter == '{' {
			seen := map[string]bool{}
			for decoder.More() {
				key, err := decoder.Token()
				if err != nil {
					return err
				}
				text, ok := key.(string)
				if !ok || seen[text] {
					return errors.New("duplicate JSON member")
				}
				seen[text] = true
				if err = visit(depth + 1); err != nil {
					return err
				}
			}
		} else if delimiter == '[' {
			for decoder.More() {
				if err = visit(depth + 1); err != nil {
					return err
				}
			}
		} else {
			return errors.New("unexpected JSON delimiter")
		}
		_, err = decoder.Token()
		return err
	}
	if err := visit(0); err != nil {
		return err
	}
	if _, err := decoder.Token(); err != io.EOF {
		return errors.New("trailing JSON")
	}
	decoder = json.NewDecoder(bytes.NewReader(data))
	decoder.UseNumber()
	decoder.DisallowUnknownFields()
	return decoder.Decode(dest)
}
func integer(value any) (int64, bool) {
	n, ok := value.(json.Number)
	if !ok {
		return 0, false
	}
	v, err := n.Int64()
	return v, err == nil
}

type gateway struct {
	api     provider
	store   *commandStore
	token   string
	allowed map[string][]string
	slots   chan struct{}
}

func newGateway(c config, api provider) (*gateway, error) {
	token, err := readPrivate(c.DesktopTokenFile)
	if err != nil || len(token) < 32 || len(token) > 1024 || !tokenPattern.Match(token) {
		return nil, errors.New("invalid desktop token")
	}
	if c.Devices == nil || len(c.Devices) > 64 {
		return nil, errors.New("explicit device authorization required")
	}
	for device, codes := range c.Devices {
		if !identifier.MatchString(device) || len(codes) > 24 {
			return nil, errors.New("invalid device authorization")
		}
		seen := map[string]bool{}
		for _, code := range codes {
			if !identifier.MatchString(code) || seen[code] {
				return nil, errors.New("invalid capability authorization")
			}
			seen[code] = true
		}
	}
	store, err := openStore(c.CommandDatabase)
	if err != nil {
		return nil, err
	}
	return &gateway{api: api, store: store, token: string(token), allowed: c.Devices, slots: make(chan struct{}, 16)}, nil
}
func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}
func failure(w http.ResponseWriter, status int, code string) {
	writeJSON(w, status, map[string]any{"error": code, "state_confirmed": false})
}
func (g *gateway) device(ctx context.Context, id string) (map[string]any, error) {
	result, err := g.api.call(ctx, "GET", "/v1.0/devices/"+id, nil)
	if err != nil {
		return nil, err
	}
	info, ok := result.(map[string]any)
	_, online := info["online"].(bool)
	if !ok || !online || info["id"] != id {
		return nil, errors.New("invalid upstream identity")
	}
	return info, nil
}
func contains(values []string, value string) bool {
	for _, v := range values {
		if v == value {
			return true
		}
	}
	return false
}
func (g *gateway) functions(ctx context.Context, id string) ([]map[string]any, error) {
	result, err := g.api.call(ctx, "GET", "/v1.0/devices/"+id+"/functions", nil)
	if err != nil {
		return nil, err
	}
	obj, ok := result.(map[string]any)
	if !ok {
		return nil, errors.New("invalid functions response")
	}
	values, ok := obj["functions"].([]any)
	if !ok || len(values) > 128 {
		return nil, errors.New("invalid functions")
	}
	out := []map[string]any{}
	seen := map[string]bool{}
	for _, raw := range values {
		value, ok := raw.(map[string]any)
		if !ok {
			return nil, errors.New("invalid capability")
		}
		code, _ := value["code"].(string)
		kind, _ := value["type"].(string)
		kind = map[string]string{"boolean": "Boolean", "integer": "Integer", "enum": "Enum"}[strings.ToLower(kind)]
		if !contains(g.allowed[id], code) || kind == "" {
			continue
		}
		if seen[code] {
			return nil, errors.New("duplicate capability")
		}
		seen[code] = true
		limits, ok := value["values"].(map[string]any)
		if text, isText := value["values"].(string); isText {
			if err = strictJSON([]byte(text), &limits); err != nil {
				return nil, err
			}
			ok = limits != nil
		}
		if !ok {
			return nil, errors.New("invalid capability limits")
		}
		if kind == "Integer" {
			lo, a := integer(limits["min"])
			hi, b := integer(limits["max"])
			step, c := integer(limits["step"])
			if !a || !b || !c || lo < -2147483648 || hi > 2147483647 || lo > hi || step <= 0 || step > 4294967295 {
				return nil, errors.New("invalid integer limits")
			}
		}
		if kind == "Enum" {
			values, ok := limits["range"].([]any)
			if !ok || len(values) > 64 {
				return nil, errors.New("invalid enum range")
			}
			for _, v := range values {
				if text, ok := v.(string); !ok || len(text) > 256 {
					return nil, errors.New("invalid enum value")
				}
			}
		}
		// Return only the desktop's public capability schema, never provider metadata.
		entry := map[string]any{"code": code, "type": kind, "values": limits}
		if name, ok := value["name"].(string); ok {
			entry["name"] = name
		}
		out = append(out, entry)
	}
	return out, nil
}
func (g *gateway) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	defer func() {
		if recover() != nil {
			failure(w, 502, "provider_failed")
		}
	}()
	if len(r.Header.Values("Authorization")) != 1 || subtle.ConstantTimeCompare([]byte(r.Header.Get("Authorization")), []byte("Bearer "+g.token)) != 1 {
		failure(w, 403, "unauthorized")
		return
	}
	select {
	case g.slots <- struct{}{}:
		defer func() { <-g.slots }()
	default:
		failure(w, 503, "busy")
		return
	}
	ctx, cancel := context.WithTimeout(r.Context(), 25*time.Second)
	defer cancel()
	if r.URL.RawQuery != "" || r.URL.RawPath != "" {
		failure(w, 400, "invalid_request")
		return
	}
	if r.URL.Path == "/v1/devices" && r.Method == "GET" {
		ids := make([]string, 0, len(g.allowed))
		for id := range g.allowed {
			ids = append(ids, id)
		}
		sort.Strings(ids)
		result := []map[string]any{}
		for _, id := range ids {
			info, err := g.device(ctx, id)
			if err != nil {
				failure(w, 502, "provider_failed")
				return
			}
			name, ok := info["name"].(string)
			if !ok {
				name = id
			}
			status, ok := info["status"].([]any)
			if !ok {
				failure(w, 502, "provider_failed")
				return
			}
			result = append(result, map[string]any{"id": id, "name": name, "room": "", "online": info["online"], "status": status})
		}
		writeJSON(w, 200, map[string]any{"devices": result})
		return
	}
	parts := strings.Split(r.URL.Path, "/")
	if len(parts) != 5 || parts[1] != "v1" || parts[2] != "devices" {
		failure(w, 404, "not_found")
		return
	}
	id := parts[3]
	if _, ok := g.allowed[id]; !ok {
		failure(w, 403, "unauthorized")
		return
	}
	if parts[4] == "functions" && r.Method == "GET" {
		values, err := g.functions(ctx, id)
		if err != nil {
			failure(w, 502, "provider_failed")
			return
		}
		writeJSON(w, 200, map[string]any{"control_allowed": len(g.allowed[id]) > 0, "functions": values})
		return
	}
	if parts[4] != "commands" || r.Method != "POST" {
		failure(w, 405, "method_not_allowed")
		return
	}
	media, _, err := mime.ParseMediaType(r.Header.Get("Content-Type"))
	key := r.Header.Get("Idempotency-Key")
	if err != nil || media != "application/json" || len(r.Header.Values("Idempotency-Key")) != 1 || !keyPattern.MatchString(key) {
		failure(w, 400, "invalid_request")
		return
	}
	raw, err := io.ReadAll(http.MaxBytesReader(w, r.Body, 8192))
	var body map[string]any
	if err != nil || strictJSON(raw, &body) != nil || len(body) != 2 {
		failure(w, 400, "invalid_request")
		return
	}
	code, ok := body["code"].(string)
	value, hasValue := body["value"]
	if !ok || !hasValue {
		failure(w, 400, "invalid_request")
		return
	}
	if !contains(g.allowed[id], code) {
		failure(w, 403, "unauthorized")
		return
	}
	canonical, _ := json.Marshal(body)
	digest := sha256.Sum256(append([]byte(id), canonical...))
	if err = g.store.reserve(key, hex.EncodeToString(digest[:])); err != nil {
		if errors.Is(err, errDuplicate) {
			failure(w, 409, "duplicate_or_outcome_unknown")
		} else {
			failure(w, 502, "provider_failed")
		}
		return
	}
	info, err := g.device(ctx, id)
	if err != nil {
		failure(w, 502, "provider_failed")
		return
	}
	if info["online"] != true {
		failure(w, 409, "device_offline")
		return
	}
	capabilities, err := g.functions(ctx, id)
	if err != nil {
		failure(w, 502, "provider_failed")
		return
	}
	var capability map[string]any
	for _, c := range capabilities {
		if c["code"] == code {
			capability = c
			break
		}
	}
	if capability == nil {
		failure(w, 409, "capability_unavailable")
		return
	}
	limits := capability["values"].(map[string]any)
	valid := false
	switch capability["type"] {
	case "Boolean":
		_, valid = value.(bool)
	case "Integer":
		v, ok := integer(value)
		lo, _ := integer(limits["min"])
		hi, _ := integer(limits["max"])
		step, _ := integer(limits["step"])
		valid = ok && v >= lo && v <= hi && (v-lo)%step == 0
	case "Enum":
		s, ok := value.(string)
		if ok {
			for _, v := range limits["range"].([]any) {
				if s == v {
					valid = true
				}
			}
		}
	}
	if !valid {
		failure(w, 400, "invalid_value")
		return
	}
	result, err := g.api.call(ctx, "POST", "/v1.0/devices/"+id+"/commands", map[string]any{"commands": []any{body}})
	if err != nil || result != true {
		failure(w, 502, "provider_failed")
		return
	}
	if err = g.store.accepted(key); err != nil {
		failure(w, 502, "provider_failed")
		return
	}
	writeJSON(w, 200, map[string]any{"accepted": true, "state_confirmed": false})
}
func main() {
	syscall.Umask(0077)
	path := flag.String("config", os.Getenv("DSHANCONTROL_TUYA_GATEWAY_CONFIG"), "private gateway configuration")
	flag.Parse()
	if err := serve(*path); err != nil {
		log.Print("Tuya gateway stopped: configuration, storage or transport failure")
		os.Exit(1)
	}
}
func serve(path string) error {
	raw, err := readPrivate(path)
	if err != nil {
		return err
	}
	var c config
	if err = strictJSON(raw, &c); err != nil {
		return err
	}
	if c.Listen == "" {
		c.Listen = "127.0.0.1:9443"
	}
	if c.TLSCertificate == "" || c.TLSKey == "" {
		return errors.New("TLS credentials required")
	}
	key, err := readPrivate(c.TLSKey)
	if err != nil {
		return err
	}
	cert, err := os.ReadFile(c.TLSCertificate)
	if err != nil {
		return err
	}
	pair, err := tls.X509KeyPair(cert, key)
	if err != nil {
		return err
	}
	api, err := newTuya(c)
	if err != nil {
		return err
	}
	gateway, err := newGateway(c, api)
	if err != nil {
		return err
	}
	defer gateway.store.close()
	server := &http.Server{Addr: c.Listen, Handler: gateway, ReadHeaderTimeout: 5 * time.Second, ReadTimeout: 10 * time.Second, WriteTimeout: 30 * time.Second, IdleTimeout: 30 * time.Second, MaxHeaderBytes: 16384, TLSConfig: &tls.Config{MinVersion: tls.VersionTLS12, Certificates: []tls.Certificate{pair}}}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	go func() {
		<-ctx.Done()
		deadline, cancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer cancel()
		_ = server.Shutdown(deadline)
	}()
	fmt.Println("DshanControl Go Tuya gateway starting with TLS")
	err = server.ListenAndServeTLS("", "")
	if errors.Is(err, http.ErrServerClosed) {
		return nil
	}
	return err
}
