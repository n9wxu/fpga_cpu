#!/bin/bash
# Runs inside the ISE container: synthesize, implement and generate a bitstream.
# Usage: ise_build.sh <top> <part> <ucf> <verilog files...>
set -euo pipefail
TOP=$1; PART=$2; UCF=$3; shift 3

# In LXC containers /proc/cpuinfo is served by lxcfs (FUSE), and xst dies with
# SIGBUS reading it. Re-run in a private mount namespace with a plain-file copy.
if grep -q " /proc/cpuinfo fuse.lxcfs " /proc/mounts && [ -z "${ISE_CPUINFO_FIXED:-}" ]; then
    cp /proc/cpuinfo /tmp/cpuinfo.static
    exec env ISE_CPUINFO_FIXED=1 unshare -m bash -c \
        'mount --bind /tmp/cpuinfo.static /proc/cpuinfo && exec "$0" "$@"' \
        "$0" "$TOP" "$PART" "$UCF" "$@"
fi

# settings64.sh treats its first argument as the install dir, so pass it explicitly
# (otherwise it picks up this script's arguments).
ISE_DS=/opt/Xilinx/14.7/ISE_DS
set +u   # Xilinx's scripts reference unset variables
source "$ISE_DS/settings64.sh" "$ISE_DS" > /dev/null
set -u
mkdir -p build && cd build

: > "$TOP.prj"
for f in "$@"; do echo "verilog work \"../$f\"" >> "$TOP.prj"; done

mkdir -p xst/tmp
# XSTDEF (environment): a Verilog define, e.g. T1F
DEFOPT=""
if [ -n "${XSTDEF:-}" ]; then DEFOPT="-define {$XSTDEF}"; fi
cat > "$TOP.xst" <<XST
set -tmpdir xst/tmp
run -ifn $TOP.prj -ifmt mixed -ofn $TOP -ofmt NGC -p $PART -top $TOP -opt_mode Speed -opt_level 1 $DEFOPT
XST

xst -ifn "$TOP.xst" -ofn "$TOP.syr"
# -aul: the UCF covers the whole board; a design may use only some of it.
ngdbuild -aul -p "$PART" -uc "../$UCF" "$TOP.ngc" "$TOP.ngd"
# -pr b: registers next to pins go into the I/O blocks (the SRAM bus);
# -c 100: pack unrelated logic into slices too (the system design fills the part);
# -timing, -ol high: timing-driven packing and placement, full effort
map -pr b -c 100 -timing -ol high -p "$PART" -o "${TOP}_map.ncd" "$TOP.ngd" "$TOP.pcf"
par -w -ol high "${TOP}_map.ncd" "$TOP.ncd" "$TOP.pcf"
trce -v 3 -u "$TOP.ncd" -o "$TOP.twr" "$TOP.pcf"
# JtagClk startup is right for loading over JTAG; use CCLK when writing the Platform Flash.
bitgen -w -g StartUpClk:JtagClk "$TOP.ncd" "$TOP.bit" "$TOP.pcf"

# iMPACT's JTAG programming sequence as SVF, for OpenOCD to play. OpenOCD's own
# "pld load" waits only 1 ms after JPROGRAM, too short for the XC3S200 to clear
# its configuration memory, so the data is ignored and DONE stays low; iMPACT
# waits 14000 TCK. The chain from TDI: XC3S200, then the XCF02S.
cat > impact.cmd <<IMPACT
setMode -bs
setCable -port svf -file $TOP.svf
addDevice -p 1 -file $TOP.bit
addDevice -p 2 -part xcf02s
program -p 1
quit
IMPACT
impact -batch impact.cmd > impact.log 2>&1 || { cat impact.log; exit 1; }

grep -A3 "Timing summary" "$TOP.twr" || true
