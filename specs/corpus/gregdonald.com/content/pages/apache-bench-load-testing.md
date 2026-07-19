---
title: Apache Bench load testing
date: 2021-01-17
slug: apache-bench-load-testing
description: Apache Bench (ab) is a popular command-line tool used for benchmarking the performance of HTTP web servers. Here's a basic guide on how to use it: N=100 C=15 AB=/usr/sbin/ab URL=https://yahoo.com/...
tags: [apache, test]
archives: [2021-01]
---
Apache Bench (ab) is a popular command-line tool used for benchmarking the performance of HTTP web servers. Here's a basic guide on how to use it:

**Installation**

Apache Bench is usually bundled with the Apache HTTP server, but it can also be installed separately. On most Unix-based systems, you can install it using your package manager. For example, on Debian, you can install it using:

```bash
apt install apache2-utils
```

**Basic Usage**

The basic syntax of Apache Bench is:

```bash
ab [options] [http[s]://]hostname[:port]/path
```

**Key Options**

- `-n requests`: Number of requests to perform. For benchmarking, you might start with at least 1000 requests.
- `-c concurrency`: Number of multiple requests to make at a time. This can be used to simulate multiple users accessing the server concurrently.
- `-t timelimit`: Maximum number of seconds to spend for benchmarking. This overrides the `-n` parameter.
- `-p postfile`: File containing data to POST.
- `-T content-type`: Content-type header to use for POST data.

**Example Command**

Here's an example command that sends 1000 requests to a server, with a concurrency level of 10:

```bash
ab -n 1000 -c 10 http://example.com/
```

**Interpreting Results**

After running the command, Apache Bench will provide a summary of the results, including:

- Time taken for tests
- Complete requests
- Failed requests
- Requests per second (a higher number indicates better performance)
- Time per request (both across all concurrent requests and per request)
- Transfer rate

**Tips for Effective Benchmarking**

1. **Test in a Controlled Environment**: Ensure that the testing environment is consistent for each test.
2. **Monitor Server Resources**: Keep an eye on server resources like CPU, memory, and disk I/O during the test.
3. **Vary the Concurrency and Request Numbers**: This helps you understand how your server performs under different loads.
4. **Use Realistic Scenarios**: If possible, use URLs and POST data that mimic actual usage patterns.

**Important Considerations**

- **Server Impact**: Running these tests can put significant load on the server, so it's best to do this on a test server or during low-traffic periods.
- **Legal and Ethical Considerations**: Only benchmark servers that you own or have explicit permission to test.

Apache Bench is a powerful tool for a quick assessment of HTTP server performance. For more comprehensive testing, consider other tools like JMeter or Gatling, especially for applications with more complex interactions.

**Example Shell Script**

Here's a small shell script to easily automate Apache Bench usage:

```bash
N=100
C=15
AB=/usr/sbin/ab
URL=https://example.com/

$AB -n $N -c $C $URL
```
