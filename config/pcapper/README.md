# PCapper

PCapper is part of the Fortian Utilization Monitor.  It's an agent that
captures network traffic metadata (TCP and UDP packet headers) and sends it
to connected `recollect` instances.

PCapper doesn't have a configuration file.  All configuration is done via
command-line options.

# Quick Start

Run `pcapper` on each host of interest, and connect to it with a properly
configured `recollect` instance.  Errors are printed to standard
error, and `pcapper` will exit with a non-zero error code.  It normally runs
forever (or at least, until killed).

PCapper has a binary protocol on the wire which is fairly compact.  A
textual version of the same information is stored in the logfile.
Each line in the logfile or in a TCP session has the following format, and is
terminated with `\n` (ASCII `0x0A`, aka line feed, `LF`, or newline).

`time:src-addr:src-port:dest-addr:dest-port:length:flowid:proto`

- `time` is the UNIX timestamp since the epoch, with microsecond precision,
  from the header of the packet.

- `src-addr` and `dest-addr` are the dotted-quad IPv4 addresses of the
  packet's source and destination.

- `src-port` and `dest-port` are the unsigned 16-bit port numbers of the
  source and destination.

- `length` is the packet length, from the header.  Consequently, fragmented
  packets are accounted for at the start of transmission.

- `flowid` is the probable MGEN flow-ID.  This is zero if PCapper doesn't
  think the packet is an MGEN packet.

- `proto` is the IP protocol number (usually 17, for UDP; TCP is 6).

All numbers are base-10.

# Mode of Operation

PCapper is an agent that runs in each container, virtual machine, or host of
interest.  It uses `libpcap` to capture traffic and distills it down to a
textual summary, which it provides to each connected instance of
`recollect`.  It also logs all traffic sent, for both archival and debugging
purposes.  However, the log is not written to unless there is at least
one active connection to the agent.

# Syntax

Usage: `pcapper [-f logfile] [-p port] [-i interface] [-m <none | src | dst | spoof | noloop-<1 | 2 | 3> | "BPF filter">]`

- `-f` allows you to specify a logfile other than the default, which is
  `pcapper.conf` in the agent's current working directory.  Logfiles are
  only created or updated when one or more collectors (`recollect` instances)
  attach to an agent instance.

- `-p` specifies the port number to listen to.  The default is 7073, but you
  can use a different port number to facilitate multiple agents on a single
  host.

- `-i` specifies the interface to monitor.  It must be up and have an IPv4
  address at the time `pcapper` is launched.  The default is `eth0`, though
  `eth1` is another common interface to use.  If you want to monitor multiple
  interfaces, you must run multiple instances of the agent.

- `-m` is the method to use for identifying traffic.  In the explanations
  below, *MAC* is the MAC address of the monitored interface, and *IP* is
  its IPv4 address.

  - `none` performs no filtering whatsoever

  - `src` is the default, and only collects traffic sent from the current host
    (the BPF syntax is `ether src` *MAC*)

    This is also the recommended setting, since sourced traffic should be unique
    on a given (sub)network, whereas received traffic (see below) may be
    multiplied by the network in the presence of broadcasts, multicast, or even
    a shared collision domain.

  - `dst` only collects traffic sent to the current host, ignoring its own
    messages (the BPF is `ether src not` *MAC*)

  - `spoof` collects traffic sent from the current host but not by it, i.e.,
    traffic that is spoofed to be from this host, but came from elsewhere (the
    BPF is `src` *IP* `and ether src not` *MAC*)

    This is largely only relevant to BMF, which relayed traffic using this
    signature.  It might help identify traffic from other multicast forwarders.

  - `"BPF filter"` is any valid BPF filter, with the following extensions:

    - `%m` will always be replaced with the chosen interface's MAC address (see
      `-r` below)

    - `%i` will always be replaced with the chosen interface's IPv4 address

    - any other character following a `%` will be passed through unchanged,
      including another `%`, though the leading `%` will always be consumed

    Remember to escape the filter from the shell if needed.  Use this if you
    want to specify an arbitrary BPF filter.  Useful examples include:

    - `ether src %m and not dst %i`

    - `ether src %m and not ether dst %m`

    - `ether src %m and not (dst %i or ether dst %m)`

- `-r` allows you to specify the interface whose MAC address you want to use
  when determining the source of a packet.  The default is to use the MAC
  address of the monitored interface.

## Obsolete Syntax:

`-s` used to specify the server interface to listen on.  It needed to be up
and shouldn't have been the monitored interface.  This was usually `ctrl0`
(the default) or `eth0`.

However, `pcapper` now listens on all interfaces by default, to support
operating in networks where no control plane exists.

# CORE Integration

A CORE service definition file has been provided by Aypeks Consulting to
launch a single instance of `pcapper` on hosts with a single interface of
interest (and a second on hosts with two interfaces of interest).  It has
been adapted by Fortian to demonstrate running additional instances to
observe received traffic instead of sourced traffic.

An additional service definition, `pcapper-src-many.pl`, has been added to
demonstrate launching on up to 4 interfaces at once, based upon a hostname
key.
