# Recollect

Recollect is part of the Fortian Utilization Monitor.  It aggregates
network utilization information provided by PCapper instances.

# Quick Start

`recollect | mwmg`

# Syntax

Usage: `recollect [-c config]`

`recollect` operates as the start of a UNIX pipeline, and connects to agents
specified in its configuration file (by default, `collector.conf` in the
current working directory).

For troubleshooting, you can run `recollect` without piping its standard
output to `mwmg`.  Errors are printed to standard error and cause
`recollect` to exit with a non-zero error code.

`recollect` generally runs until interrupted with Control-C, but will also
exit if standard output closes.

Each line of output has the following format, and is terminated with `\n`
(ASCII `0x0A`, aka line feed, `LF`, or newline).

`timwstamp:host:id:length`

- `timestamp` is the timestamp from the packet's header, as provided by
  `pcapper`

- `host` refers to the serial number of the host from the `collector.conf`
  file

- `id` refers to the serial number of the traffic type from the
  `collector.conf` file, or the character `X` if it is not a recognized type

- `length` is the length of the packet, in bytes

All numbers are base-10.

# Configuration

A sample `collector.conf` is included, which tries to connect to the same
agent multiple times.  A more realistic configuration would connect to
multiple hosts.

The configuration format is newline-delimited, and lists both the traffic
flows of interest and the hosts of interest.

## Traffic Flows of Interest

Traffic flows are identified by positive numbers starting at 1.  Flows can
be specified as port numbers or MGEN flow-IDs, and can be singletons or
ranges.  For example:

    1=2000
    2=M1000
    3=2020-2029
    4=M1050-2000

This specifies interest in traffic on TCP or UDP port 2000 (with traffic-ID
1), or with MGEN flow-ID 1000, or on ports 2020-2029 (inclusive), or with
flow-IDs between 1050 and 2000 (inclusive), each with traffic IDs 1 through
4, respectively.  The same identifier can be used more than once:

    1=2000
    1=3000-3011
    1=5000

would classify traffic on ports 2000, 3000-3011, and 5000 as all belonging
to identifier `1`.

Overlapping ports or MGEN flows are not recommended; MGEN flows that also
use a port of interest are similarly discouraged, as either case may lead to
undefined behavior.  Any traffic that does not match a specified port, port
range, flow-ID, or flow-ID range, will be classified as "unknown."

## Hosts of Interest

Hosts of interest must also be numbered sequentially, starting with `1`.
Unlike traffic flows, host identifiers shouldn't be repeated.  Each line is
colon-delimited (`:`) and has the sequence number, the dotted-quad IP
address, and that `pcapper`'s port number, in that order.  For example:

    1:192.168.100.100:7073
    2:192.168.100.101:7073
    3:192.168.100.101:7074

This would direct `recollect` to connect to agents running on two hosts,
192.168.100.100 and 192.168.100.101, as well as to connect to the latter host twice,
once on TCP port 7073 and once on TCP port 7074.

## A Complete Configuration

The snippets above could appear on disk as:

    1=2000
    2=M1000
    3=2020-2029
    4=M1050-2000
    1=2000
    1=3000-3011
    1=5000
    1:192.168.100.100:7073
    2:192.168.100.101:7073
    3:192.168.100.101:7074

# Diagnostics



Each time `recollect` receives a packet, it examines the timestamp.  If the
number of seconds since the epoch differs from that of the previous packet,
`recollect` will print the new time in HH:MM:SS format.  If this timestamp
changes more than once per second, `recollect` is processing packets in
non-real-time.

If you're getting:

- Times consistently older than the current second:

  - The host running `recollect` is likely to be overloaded

  - A network link, usually between `recollect` and the agents, is experiencing severe latency

  - Possibly all of the hosts under test are overloaded

- Timestamps flipping between two different seconds frequently:

  - A host under test is probably overloaded, or many are

  - A network link, possibly between `recollect` and the agents, is experiencing severe jitter

- Times in the future:

  - Your network is not properly time-synchronized

Note that `mwmg` will believe `recollect` and will place packets in the
time-based bucket identified.  If `mwmg` is on the same host as `recollect`,
and that host is overloaded, graph update times may be quite irregular.
