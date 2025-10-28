// Copyright (C) 2025 Bytedance. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License.

package main

import (
	"context"
	"encoding/base64"
	"errors"
	"log"
	"strings"

	"github.com/milvus-io/milvus-proto/go-api/v2/hook"
	"github.com/milvus-io/milvus-proto/go-api/v2/milvuspb"
	"google.golang.org/grpc/metadata"
	// Do not import ANY internal milvus package!!!
)

const (
	// deleteCredentialMethod is the full gRPC method name for deleting credentials.
	deleteCredentialMethod = "/milvus.proto.milvus.MilvusService/DeleteCredential"

	// updateCredentialMethod is the full gRPC method name for updating credentials.
	updateCredentialMethod = "/milvus.proto.milvus.MilvusService/UpdateCredential"

	// operateUserRoleMethod is the full gRPC method name for operating user-role mapping (grant/revoke roles).
	operateUserRoleMethod = "/milvus.proto.milvus.MilvusService/OperateUserRole"
)

// MethodHandler defines the signature of per-method hook handlers.
// Each handler decides whether to allow the request or return an error.
type MethodHandler func(ctx context.Context, req any) (context.Context, error)

// ProxyHook implements hook.Hook to provide custom logic before/after Milvus requests.
// Keep the package as main and export variables MilvusHook and MilvusExtension
// so Milvus can find them via plugin.Lookup.
type ProxyHook struct {
	// initialUsers holds the protected usernames parsed from Init params.
	initialUsers []string
	// handlers is a registry that maps fullMethod to its specific handler.
	handlers map[string]MethodHandler
}

// Ensure ProxyHook implements hook.Hook interface
var _ hook.Hook = (*ProxyHook)(nil)

// VerifyAPIKey verifies an API key and returns the associated username.
// For this plugin, API key verification is not implemented.
func (p *ProxyHook) VerifyAPIKey(key string) (string, error) {
	// Not implemented in this plugin
	return "", errors.New("proxy_hook: VerifyAPIKey not implemented")
}

// Init initializes the hook with parameters from hook.yaml (SoConfig).
// params["initialUsers"] should be a comma-separated list of usernames.
func (p *ProxyHook) Init(params map[string]string) error {
	p.initialUsers = make([]string, 0)
	// Read the initial users list from configuration.
	raw := ""
	if v, ok := params["initialusers"]; ok {
		raw = v
	}

	if raw != "" {
		for _, u := range strings.Split(raw, ",") {
			name := strings.TrimSpace(u)
			if name == "" {
				continue
			}
			p.initialUsers = append(p.initialUsers, name)
		}
	}

	log.Printf("[proxy_hook] Init: initialUsers=%v", p.initialUsers)

	// Initialize method handlers registry.
	p.handlers = map[string]MethodHandler{
		deleteCredentialMethod: p.handleDeleteCredential,
		updateCredentialMethod: p.handleUpdateCredential,
		operateUserRoleMethod:  p.handleOperateUserRole,
	}
	return nil
}

// Mock allows short-circuiting a request with a mocked response.
// This plugin does not mock any responses.
func (p *ProxyHook) Mock(ctx context.Context, req interface{}, fullMethod string) (bool, interface{}, error) {
	// No mocking, always let request proceed.
	return false, nil, nil
}

// Before runs prior to the actual handler. It can mutate the context or return an error to block.
// It now routes to a small per-method handler for better extensibility.
func (p *ProxyHook) Before(ctx context.Context, req interface{}, fullMethod string) (context.Context, error) {
	if handler, ok := p.handlers[fullMethod]; ok {
		return handler(ctx, req)
	}
	// Unhandled methods: pass through
	return ctx, nil
}

// handleDeleteCredential blocks deleting usernames configured as initial users.
func (p *ProxyHook) handleDeleteCredential(ctx context.Context, req any) (context.Context, error) {
	// Assert the request type to DeleteCredentialRequest.
	dcReq, ok := req.(*milvuspb.DeleteCredentialRequest)
	if !ok {
		return ctx, nil
	}

	requester := p.requesterFromContext(ctx)
	// Check if the username is one of the protected initial users.
	for _, u := range p.initialUsers {
		if u == dcReq.GetUsername() && requester != "root" {
			// Return error to block deletion; Milvus will convert this to an InvalidArgument gRPC status.
			return ctx, errors.New("initial user cannot be deleted")
		}
	}
	return ctx, nil
}

// handleUpdateCredential blocks modifying root when requester is not root.
func (p *ProxyHook) handleUpdateCredential(ctx context.Context, req any) (context.Context, error) {
	// Assert the request type to UpdateCredentialRequest
	ucReq, ok := req.(*milvuspb.UpdateCredentialRequest)
	if !ok {
		return ctx, nil
	}
	// Unified requester extraction for HTTP and gRPC
	requester := p.requesterFromContext(ctx)
	// If target is root and requester is not root, block update
	if ucReq.GetUsername() == "root" && requester != "root" {
		return ctx, errors.New("root user cannot be modified by non-root")
	}
	return ctx, nil
}

// handleOperateUserRole blocks changing roles for usernames configured as initial users.
func (p *ProxyHook) handleOperateUserRole(ctx context.Context, req any) (context.Context, error) {
	// Assert the request type to OperateUserRoleRequest
	orReq, ok := req.(*milvuspb.OperateUserRoleRequest)
	if !ok {
		return ctx, nil
	}
	// Unified requester extraction for HTTP and gRPC
	requester := p.requesterFromContext(ctx)
	username := orReq.GetUsername()
	for _, u := range p.initialUsers {
		if u == username && requester != "root" {
			return ctx, errors.New("initial user privileges cannot be modified")
		}
	}
	return ctx, nil
}

// After runs after the handler finishes. No-op for this plugin.
func (p *ProxyHook) After(ctx context.Context, result interface{}, err error, fullMethod string) error {
	return nil
}

// Release releases any resources. No-op for this plugin.
func (p *ProxyHook) Release() {}

// NoopExtension implements hook.Extension with minimal behavior.
// Milvus requires both Hook and Extension symbols to be present.
type NoopExtension struct{}

// Ensure NoopExtension implements hook.Extension interface
var _ hook.Extension = (*NoopExtension)(nil)

// Report receives arbitrary info; return 0 as a dummy status code.
func (e *NoopExtension) Report(info any) int { return 0 }

// ReportRefused can be used to report refused requests; No-op here.
func (e *NoopExtension) ReportRefused(ctx context.Context, req interface{}, resp interface{}, err error, fullMethod string) error {
	return nil
}

// Global variables that must be exported by the plugin
// Note: These must be exported as concrete pointer types for Go plugin system
var (
	MilvusHook      ProxyHook
	MilvusExtension NoopExtension
)

// init function is automatically executed when the plugin is loaded
func init() {
}

// main function is required for plugin build mode but will not be called
func main() {
	// This function is required for -buildmode=plugin but will not be executed
	// The actual plugin functionality is provided through exported variables
}

// decodeBase64 tries standard and raw base64 decoding.
func decodeBase64(s string) (string, error) {
	if s == "" {
		return "", errors.New("empty base64 string")
	}
	if b, err := base64.StdEncoding.DecodeString(s); err == nil {
		return string(b), nil
	}
	if b, err := base64.RawStdEncoding.DecodeString(s); err == nil {
		return string(b), nil
	}
	return "", errors.New("fail to decode base64 string")
}

// requesterFromContext returns the requester username for both HTTP(gin) and gRPC(SDK).
// Priority:
// 1) Gin params (HTTP path)
// 2) gRPC metadata: authorization(base64(username:password))
func (p *ProxyHook) requesterFromContext(ctx context.Context) string {
	// Try Gin-injected params first
	if kv, ok := ctx.Value(hook.GinParamsKey).(map[string]any); ok && kv != nil {
		if v, ok := kv["username"]; ok && v != nil {
			if s, ok2 := v.(string); ok2 && s != "" {
				return s
			}
		}
	}
	// Fallback to gRPC metadata
	if md, ok := metadata.FromIncomingContext(ctx); ok {
		if authVals := md["authorization"]; len(authVals) > 0 {
			// metadata usually carries pure base64; be robust to optional "Bearer " prefix
			token := strings.TrimPrefix(authVals[0], "Bearer ")
			if raw, err := decodeBase64(token); err == nil {
				if idx := strings.IndexByte(raw, ':'); idx > 0 {
					uname := raw[:idx]
					return uname
				}
			}
		}
	}
	return ""
}
