# 📡 HTTP Request Format & Response Codes

## 📝 HTTP Request Format

An HTTP request consists of:

1. **Request Line**
2. **Headers**
3. **Blank Line** (delimiter between headers and body)
4. **Optional Body** (for methods like POST, PUT)

---

### 🔹 Raw Format

```
<Method> <Request-URI> <HTTP-Version>\r\n
<Header-Name>: <Header-Value>\r\n
<Header-Name>: <Header-Value>\r\n
...
\r\n
<Optional-Body>
```

### 🔹 Request Line

```http
<HTTP_METHOD> <REQUEST_URI> <HTTP_VERSION>
```

**Example:**

```http
GET /index.html HTTP/1.1
```

- **HTTP_METHOD**: `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `HEAD`, `OPTIONS`, etc.
- **REQUEST_URI**: Path to the resource (can include query parameters).
- **HTTP_VERSION**: Usually `HTTP/1.1` or `HTTP/2`.

---

### 🔹 Headers

Headers provide metadata about the request. Here's a breakdown of common headers:

| Header              | Description                                 | Common Values                                           |
| ------------------- | ------------------------------------------- | ------------------------------------------------------- |
| `Host`              | Specifies the domain name of the server     | `example.com`                                           |
| `User-Agent`        | Identifies the client software              | `Mozilla/5.0`, `curl/7.68.0`                            |
| `Accept`            | Specifies media types the client can handle | `text/html`, `application/json`                         |
| `Accept-Encoding`   | Specifies encoding algorithms               | `gzip`, `deflate`, `br`                                 |
| `Accept-Language`   | Preferred languages                         | `en-US`, `pt-PT`                                        |
| `Content-Type`      | Media type of the request body              | `application/json`, `application/x-www-form-urlencoded` |
| `Content-Length`    | Size of the request body in bytes           | `348`                                                   |
| `Authorization`     | Credentials for authentication              | `Basic <base64>`, `Bearer <token>`                      |
| `Cache-Control`     | Caching directives                          | `no-cache`, `max-age=3600`                              |
| `Connection`        | Control connection behavior                 | `keep-alive`, `close`                                   |
| `Referer`           | URL of the previous page                    | `https://example.com/page`                              |
| `Cookie`            | Cookies sent by the client                  | `sessionId=abc123; theme=dark`                          |
| `If-Modified-Since` | Conditional GET                             | `Wed, 21 Oct 2015 07:28:00 GMT`                         |
| `Range`             | Request partial content                     | `bytes=0-499`                                           |

---

### 🔹 Body (Optional)

Used with methods like `POST`, `PUT`, or `PATCH`.

**Example:**

```json
{
  "username": "john_doe",
  "password": "secure123"
}
```

---

## ✅ HTTP Response Status Codes

### 🔹 1xx — Informational

| Code | Meaning             |
| ---- | ------------------- |
| 100  | Continue            |
| 101  | Switching Protocols |
| 102  | Processing          |
| 103  | Early Hints         |

### 🔹 2xx — Success

| Code | Meaning                       |
| ---- | ----------------------------- |
| 200  | OK                            |
| 201  | Created                       |
| 202  | Accepted                      |
| 203  | Non-Authoritative Information |
| 204  | No Content                    |
| 205  | Reset Content                 |
| 206  | Partial Content               |
| 207  | Multi-Status                  |
| 208  | Already Reported              |
| 226  | IM Used                       |

### 🔹 3xx — Redirection

| Code | Meaning            |
| ---- | ------------------ |
| 300  | Multiple Choices   |
| 301  | Moved Permanently  |
| 302  | Found              |
| 303  | See Other          |
| 304  | Not Modified       |
| 305  | Use Proxy          |
| 307  | Temporary Redirect |
| 308  | Permanent Redirect |

### 🔹 4xx — Client Errors

| Code | Meaning                         |
| ---- | ------------------------------- |
| 400  | Bad Request                     |
| 401  | Unauthorized                    |
| 402  | Payment Required                |
| 403  | Forbidden                       |
| 404  | Not Found                       |
| 405  | Method Not Allowed              |
| 406  | Not Acceptable                  |
| 407  | Proxy Authentication Required   |
| 408  | Request Timeout                 |
| 409  | Conflict                        |
| 410  | Gone                            |
| 411  | Length Required                 |
| 412  | Precondition Failed             |
| 413  | Payload Too Large               |
| 414  | URI Too Long                    |
| 415  | Unsupported Media Type          |
| 416  | Range Not Satisfiable           |
| 417  | Expectation Failed              |
| 418  | I'm a teapot ☕                 |
| 421  | Misdirected Request             |
| 422  | Unprocessable Entity            |
| 423  | Locked                          |
| 424  | Failed Dependency               |
| 425  | Too Early                       |
| 426  | Upgrade Required                |
| 428  | Precondition Required           |
| 429  | Too Many Requests               |
| 431  | Request Header Fields Too Large |
| 451  | Unavailable For Legal Reasons   |

### 🔹 5xx — Server Errors

| Code | Meaning                         |
| ---- | ------------------------------- |
| 500  | Internal Server Error           |
| 501  | Not Implemented                 |
| 502  | Bad Gateway                     |
| 503  | Service Unavailable             |
| 504  | Gateway Timeout                 |
| 505  | HTTP Version Not Supported      |
| 506  | Variant Also Negotiates         |
| 507  | Insufficient Storage            |
| 508  | Loop Detected                   |
| 510  | Not Extended                    |
| 511  | Network Authentication Required |
