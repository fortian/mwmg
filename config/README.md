# Overview

This is a collection of configurations and documentation for each utility.  See
the individual subdirectories for more information.

- [pcapper](pcapper/README.md)
- [recollect](recollect/README.md)
- [svcreqollect](svcreqollect/README.md)
- [mwmg](mwmg/README.md)

`recollect` connects to `pcapper` instances, consumes their summaries, and
provides the aggregated summaries to `mwmg`.  `recollect` doesn't inform
`pcapper` of the traffic of interest, to facilitate different pairs of
`recollect`/`mwmg` looking at different information.

`svcreqollect` connects directly to a REST API for a service request daemon,
consumes the JSON output from it, and provides a summary to `mwmg`.

`pcapper` will run on all hosts whose traffic you wish to monitor.  `recollect`
can run on any host that can access the control interface of the hosts running
`pcapper` and that is able to provide input to `mwmg` (via UNIX pipes, netcat,
SSH, etc.).

`svcreqollect` will run on any host that can talk to the service request daemon
and that can input to `mwmg` as above.

`mwmg` will run on any host with an X display, though connecting to a local X
display is preferred, in order to minimize delays and bizarre window manager
interactions.


# Example Network

## Traffic Flows of Interest

You'll need to be able to describe traffic of interest by (TCP or UDP) port
numbers, or by MGEN flow-IDs.  It's not recommended to overlap these groups in
any way.  You'll also need to know the "control" network IP of each host running
`pcapper` and the port it's listening on (or a way to communicate with a given
`pcapper` instance via IP).

> If you have multiple identifiers that overlap, a packet matching both
> identifiers will be counted in each time; additionally, an MGEN flow-ID of
> interest that happens to use a port number of interest will also be counted
> each time.

The example data below will be used to configure each subsystem.

| Name     | Identification               |
|----------|------------------------------|
| Chat     | Uses UDP port 1000           |
| Video    | Uses UDP ports 2000-2100     |
| FTP      | Uses TCP ports 20-21         |
| SA       | Uses MGEN flow IDs 1000-1999 |
| Priority | Uses MGEN flow ID 9999       |

## Sample Hosts of Interest

- Hosts, which generate traffic

- Single-homed gateways, which have two interfaces on the data plane, one for
  the "local LAN" and one for the connecting supernet, and primarily relay
  generated traffic

- Dual-homed gateways, with three interfaces on the data plane, one on the
  "local LAN" and two bridging disparate networks, primarily relaying traffic as
  appropriate

For this example, each system above will be running one instance of `pcapper`
per data plane interface, all looking at traffic being sent or forwarded by the
interface (all outbound traffic, from the point of view of that interface).

This is only an example; `recollect` doesn't care how many systems you have in
the network or which interfaces you monitor.  It also doesn't care about your
data plane topology at all, it only summarizes traffic recorded by `pcapper`.

| Type | Control Plane Identification | Data Plane Interfaces |
|--------------|---------------------------------------------|-|
| Hosts        | 192.168.1.*A*; *A* is between 1 and 4 | `eth0` |
| Single-Homed | 192.168.1.*B*, *B* is between 5 and 6 | `eth0`, `eth1` |
| Dual-Homed | 192.168.1.*C*, *C* is between 7 and 8 | `eth0`, `eth1`, `eth2` |

## Service Requests of Interest

For the purposes of this example, assume that service request IDs 1000, 1001,
1002, and 1003 are of interest.  These are opaque numbers to `svcreqollect` and
are provided by the service request daemon's JSON output at a specific part of
an object.

# Integration

## `pcapper`

- Generally, observing sent traffic measures network utilization (consumed
  bandwidth)

- Observing received traffic indicates network behavior (traffic reception)

- In the presence of multicast, it's easier to correlate sent traffic to network
  behavior

- Be aware that each instance of `pcapper` is equivalent to running an instance
  of `tcpdump`, which can cause additional CPU load

`pcapper` doesn't need any configuration files.  It's controlled entirely by
command line arguments.  In the network above, the 1st 4 hosts would run,
possibly via a CORE service, the command

    pcapper -f /logs/pcapper.`hostname`.eth0.log -p 7073 -i eth0 -m src

(or just ```pcapper -f /logs/pcapper.`hostname`.eth0.log``` since the other
options default to those values).

The next 2 hosts would run, via the same mechanism,

    pcapper -f /logs/pcapper.`hostname`.eth0.log -p 7073 -i eth0 -m src
    pcapper -f /logs/pcapper.`hostname`.eth1.log -p 7074 -i eth1 -m src

Similarly, the next 2 would run

    pcapper -f /logs/pcapper.`hostname`.eth0.log -p 7073 -i eth0 -m src
    pcapper -f /logs/pcapper.`hostname`.eth1.log -p 7074 -i eth1 -m src
    pcapper -f /logs/pcapper.`hostname`.eth2.log -p 7075 -i eth2 -m src

### Additional Information

- `pcapper` requires that the interface it observes has an IP address at the
  time it launches

  - This IP can later be moved (to a subinterface, bridge, tunnel, etc.)

  - This is used to understand the difference between traffic:

    - Sourced from this host

    - Sent to this host

    - Relayed to this host

- Only TCP, UDP, and ICMPv4 traffic is recorded

  - Not NetBIOS, raw Ethernet, SCTP, nor any other IP-layer protocols

  - If it's not TCP, UDP, or ICMPv4, a warning is printed to standard output,
    but it's otherwise ignored

See `pcapper`'s [README](config/pcapper/README.md) for more details.

## `recollect`

The `recollect` configuration file defaults to `collector.conf` in the current
working directory, but alternatives can be specified on the command line.

### Example Configuration File

The top of the file contains a list of key/value pairs of the format
*N*`=`flow-definition`.  The bottom of the file contains control network IP
addresses of hosts running `pcapper` and the appropriate TCP port number.  Order
of hosts/ports matters for understanding `mwmg`'s output, as does the number
assigned to each type of traffic.

```
1=1000
2=2000-2100
3=20-21
4=M1000-1999
5=M9999
192.168.1.1:7073
192.168.1.2:7073
192.168.1.3:7073
192.168.1.4:7073
192.168.1.5:7073
192.168.1.6:7073
192.168.1.7:7073
192.168.1.8:7073
192.168.1.5:7074
192.168.1.6:7074
192.168.1.7:7074
192.168.1.7:7075
192.168.1.8:7074
192.168.1.8:7075
```

Note that the last 4 hosts' IPs are specified more than once, each with
different port numbers.

### Explanation

#### Key-Pair Left-Hand Side

The left side (*N*, above) is used as an internal flow ID, and should be a
positive integer.  You may not have a flow ID less than 1 nor greater than 64.

- More than one line can have the same number

- Don't skip numbers

  - I.e., if you include ID `5`, it's expected that you include IDs `1` through
    `4` as well

  - If you do skip a number, it will make it harder to configure `mwmg`, but
    `recollect` won't notice or care

- Ordering isn't significant, but it's easier to list them in ascending order,
  to avoid overlooking an ID

#### Key-Pair Right-Hand Side

- *Flow-definition* is a port number, a range of port numbers, an MGEN flow-ID,
  or a range of MGEN flow-IDs

  - Ports are identified by number (e.g., `22` would indicate that SSH traffic
    is of interest)

  - Port ranges are specified as *start*`-`*end* (e.g., `20-21` would indicate
    that FTP traffic, both control and data, is of interest)

  - MGEN flow-IDs are identified as `M`*number* (e.g., `M1000` indicates that
    MGEN flow-ID 1000 is of interest)

  - A range of MGEN flow-IDs ae identified as `M`*start*-*end* (e.g,
    `M2000-2100` indicates that MGEN flow-IDs 2000 through 2100, inclusive, are
    of interest)

  - Traffic not otherwise identified will be summarized as type "other"

  - As there's no way to identify ICMP traffic with this syntax, ICMP will
    always be considered to be "other"

  - No distinction is made between types of traffic that match an identifier

See `recollect`'s [README](config/recollect/README.md) for more information.

## `svcreqollect`

The `svcreqollect` main configuration file also defaults to `collector.conf` in
the current working directory, but alternatives can be specified on the command
line.

The URI to the REST interface for the service request daemon can be specified
with `-u`.  If unspecified, it defaults to
`http://127.0.0.1:8618/stats/net/servicerequests`, which can be a tunneled port.

Finally, it also needs a JSON file generated by specific network
modeling software.  The default location is `node_info.json` in the current
working directory.

The interval between samples can be specified with `-t` (the default is 15
seconds).

`svcreqollect` uses `libcurl`, so it uses the same environment variables for
setting proxies.  See `libcurl-env(3)` for details, but `http_proxy` is
probably the most useful one.

### Example Configuration Files

#### `collector.conf`

This file can be the same as the one used for `recollect`.  Only the top section
is required.

#### Sample URI

The REST path is `/stats/net/servicerequests` but an example of the output
generated by the service request daemon is unavailable.

#### Sample JSON

Sample JSON identifying systems is also unavailable, but the object format
includes:

```json
{
  "HOSTNAME": {
    "address": "IPADDRESS"
  }
}
```

where `HOSTNAME` is a valid hostname that must contain the literal string
`Host`, and is repeated for each host providing a service of interest, and
`IPADDRESS`  is an IPv4 address.

## `mwmg`

```
mwmg [-c CONFIG] [-d RUNDIR] [-g] [-i INTERVAL] [-n NSYS] [-t TITLE] [-u UNITS]
    [-w WINSPEC]

    CONFIG is the fully qualified filename of the MWMG configuration file
        (default: mwmg.conf in the current working directory)
    RUNDIR is an existing directory to change into before continuing processing
    -g enables "gauge" mode: data points are absolutes instead of deltas
    INTERVAL is the number of seconds between sweeps
    NSYS is the number of systems to be graphed, and must be positive
        (default: 20)
    TITLE is a name to include in the name of all windows, such as "Network"
    UNITS is one of: 'bits' 'reqs' 'requests' (default: bits)
```

`-n` is probably the most useful argument, as it limits the X axis of the
"vu-meter" graphs.

`-g` is useful for use with `svcreqollect` or any other collector which
represents instantaneous measurements instead of counters.

### Example Configuration File

```
[Chat]
Color=#ff0000
Relay=#a00000

[Video]
Color=#0080ff
Relay=#0050a0

[FTP]
Color=#ff8000
Relay=#a05000

[SA]
Color=#00c000
Relay=#008000

[Priority]
Color=#c000ff
Relay=#7800a0
```

### Explanation

Ordering matters; the flows of interest specified to `recollect` or
`svcreqollect` as ID 1 are correlated to the "Color" line of the first section,
and so on for additional flows.

All traffic which doesn't fit into one of the flows identified by the
appropriate collector will be drawn in black (and black shouldn't be used for
anything else).

## Final Integration

A sample launch script for displaying traffic on the network described above
follows.

### Launch Script

```sh
#!/bin/sh

cd /home/project/mwmg
recollect | mwmg -n 10
svcreqollect | mwmg -n 4 -g
```

### Explanation

The configuration files (`collector.conf`, `node_info.json`, and `mwmg.conf`)
are stored in `/home/proect/mwmg`.  There are 10 interfaces of interest (two
each on the single-homed gateways and three each on the dual-homed gateways),
and 4 hosts generating services of interest.  `svcreqollect` provides
instantaneous measurements, so it needs `-g`.
