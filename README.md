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

Each utility is documented in the README inside their respective folder
underneath `config/`; i.e.,
- [pcapper](config/pcapper/README.md)
- [recollect](config/recollect/README.md)
- [svcreqollect](config/svcreqollect/README.md)
- [mwmg](config/mwmg/README.md)

# Other Utilities

## Replay

In addition to the programs above, a debugging script
[`replay-pcapper.pl`](blob/master/replay-pcapper.pl) demonstrates how to
replay a previous run, using only `mwmg` and saved `pcapper` logfiles.
Create a storage directory.  Inside this new directory, create a
subdirectory for each system, named `TN-#`, where *#* is the two-digit host
identifier used by `recollect` (with or without leading zeroes, as you
prefer).  Inside each subdirectory, place a PCapper logfile named
`runtime.log`.

Usage: `replay-pcapper.pl` *storage-directory* `| mwmg`

Warning: this utility has not been updated in some time.  It may need to be
updated to match the current `pcapper` log format.

## Other Inputs

Anything that can provide input to `recollect` (if broadly spread across
much of the network) or to `mwmg` (if already aggregated) can be used to
generate graphs.

The information below can be used to create new agents for `recollect` to
interrogate or new aggregators for `pcapper` and/or `mwmg`.

### Agent-Collector Protocol

Recollect consumes an unversioned binary packet format.  Future versions of
`recollect` will either be able to process this format, or will terminate
the connection to the agent if they cannot.

All sizes below are in bytes.  All numbers are unsigned and in network
order.  PCapper takes all values from the original packet whenever possible.

Name                | Length | Offset | Note
--------------------|--------|--------|-----
Seconds             | 4      | 0      | Seconds since the epoch
Microseconds        | 4      | 4      | Microseconds since the second
Source Address      | 4      | 8      | IPv4 source address
Destination Address | 4      | 12     | IPv4 destination address
Source Port         | 2      | 16     |
Destination Port    | 2      | 18     |
Length              | 2      | 20     | Packet length in bytes
Flow-ID             | 4      | 22     | MGEN Flow-ID, if identified
Protocol            | 1      | 26     | UDP, TCP (others possible; currently ignored)
Flags               | 1      | 27     | Reserved, must be 0
Current on-the-wire size: | - | 28 |

Additional data will be appended and ignored by clients which don't support
it.

### Collector-Grapher Protocol

MWMG consumes an unversioned text format.  Future versions of `mwmg` will
either be able to process this format or will exit immediately upon receipt
of an invalid line.

All line end with a UNIX newline (ASCII NL [0x0A]: `\n`).  All numbers are
base ten.  `mwmg` expects network traffic in bits, as is typical for network
analysis, and doesn't multiply values by 8.  Each field is delimited with a
colon (`:`).

Name | Sample | Note
-|-|-
Timestamp | `1560390672.895279` | 
Host ID | `3` | One-based; zero is invalid
Flow ID | `1` | One-based; zero is invalid; `X` indicates unknown
Value | `732` | Number of bits, current temperature, whatever
Level | `0` | Primary or Secondary data; usually 0
