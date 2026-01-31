Mac -> Linux 01/28
8 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   167.14ms   39.17ms 889.04ms   75.24%
    Req/Sec   183.91     47.33   380.00     74.41%
  44059 requests in 30.09s, 3.91MB read
  Socket errors: connect 267, read 0, write 0, timeout 0
Requests/sec:   1464.32
Transfer/sec:    132.99KB

Caddy on Linux
  8 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   183.60ms   78.25ms 539.48ms   75.71%
    Req/Sec   352.42    149.96   626.00     53.98%
  83767 requests in 30.05s, 19.97MB read
Requests/sec:   2788.01
Transfer/sec:    680.67KB

Initial
Custom on Linux
  8 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   326.37ms   78.40ms   1.08s    64.30%
    Req/Sec   195.51     60.04   410.00     70.93%
  46845 requests in 30.10s, 4.15MB read
Requests/sec:   1556.49
Transfer/sec:    141.36KB

Updated: Updated Response Building
  8 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   320.94ms   78.95ms 557.14ms   60.93%
    Req/Sec   198.82     63.60   420.00     69.98%
  47606 requests in 30.09s, 4.22MB read
Requests/sec:   1582.33
Transfer/sec:    143.71KB

Updated: Optimized build for release
  8 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   325.58ms   74.15ms 524.02ms   67.56%
    Req/Sec   195.90     60.63   434.00     72.34%
  46915 requests in 30.09s, 4.16MB read
Requests/sec:   1558.95
Transfer/sec:    141.58KB


# Local
  8 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.10ms    1.48ms  25.61ms   84.53%
    Req/Sec    67.56k     6.87k   88.15k    76.17%
  Latency Distribution
     50%  327.00us
     75%    1.37ms
     90%    3.51ms
     99%    5.91ms
  16133083 requests in 30.04s, 1.40GB read
Requests/sec: 537045.30
Transfer/sec:     47.63MB

  8 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    11.20ms   15.96ms 183.87ms   84.17%
    Req/Sec    16.37k     1.50k   22.89k    69.33%
  Latency Distribution
     50%    2.50ms
     75%   17.58ms
     90%   34.84ms
     99%   66.28ms
  3909425 requests in 30.04s, 0.91GB read
Requests/sec: 130119.34
Transfer/sec:     31.02MB

