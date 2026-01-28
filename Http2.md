
HTTP/1.0 - Only allowed one request per TCP connection
HTTP/1.1 - Added request pipelining

```C++
struct Frame {
  uint32_t len;
  uint8_t type;
  uint8_t flags;
  bool reserved;
  
};
```
