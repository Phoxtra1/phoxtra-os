#!/usr/bin/env python3
"""Short-lived TLS origin server for certificate validation tests."""
import argparse
import socket
import ssl
def main():
 p=argparse.ArgumentParser()
 p.add_argument("--cert",required=True);p.add_argument("--key",required=True)
 p.add_argument("--max-version",choices=("TLSv1_2","TLSv1_3"),default="TLSv1_3")
 args=p.parse_args()
 ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.minimum_version=ssl.TLSVersion.TLSv1_2
 ctx.maximum_version=getattr(ssl.TLSVersion,args.max_version);ctx.load_cert_chain(args.cert,args.key)
 server=socket.socket(socket.AF_INET,socket.SOCK_STREAM);server.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
 server.bind(("127.0.0.1",0));server.listen(1);server.settimeout(15)
 print("LISTENING {}".format(server.getsockname()[1]),flush=True)
 try:
  conn,_=server.accept();conn.settimeout(10)
  try:
   tls=ctx.wrap_socket(conn,server_side=True);request=bytearray()
   while b"\r\n\r\n" not in request and len(request)<4096:
    part=tls.recv(512)
    if not part:raise RuntimeError("HTTP request ended early")
    request.extend(part)
   if b"GET / HTTP/1.1\r\n" not in request:raise RuntimeError("bad HTTP request")
   body=b"PHOXTRA_TLS_OK"
   tls.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 14\r\nConnection: close\r\n\r\n"+body)
   try:raw=tls.unwrap();raw.close()
   except (ssl.SSLError,OSError,TimeoutError):tls.close()
  except (ssl.SSLError,OSError,RuntimeError,TimeoutError):conn.close()
 finally:server.close()
if __name__=="__main__":main()
