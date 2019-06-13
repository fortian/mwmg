# About

SvcReqollect is part of the Fortian Utilization Monitor.  It aggregates
JSON-formatted service request information provided via a particular REST
API, but can be adapted to support others.

# Quick Start

`svcreqollect | mwmg`

# Building

Different distributions keep their `libjson-c` include files in different
places, and do not always inform `pkgconfig` of its presence.

If your `json.h` include file is in `/usr/include/json`, define
`JSON_H_IN_JSON` before building `svcreqollect`.  Otherwise, it will look in
`/usr/include/json-c` for it.

You will need to install the "development" packages for the software listed
below in order to build `svcreqollect`, along with all of their
dependencies.

- cURL
  - CentOS 7.6: `yum install curl-devel`
  - Debian-based distributions tend to call it `libcurl-dev` or `libcurl3-dev` or something like that
  - Red Hat-based distributions tend to call it `curl-devel`
  - Slackware: `slackpkg install curl` or the like
  - Ubuntu 16.04 LTS: `apt-get install libcurl4-nss-dev libcurl4-openssl-dev libcurl3` (mixing `libcurl3` and `libcurl4` isn't a typo)
- JSON-C
  - CentOS: `json-c-devel`
  - Slackware: `json-c`
  - Ubuntu: `libjson-c-dev`

# Syntax

Usage: `svcreqollect [-c config] [-x]`

`svcreqollect` operates as the start of a UNIX pipeline, and connects to a
REST API defined at compile time.  It uses a configuration file specified by
`-c` (by default, `collector.conf` in the current working directory) to
identify types of service requests (via destination port number), and a
specific JSON-formatted configuration file (specified at compile time) to
identify hosts of interest.

`-x` is a debugging flag and is used to generate data based upon
pre-configured JSON files.

## Service Requests of Interest

Service requests are identified by positive numbers starting
with 1.  Each request can have one or more destination ports associated with
it, by repeating the identifier.

    1=2000
    2=1000
    3=2020
    3=2021

This specifies interest in traffic to TCP or UDP port 2000, to port 1000,
and to ports 2020 and 2021.

## Hosts of Interest

Each entry in the JSON file (typically `node_info.json` in a well-known directory) that contains the (case-sensitive) characters "Host" in the name will be included as a host of interest to `svcreqollect`.

# Open Issues

- Sample JSON is missing.

- Too many things are hard-coded.
