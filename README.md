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
- [pcapper](blob/master/config/pcapper/README.md)
- [recollect](blob/master/config/recollect/README.md)
- [svcreqollect](blob/master/config/svcreqollect/README.md)
- [mwmg](blob/master/config/mwmg/README.md)


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
