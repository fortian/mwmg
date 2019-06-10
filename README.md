# About

This is the Fortian Utilization Monitor, also known as MWMG or the
Multi-Window Multi-Grapher.

It can show an arbitrary number of windows (up to 64), each with 2 graphs in
them, an instantaneous graph with no history on top, and a time graph of
historical aggregated information on the bottom.

All graphs can be resized arbitrarily.  A dynamic scale appears on the left,
and the units on the right.

# Quick Start

- Build the software with GNU Make (i.e, run `make` on a Linux host).
- On each container, VM, machine, or other sort of host of interest, run `pcapper`.
- On the monitoring host(s), run `recollect | mwmg` or `svcreqollect | mwmg`, with suitable configuration files in the current working directory.
- Click a name in the MWMG control window that appears to show or hide a
  traffic type of interest.

## PCapper Syntax

Usage: `pcapper [-f logfile] [-s interface] [-p port] [-i interface] [-m
<src | dst | spoof>]`

Use of `-f` is recommended, as it tells you what traffic `pcapper` is
collecting.  If you do not use `-f`, the logfile will be written to
`pcapper.log` in the current working directory.  Logfiles are only created
or updated when one or more collectors (`recollect` instances) attach to an
agent instance.

`-s` is the server interface to listen on.  It must be up and shouldn't be
the monitored interface.

`-p` specifies the port number to listen to on the server interface.  The
default is 7073, but you can use a different port number to facilitate
multiple agents on a single host.

`-i` specifies the interface to monitor.  It must be up and have an IP
address at the time `pcapper` is launched.

`-m` is the method to use for identifying traffic.

- `src` is the default, and only collects traffic sent from the current host
  (the BPF syntax is `ether src` (*MAC address* of the host).  This is also
  the recommended setting, since sourced traffic should be unique on a given
  network, whereas received traffic (see below) may be multiplied by the
  network in the presence of broadcasts, multicast, or even a shared
  collision domain.

- `dst` only collects traffic sent to the current host, ignoring its own
  messages (the BPF is `ether src not` (*MAC address* of the host).

- `spoof` collects traffic sent from the current host but not by it (i.e.,
  traffic that is spoofed to be from this host, but came from elsewhere).
  The BPF is `src` (*IP address* of the host) `and ether src not` (*MAC
  address* of the host).  This is largely relevant to BMF, which relayed
  traffic using this signature, and is unlikely to be helpful otherwise.

As you may have noticed, multiple collectors can connect to the same agent,
allowing collection of information on multiple hosts at once.

## Recollect Syntax and Configuration

Usage: `recollect [-c config]`

`recollect` operates as the first half of a UNIX pipeline, and connects to
agents specified in its configuration file (by default, `collector.conf` in
the current working directory).

### Recollect Configuration Syntax

A sample `collector.conf` is included,
which tries to connect to the same agent multiple times.  A more realistic
configuration would connect to multiple hosts.

The configuration format is line-delimited, and lists both the traffic flows
of interest and the hosts of interest.

#### Traffic Flows of Interest

Traffic flows start the file and are identified by positive numbers starting
at 1.  Flows can be specified as port numbers or MGEN flow-IDs, and can be
singletons or ranges.  For example:

    1=2000
    2=M1000
    3=2020-2029
    4=M1050-2000

This specifies interest in traffic on TCP or UDP port 2000, or with MGEN
flow-ID 1000, or on ports 2020-2029 (inclusive), or with flow-IDs between
1050 and 2000 (inclusive).  The same identifier can be used more than once:

    1=2000
    1=3000-3011
    1=5000

would classify traffic on ports 2000, 3000-3011, and 5000 as all belonging
to identifier `1`.

Overlapping ports or MGEN flows are not recommended; MGEN flows that also
use a port of interest are similarly discouraged, as either case may lead to
undefined behavior.  Any traffic that does not match a specified port, port
range, flow-ID, or flow-ID range, will be classified as "unknown."

#### Hosts of Interest

Hosts of interest immediately following the last flow specification.  These
must be numbered sequentially, starting with `1`.  Each line is
colon-delimited (`:`) and has the sequence number, the dotted-quad IP
address, and the port number, in that order.  For example:

    1:192.168.100.100:7073
    2:192.168.100.101:7073
    3:192.168.100.101:7074

would direct `recollect` to connect to agents running on two hosts,
192.168.100.100 and 192.168.100.101, and connect to the latter host twice,
once on TCP port 7073 and once on TCP port 7074.

#### A Complete Configuration

The snippets above would appear on disk as:

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


## SvcReqollect Syntax and Configuration

TBD

## MWMG Syntax and Configuration

Usage: `mwmg [-c CONFIG] [-d OUTDIR] [-g] [-i INTERVAL] [-n NSYS] [-t TITLE] [-u UNITS] [-w WINSPEC]`

- `CONFIG` refers to an MWMG configuration file (by default, `mwmg.conf` in
  the current working directory).

- `OUTDIR` is an existing directory in which to store output files (but is
  currently ignored; this is a legacy option).

- `-g` enables "gauge" mode; all data is considered an absolute value rather
  than a delta while collating historical data.

- `INTERVAL` is the number of seconds between updates of each graph.  It
  defaults to 1.

- `NSYS` is the number of hosts to expect `recollect` to provide information
  for.  This treats each agent connection as a distinct host, so it must be
  at least as large as the last host number at the end of `collector.conf`.
  You *will* get crashes if this number is too low.

- `TITLE` is a name to add to all window titles before the word
  "Utilization", such as "Network", "Bandwidth", "Request", or etc.

- `UNITS` is one of "bits", "reqs", or "requests" (a synonym for "reqs").
  It defaults to "bits".

Generally, the only option you need is `-n`.

### MWMG Configuration Syntax

`mwmg` uses a format similar to `.ini` files, for historical reasons.  Order
in the file matters, as it is correlated with the flow sequence ID provided
by `recollect`.  A sample `mwmg.conf` is provided, and excerpts are
discussed below.

    [Video]
    Color=#0080ff
    Relay=#0050a0
    
    [File Exchange]
    Color=#ff8000
    Relay=#a05000
    
    [Channel Guide]
    Color=#8000ff
    Relay=#5000a0
    
    [Voice]
    Color=#00eeee
    Relay=#009090
    
    Memory=#0000ff
    
    Processor=#006040
    
    Delay=#000000

The first entry above corresponds to the flows identified with sequence ID
"1" by `recollect`, will be identified by the name "Video" in the MWMG
control window, and its traffic will be colored a medium blue (slightly
darker for traffic identified as having been relayed through the monitored
host).

You must have at least as many traffic types as sequence identifiers used by
`recollect`, or you *will* get crashes.

Each traffic type must have a `Color=` and a `Relay=` line.  Each color is
any valid X color name, though `#RRGGBB` format is recommended.

Finally, the lines starting with `Memory=`, `Processor=` and `Delay=` are
ignored unless `SHOW_STATISTICS` was enabled for both `pcapper` and `mwmg`
when they were compiled (which is not currently recommended).

# CORE Integration

TBD

# Known Missing Features

- `svcreqollect` needs to be documented and the compilation process needs
  explanation.

- `replay-pcapper.pl` needs to be documented.

- CORE integration, based upon work by Neil Fenwick, needs to be included
  and documented.

- `pcapper` doesn't use multicast to talk to multiple collectors at once.

- `pcapper` only understands IP packets, and specifically IPv4.

- `recollect` doesn't attempt to reconnect to hosts if communications are
  interrupted.

- `mwmg` has a double-free on exit, which should be removed.  It is
  otherwise harmless.

- `mwmg` has trouble making Unity honor its window positions, especially
  when running via X forwarding, and especially when being forwarded onto an
  X server with multiple displays.

- Occasionally, `mwmg` will crash at launch due to a problem allocating
  colors.  This is harmless, though embarrassing; re-launch the `mwmg`
  pipeline (i.e., `recollect | mwmg` or the like).

- `mwmg` and `recollect` should use JSON for their configuration files
  instead of the ancient DSL which predates JSON.

- `mwmg` only displays host IDs by the number provided by `recollect` or
  `svcreqollect`; it should be able to map numbers to names.

- `mwmg`'s data collection duration should be specifiable on the command
  line, and unlimited collection should be possible (within the extent of
  available memory on the host).

# Displays

MWMG specifically shows both instantaneous ("vu-meter") bar graphs and
historical ("strip-chart") bar graphs (though these bars are often a single
pixel wide).

## Instantaneous Graphs

Instantaneous graphs update every *n* seconds (usually 1) and show traffic
over the previous second, broken out by source (or destination).

## Historical Graphs

Historical graphs also update every *n* seconds, but show total traffic
recorded since data started flowing.  Each interval is collected into a
bucket, and these buckets are merged as needed to fit the graph onto the
bottom of the window.  This means that, depending upon the width of the
window, 1 pixel may represent multiple intervals of data, or each interval
may take up more than one pixel of width.

## Other Graphs

In addition to the graphs defined in the configuration files, an Aggregate
graph and an Unknown graph are always generated.  Aggregate traffic is shown
with all traffic per interval in a single bar graph on top and all
historical traffic on bottom.  Traffic is colored to match the colors
defined for each traffic type, and is scaled proportionately, except that if
a given traffic type appeared during the interval or run, it will always be
at least one pixel tall.

Unknown traffic is always shown in black, for lack of any better
alternative.
