#!/usr/bin/env bash
set -euo pipefail
CLIENT="$1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
SERVER_PID=""
cleanup(){
    if [[ -n "$SERVER_PID" ]];then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP"
}
trap cleanup EXIT
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$TMP/ca.key" >/dev/null 2>&1
openssl req -x509 -new -key "$TMP/ca.key" -sha256 -days 3650 -subj "/CN=Phoxtra HTTPS Test Root" \
 -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" -out "$TMP/ca.pem"
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$TMP/server.key" >/dev/null 2>&1
openssl req -new -key "$TMP/server.key" -subj "/CN=localhost" -out "$TMP/server.csr"
cat > "$TMP/server.ext" <<'EOF'
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:localhost
EOF
openssl x509 -req -in "$TMP/server.csr" -CA "$TMP/ca.pem" -CAkey "$TMP/ca.key" -CAcreateserial \
 -out "$TMP/server.pem" -days 2 -sha256 -extfile "$TMP/server.ext" >/dev/null 2>&1
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$TMP/unrelated.key" >/dev/null 2>&1
openssl req -x509 -new -key "$TMP/unrelated.key" -sha256 -days 3650 -subj "/CN=Unrelated Phoxtra Root" \
 -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" -out "$TMP/unrelated.pem"
run_case(){
 local tls_version="$1" hostname="$2" trust="$3" offset="$4" expectation="$5"
 : > "$TMP/server.log"
 python3 -u "$ROOT/tests/https_test_server.py" --cert "$TMP/server.pem" --key "$TMP/server.key" \
   --max-version "$tls_version" > "$TMP/server.log" 2>&1 &
 SERVER_PID=$!
 for _ in $(seq 1 100);do
  grep -q '^LISTENING ' "$TMP/server.log" && break
  if ! kill -0 "$SERVER_PID" 2>/dev/null;then cat "$TMP/server.log";return 1;fi
  sleep 0.05
 done
 local port
 port="$(awk '/^LISTENING /{print $2;exit}' "$TMP/server.log")"
 if [[ -z "$port" ]];then cat "$TMP/server.log";echo "TLS test server did not start";return 1;fi
 "$CLIENT" "$port" "$hostname" "$trust" "$offset" "$expectation"
 wait "$SERVER_PID" 2>/dev/null || true
 SERVER_PID=""
}
run_case TLSv1_2 localhost "$TMP/ca.pem" 0 success
run_case TLSv1_3 localhost "$TMP/ca.pem" 0 success
run_case TLSv1_2 wrong.example.test "$TMP/ca.pem" 0 reject
run_case TLSv1_2 localhost "$TMP/unrelated.pem" 0 reject
run_case TLSv1_2 localhost "$TMP/ca.pem" 2592000 reject
"$CLIENT" 1 localhost - 0 reject
echo "HTTPS validation passed: TLS 1.2/1.3, wrong host, untrusted CA, expired certificate, missing trust."
