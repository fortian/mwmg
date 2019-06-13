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

# Other Utilities

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
