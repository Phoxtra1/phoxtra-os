# Verified HTTPS transport — Step 2

Phoxtra pins Mbed TLS 4.1.1 and certifi's Mozilla-derived PEM bundle at fixed repository commits. The TLS engine is upstream code; Phoxtra does not implement encryption or certificate parsing itself.

The transport API permits TLS 1.2 and TLS 1.3, requires a CA store and mandatory peer verification, sets the requested hostname before handshake, rejects invalid or missing trust data, and returns success only after receiving the TLS peer close-notify. It returns bounded raw HTTP response bytes; it is not a browser renderer.

The OS entropy service uses RDRAND only if CPU feature detection confirms support and bounded retries succeed; otherwise TLS initialization fails closed. The RTC service checks consistent CMOS snapshots and validates date fields before returning Unix time. The current implementation interprets RTC values as UTC, so an incorrectly configured RTC can make valid certificates fail validation.

The host integration harness uses a local TLS server and tests TLS 1.2/1.3, valid trusted certificate and hostname, hostname mismatch, unrelated CA, expired leaf certificate, and missing trust bundle.

The existing TCP stack remains experimental and physical NIC interoperability is unvalidated. Browser rendering, JavaScript, redirects, cookies, and general web-app navigation belong to the subsequent browser integration stage and are not claimed here.
