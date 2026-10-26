# fpga

Spartan-3 Starter Board (XC3S200-4FT256) build.

- `constraints/`: UCF pin and timing constraints, starting from Digilent's reference UCF
- `scripts/`: ISE 14.7 command-line flow (xst → ngdbuild → map → par → trce → bitgen)

Build on a Linux host with ISE 14.7 installed. Program from the Mac with:

    openFPGALoader -c <cable> build/top.bit
