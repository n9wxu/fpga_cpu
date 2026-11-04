#!/usr/bin/env python3
"""apicula's gowin_pack, with ELVDS_OBUF allowed on true-LVDS pins (the Tang
Nano 20K's HDMI, pins 33-40) as Gowin's tools allow it, and the bank of an
emulated-LVDS output at the 3.3 V the board gives it (Gowin's BANK_VCCIO=3.3;
apicula's default for LVDS banks is 1.2). The fuses are checked against
NESTang's Gowin-built bitstream (see tang/README.md). Run with oss-cad-suite's
tabbypy3 and gowin_pack's arguments:

    tabbypy3 tools/gowin_pack_elvds.py -d GW2A-18C -o out.fs in_pnr.json
"""
import os
import sys
from apycula import gowin_pack as gp

_check = gp.Device.check_elvds_placement


def check_elvds_placement(self, bel):
    cfg = self.chipdb.get_io_diff_cfg(bel.x, bel.y, bel.idx_str)
    if cfg and cfg.true_lvds:
        # the true-LVDS pair takes an emulated output as well: only the P/N check
        if cfg.positive != (bel.cell.parms.get('DIFF') == 'P'):
            raise Exception(f"X{bel.x}Y{bel.y}/IOB{bel.idx_str} ({bel.cell.name}): pin P must be IOBA, pin N IOBB")
        return
    _check(self, bel)


_banks = gp.Device.check_io_banks


def check_io_banks(self):
    _banks(self)
    # Gowin sets the bank's IO_TYPE as for LVDS25 (fuse VREF1_DRIVER) in a
    # bank with emulated-LVDS outputs too, without the true-LVDS driver
    for b in self.io_banks.values():
        if b.is_used and b.has_lvds_outputs and not b.has_true_lvds_outputs:
            b.attrs['IO_TYPE'] = 'LVDS25'


gp.Device.check_elvds_placement = check_elvds_placement
gp.Device.check_io_banks = check_io_banks
gp.Device.get_lvds_bank_vccio = lambda self: os.environ.get("ELVDS_VCCIO", "3.3")

if __name__ == "__main__":
    sys.argv[0] = "gowin_pack"
    sys.exit(gp.main())
