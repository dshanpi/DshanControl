package main

import (
	"bytes"
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"crypto/tls"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"
)

var endpoints = map[string]string{"cn": "https://openapi.tuyacn.com", "us": "https://openapi.tuyaus.com", "eu": "https://openapi.tuyaeu.com", "in": "https://openapi.tuyain.com", "us-east": "https://openapi-ueaz.tuyaus.com", "eu-west": "https://openapi-weaz.tuyaeu.com"}

func signature(client, secret, token, stamp, method, path string, body []byte) string {
	digest := sha256.Sum256(body)
	canonical := method + "\n" + hex.EncodeToString(digest[:]) + "\n\n" + path
	mac := hmac.New(sha256.New, []byte(secret))
	mac.Write([]byte(client + token + stamp + canonical))
	return strings.ToUpper(hex.EncodeToString(mac.Sum(nil)))
}

type provider interface {
	call(context.Context, string, string, any) (any, error)
}
type tuya struct {
	mu                                sync.Mutex
	endpoint, clientID, secret, token string
	expires                           time.Time
	http                              *http.Client
}

func newTuya(config config) (*tuya, error) {
	endpoint, ok := endpoints[config.Region]
	if !ok || !identifier.MatchString(config.AccessID) {
		return nil, errors.New("invalid Tuya region or access ID")
	}
	secret, err := readPrivate(config.AccessSecretFile)
	if err != nil || len(secret) == 0 {
		return nil, errors.New("private project secret unavailable")
	}
	transport := &http.Transport{Proxy: nil, TLSClientConfig: &tls.Config{MinVersion: tls.VersionTLS12}, ResponseHeaderTimeout: 8 * time.Second, MaxResponseHeaderBytes: 16384, MaxIdleConns: 8, IdleConnTimeout: 30 * time.Second}
	return &tuya{endpoint: endpoint, clientID: config.AccessID, secret: string(secret), http: &http.Client{Transport: transport, Timeout: 8 * time.Second, CheckRedirect: func(*http.Request, []*http.Request) error { return errors.New("redirect refused") }}}, nil
}
func (t *tuya) request(ctx context.Context, method, path string, body []byte, token string) (any, error) {
	stamp := strconv.FormatInt(time.Now().UnixMilli(), 10)
	r, err := http.NewRequestWithContext(ctx, method, t.endpoint+path, bytes.NewReader(body))
	if err != nil {
		return nil, err
	}
	r.Header.Set("client_id", t.clientID)
	r.Header.Set("sign_method", "HMAC-SHA256")
	r.Header.Set("t", stamp)
	r.Header.Set("sign", signature(t.clientID, t.secret, token, stamp, method, path, body))
	r.Header.Set("Content-Type", "application/json")
	if token != "" {
		r.Header.Set("access_token", token)
	}
	response, err := t.http.Do(r)
	if err != nil {
		return nil, err
	}
	defer response.Body.Close()
	data, err := io.ReadAll(io.LimitReader(response.Body, 524289))
	if err != nil || len(data) > 524288 || response.StatusCode != 200 {
		return nil, errors.New("upstream response rejected")
	}
	var value map[string]any
	if err = strictJSON(data, &value); err != nil {
		return nil, err
	}
	if value["success"] != true {
		return nil, errors.New("provider rejected request")
	}
	result, ok := value["result"]
	if !ok {
		return nil, errors.New("missing result")
	}
	return result, nil
}
func (t *tuya) call(ctx context.Context, method, path string, body any) (any, error) {
	t.mu.Lock()
	if !time.Now().Before(t.expires) {
		result, err := t.request(ctx, "GET", "/v1.0/token?grant_type=1", nil, "")
		if err != nil {
			t.mu.Unlock()
			return nil, err
		}
		value, ok := result.(map[string]any)
		ttl, valid := integer(value["expire_time"])
		token, isString := value["access_token"].(string)
		if !ok || !valid || ttl < 60 || ttl > 86400 || !isString || len(token) < 1 || len(token) > 4096 {
			t.mu.Unlock()
			return nil, errors.New("invalid token response")
		}
		t.token = token
		t.expires = time.Now().Add(time.Duration(ttl-30) * time.Second)
	}
	token := t.token
	t.mu.Unlock()
	var raw []byte
	var err error
	if body != nil {
		raw, err = json.Marshal(body)
		if err != nil {
			return nil, err
		}
	}
	// Never retry commands, including token rejection and ambiguous transport errors.
	return t.request(ctx, method, path, raw, token)
}
