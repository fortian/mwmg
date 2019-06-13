# About

MWMG is the front end of the Fortian Utilization Monitor.  It summarizes
utilization information from a collector (such as `recollect` or
`svcreqollect`) and displays the data in two kinds of graphs.

It can show an arbitrary number of windows, each with two graphs in them: an
instantaneous "vu-meter" style graph and a historical "strip-chart" graph on
the bottom.

All graphs can be resized arbitrarily.  A dynamic scale appears on the left
and the units in use appear on the right.

# Quick Start

`recollect | mwmg`

`svcreqollect | mwmg`

# Syntax

Usage: `mwmg [-c CONFIG] [-d RUNDIR] [-g] [-i INTERVAL] [-n NSYS] [-t TITLE] [-u UNITS] [-w WINSPEC]`

- `CONFIG` refers to an MWMG configuration file (by default, `mwmg.conf` in
  the current working directory)

- `RUNDIR` is an existing directory for MWMG to change to before continuing
  processing of command line arguments or input data

- `-g` enables "gauge" mode; all data is considered an absolute value rather
  than a delta while collating historical data

- `INTERVAL` is the number of seconds between updates of each graph (by
  default, 1 second)

- `NSYS` is the number of hosts to expect `recollect` to report upon

  N.B.: This treats each agent connection as a distinct host, so it must be
  at least as large as the last host number at the end of `collector.conf`.
  You *will* get crashes if this number is too low.

- `TITLE` is a name to add to all window titles before the word
  "Utilization", such as "Network", "Bandwidth", "Request", or etc.

- `UNITS` is one of "bits" (the default), "reqs", or "requests" (a synonym
  for "reqs")

Generally, the only option you need is `-n`.

`mwmg` operates as the end of a UNIX pipeline, reading textual data as
formatted by `recollect` (or `svcreqollect`), bucketing it over each
interval, and displaying near-real-time results.  (All data is delayed by
the length of one interval.)

`mwmg` can be configured at compile-time to exit after a specific amount of
time has passed, or to keep running until closed.  It can be stopped by the
window manager, with `xkill` on the control window, by sending it a `TERM`
signal, or with Control-C.

# Configuration

For historical reasons, `mwmg` uses a format similar to `.ini` files.  Order
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

The first entry above corresponds to the flows identified with sequence ID
"1" by `recollect`.  All data pertaining to this flow will be identified by
the name "Video" in the MWMG control window, and its traffic will be colored
a medium blue (and a slightly darker blue for traffic identified as having
been relayed through the monitored host).

You must have at least as many traffic types as sequence identifiers used by
`recollect`, or you *will* get crashes.

Each type must have a `Color=` and a `Relay=` line, which are really the
colors for "primary" and "secondary" data.  Each color is any valid X color
name, though `#RRGGBB` format is recommended.  It is recommended that the
color used for secondary data be similar to the color used for primary data.
To suppress differentiation of secondary data, make it the same color.

There are a few other keywords which are obsolete and ignored by `mwmg`.

# Diagnostics

Any parse error in the configuration file will cause `mwmg` to print an
error on standard error and exit with a non-zero status.

`mwmg` will stall at launch until it standard input has data available.

# Displays

As mentioned above, MWMG shows both instantaneous ("vu-meter") bar graphs
and historical ("strip-chart") bar graphs (though these bars are often a
single pixel wide).

## Instantaneous Graphs

Instantaneous graphs update every *interval* seconds (usually 1) and show
traffic over the previous second, broken out by source (or destination).

## Historical Graphs

Historical graphs also update every *interval* seconds and show total
traffic recorded since data started flowing.  Each interval is collected
into a bucket, and these buckets are merged as needed to fit the graph onto
the bottom of the window.  This means that, depending upon the width of the
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

# Open Issues

- `mwmg` has a double-free on exit, which should be removed.  This is
  believed to be harmless.

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
