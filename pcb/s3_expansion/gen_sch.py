"""Generate s3_expansion.kicad_sch: a rough schematic of the Spartan-3
Starter Board expansion (sound, PS/2 mouse, microSD, two Sega Genesis pads on DE9s,
an ESP32 on A2;
a NOR flash ROM on the SRAM's bus, A1). Every pin is connected by a net label at its
end; the symbols are copied from KiCad's standard libraries.

usage: python3 gen_sch.py [KICAD_SYMBOL_DIR]
"""
import os
import re
import sys
import uuid

LIBDIR = sys.argv[1] if len(sys.argv) > 1 else \
    "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "s3_expansion.kicad_sch")
ROOT = str(uuid.uuid4())


def uid():
    return str(uuid.uuid4())


# ---------------------------------------------------------------- symbols
_libs = {}


def lib_text(lib):
    if lib not in _libs:
        _libs[lib] = open(os.path.join(LIBDIR, lib + ".kicad_sym")).read()
    return _libs[lib]


def block(lib, name):
    s = lib_text(lib)
    i = s.find('\n\t(symbol "%s"\n' % name)
    if i < 0:
        raise KeyError(lib + ":" + name)
    depth, j = 0, i + 1
    while True:                              # the balanced s-expression
        c = s[j]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return s[i + 1:j + 1]
        elif c == '"':
            j += 1
            while s[j] != '"':
                j += 2 if s[j] == '\\' else 1
        j += 1


def units(blk, name):
    """the unit sub-symbols of a symbol block"""
    out, pos = [], 0
    pat = '(symbol "%s_' % name
    while True:
        i = blk.find(pat, pos)
        if i < 0:
            return out
        depth, j = 0, i
        while True:
            c = blk[j]
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    break
            elif c == '"':
                j += 1
                while blk[j] != '"':
                    j += 1
            j += 1
        out.append(blk[i:j + 1])
        pos = j + 1


def flat(lib, name):
    """the symbol as a schematic embeds it: "Lib:Name", derived symbols resolved"""
    blk = block(lib, name)
    m = re.search(r'\(extends "([^"]+)"\)', blk)
    if m:
        parent = m.group(1)
        pblk = block(lib, parent)
        pu = units(pblk, parent)
        # the child's own properties, the parent's body (renamed)
        head = re.sub(r'\s*\(extends "[^"]+"\)', '', blk)
        head = head[:head.rstrip().rfind(')')]
        # copy the parent's settings the child lacks (pin names, numbers)
        extra = ''
        for key in ('(pin_numbers', '(pin_names', '(exclude_from_sim', '(in_bom', '(on_board'):
            if key not in head:
                k = pblk.find(key)
                if k >= 0:
                    d, e = 0, k
                    while True:
                        if pblk[e] == '(':
                            d += 1
                        elif pblk[e] == ')':
                            d -= 1
                            if d == 0:
                                break
                        e += 1
                    extra += '\n\t\t' + pblk[k:e + 1]
        body = ''.join('\n\t\t' + u.replace('(symbol "%s_' % parent, '(symbol "%s_' % name, 1) for u in pu)
        blk = head + extra + body + '\n\t)'
    blk = blk.replace('(symbol "%s"' % name, '(symbol "%s:%s"' % (lib, name), 1)
    return blk


def pins(lib, name):
    """number -> (x, y, angle, name) in symbol coordinates (unit 1 and common)"""
    blk = flat(lib, name)
    out = {}
    for m in re.finditer(r'\(pin \w+ \w+\s*\(at ([-\d.]+) ([-\d.]+) (\d+)\)'
                         r'.*?\(name "([^"]*)".*?\(number "([^"]*)"', blk, re.S):
        x, y, a, nm, num = float(m.group(1)), float(m.group(2)), int(m.group(3)), m.group(4), m.group(5)
        out[num] = (x, y, a, nm)
    return out


# ---------------------------------------------------------------- the sheet
used = {}
items = []
refs = {}


def place(lib, name, ref, value, x, y, nets, footprint="", nc_rest=False):
    """a symbol at (x, y) mm; nets: pin number or name -> net ('NC': no connect)"""
    g = 1.27                                   # the connection grid
    x, y = round(x / g) * g, round(y / g) * g
    key = lib + ":" + name
    if key not in used:
        used[key] = flat(lib, name)
    pp = pins(lib, name)
    byname = {}
    for k, v in pp.items():
        byname.setdefault(v[3], []).append(k)
    pin_uuids = ''.join('\n\t\t(pin "%s" (uuid "%s"))' % (n, uid()) for n in pp)
    items.append(f'''	(symbol
		(lib_id "{key}")
		(at {x:.2f} {y:.2f} 0)
		(unit 1)
		(exclude_from_sim no)
		(in_bom yes)
		(on_board yes)
		(dnp no)
		(uuid "{uid()}")
		(property "Reference" "{ref}" (at {x:.2f} {y - 2:.2f} 0) (effects (font (size 1.27 1.27))))
		(property "Value" "{value}" (at {x:.2f} {y + 2:.2f} 0) (effects (font (size 1.27 1.27))))
		(property "Footprint" "{footprint}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))
		(property "Datasheet" "" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes))){pin_uuids}
		(instances (project "s3_expansion" (path "/{ROOT}" (reference "{ref}") (unit 1))))
	)''')
    todo = []
    for k, net in nets.items():
        nums = [k] if k in pp else byname.get(k)
        if not nums:
            raise KeyError(f"{ref}: no pin {k}")
        todo += [(n, net) for n in nums]       # every pin of that name (GND, NC ...)
    if nc_rest:
        done = {n for n, _ in todo}
        todo += [(n, 'NC') for n in pp if n not in done]
    for num, net in todo:
        px, py, a, _ = pp[num]
        sx, sy = round(x + px, 2), round(y - py, 2)
        if net == 'NC':
            items.append(f'\t(no_connect (at {sx:.2f} {sy:.2f}) (uuid "{uid()}"))')
            continue
        la = {0: 180, 180: 0, 90: 270, 270: 90}[a]
        just = {0: 'left bottom', 180: 'right bottom', 90: 'left bottom', 270: 'right bottom'}[la]
        items.append(f'\t(label "{net}" (at {sx:.2f} {sy:.2f} {la}) (fields_autoplaced yes)'
                     f' (effects (font (size 1.27 1.27)) (justify {just})) (uuid "{uid()}"))')


def text(s, x, y, size=1.5):
    s = s.replace('"', '\\"')
    s = s.replace('\n', '\\n')
    items.append(f'\t(text "{s}" (exclude_from_sim no) (at {x:.2f} {y:.2f} 0)'
                 f' (effects (font (size {size} {size})) (justify left bottom)) (uuid "{uid()}"))')


_n = {'R': 0, 'C': 0, 'Q': 0, 'FLG': 0}


def R(value, a, b, x, y):
    _n['R'] += 1
    place('Device', 'R', f"R{_n['R']}", value, x, y, {'1': a, '2': b},
          "Resistor_SMD:R_0603_1608Metric")


def C(value, a, b, x, y):
    _n['C'] += 1
    place('Device', 'C', f"C{_n['C']}", value, x, y, {'1': a, '2': b},
          "Capacitor_SMD:C_0805_2012Metric")


def flag(net, x, y):
    _n['FLG'] += 1
    place('power', 'PWR_FLAG', f"#FLG0{_n['FLG']}", 'PWR_FLAG', x, y, {'1': net})


# ---------------------------------------------------------------- A2: sound, mouse, SD, pads, WiFi
a2 = {'1': 'GND', '2': '+5V', '3': '+3V3',
      '4': 'I2S_BCK', '5': 'I2S_LRCK', '6': 'I2S_DATA',
      '7': 'MS_CLK', '8': 'MS_DATA',
      '9': 'SD_CS', '10': 'SD_SCK', '11': 'SD_MOSI', '12': 'SD_MISO', '13': 'SD_CD',
      '14': 'PAD_SEL', '15': 'PAD1_UP', '16': 'PAD1_DOWN', '17': 'PAD1_LEFT',
      '18': 'WIFI_CS', '19': 'WIFI_SCK', '20': 'WIFI_MOSI', '21': 'WIFI_MISO', '22': 'WIFI_HS',
      '23': 'WIFI_EN', '24': 'WIFI_BOOT', '25': 'WIFI_TXD', '26': 'WIFI_RXD'}
a2.update({'27': 'PAD1_RIGHT', '28': 'PAD1_TL', '29': 'PAD1_TR', '30': 'PAD2_UP',
           '31': 'PAD2_DOWN', '32': 'PAD2_LEFT', '33': 'PAD2_RIGHT', '34': 'PAD2_TL',
           '35': 'PAD2_TR'})                   # 35: D9, the oscillator socket's (keep it empty)
for p in range(36, 41):
    a2[str(p)] = 'NC'
place('Connector_Generic', 'Conn_02x20_Odd_Even', 'J1', 'A2 (to the Spartan-3 board)', 40, 60, a2,
      "Connector_PinSocket_2.54mm:PinSocket_2x20_P2.54mm_Vertical")
text("A2 expansion connector (UG130 table 13-3)\n"
     "4 E6 I2S_BCK   5 D5 I2S_LRCK   6 C5 I2S_DATA\n"
     "7 D6 MS_CLK    8 C6 MS_DATA\n"
     "9 E7 SD_CS    10 C7 SD_SCK   11 D7 SD_MOSI\n"
     "12 C8 SD_MISO 13 D8 SD_CD\n"
     "14 C9 PAD_SEL (out)  15 D10 PAD1_UP 16 A3 PAD1_DOWN 17 B4 PAD1_LEFT\n"
     "27 B10 PAD1_RIGHT 28 A10 PAD1_TL 29 B11 PAD1_TR 30 B12 PAD2_UP\n"
     "31 A12 PAD2_DOWN 32 B13 PAD2_LEFT 33 A13 PAD2_RIGHT 34 B14 PAD2_TL\n"
     "35 D9 PAD2_TR (the oscillator socket's pin: leave the socket empty)\n"
     "18 A4 WIFI_CS 19 B5 WIFI_SCK 20 A5 WIFI_MOSI 21 B6 WIFI_MISO\n"
     "22 B7 WIFI_HS 23 A7 WIFI_EN 24 B8 WIFI_BOOT 25 A8 WIFI_TXD 26 A9 WIFI_RXD\n"
     "36-40 configuration: leave open\n"
     "2 +5V (VU), 3 +3.3V (VCCO), 1 GND", 15, 115, 1.27)
flag('+5V', 20, 30)
flag('+3V3', 40, 30)
flag('GND', 60, 30)

# ---- sound: PCM5102A, I2S from the FPGA, line out
place('Audio', 'PCM5102A', 'U1', 'PCM5102A', 120, 60, {
    'CPVDD': '+3V3', 'AVDD': '+3V3', 'DVDD': '+3V3',
    'CPGND': 'GND', 'AGND': 'GND', 'DGND': 'GND',
    'CAPP': 'CP_P', 'CAPM': 'CP_M', 'VNEG': 'VNEG', 'LDOO': 'LDOO',
    'BCK': 'I2S_BCK', 'LRCK': 'I2S_LRCK', 'DIN': 'I2S_DATA', 'SCK': 'GND',
    'FMT': 'GND', 'FLT': 'GND', 'DEMP': 'GND', 'XSMT': 'XSMT',
    'OUTL': 'DAC_L', 'OUTR': 'DAC_R'}, "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm")
C('2.2u', 'CP_P', 'CP_M', 150, 40)
C('2.2u', 'VNEG', 'GND', 160, 40)
C('1u', 'LDOO', 'GND', 170, 40)
C('10u', '+3V3', 'GND', 150, 85)
C('100n', '+3V3', 'GND', 160, 85)
C('100n', '+3V3', 'GND', 170, 85)
C('100n', '+3V3', 'GND', 180, 85)
R('10k', '+3V3', 'XSMT', 180, 40)
R('470', 'DAC_L', 'LINE_L', 195, 55)
R('470', 'DAC_R', 'LINE_R', 195, 70)
C('2.2n', 'LINE_L', 'GND', 210, 55)
C('2.2n', 'LINE_R', 'GND', 210, 70)
place('Connector_Audio', 'AudioJack3', 'J2', 'Line out (3.5 mm)', 235, 62,
      {'T': 'LINE_L', 'R': 'LINE_R', 'S': 'GND'}, "Connector_Audio:Jack_3.5mm_CUI_SJ1-3523N_Horizontal")
text("Sound: I2S (64 BCK a frame, 48,828 Hz) into a PCM5102A; SCK tied low:\n"
     "the DAC's PLL makes its clock from BCK. FMT low = I2S. XSMT high = unmuted.\n"
     "Line out through a 470R / 2.2nF filter.", 110, 100, 1.27)

# ---- PS/2 mouse: Mini-DIN-6 at 5 V, BSS138 level shifters to 3.3 V
place('Connector', 'Mini-DIN-6', 'J3', 'PS/2 mouse', 300, 60,
      {'1': 'MS_DATA5', '2': 'NC', '3': 'GND', '4': '+5V', '5': 'MS_CLK5', '6': 'NC'},
      "")                                      # no mini-DIN footprint in KiCad's library: e.g. CUI MD-60S
for i, (lo, hi) in enumerate((('MS_CLK', 'MS_CLK5'), ('MS_DATA', 'MS_DATA5'))):
    _n['Q'] += 1
    place('Transistor_FET', 'BSS138', f"Q{_n['Q']}", 'BSS138', 270, 40 + 30 * i,
          {'1': '+3V3', '2': lo, '3': hi}, "Package_TO_SOT_SMD:SOT-23")
    R('10k', '+3V3', lo, 255, 40 + 30 * i)
    R('10k', '+5V', hi, 285, 40 + 30 * i)
text("PS/2 mouse (the keyboard uses the board's own PS/2 port, M15/M16).\n"
     "Open drain both ways: the FPGA drives low or lets go. BSS138 level\n"
     "shifters (gate at 3.3 V); 10k pull-ups on both sides. Not 5 V tolerant!",
     245, 92, 1.27)

# ---- microSD, SPI mode
place('Connector', 'Micro_SD_Card_Det1', 'J4', 'microSD', 120, 150, {
    '1': 'SD_DAT2', '2': 'SD_CS', '3': 'SD_MOSI', '4': '+3V3', '5': 'SD_SCK_R',
    '6': 'GND', '7': 'SD_MISO', '8': 'SD_DAT1', '9': 'SD_CD', 'SH': 'GND'},
    "Connector_Card:microSD_HC_Hirose_DM3AT-SF-PEJM5")
R('33', 'SD_SCK', 'SD_SCK_R', 90, 135)
for k, net in enumerate(('SD_CS', 'SD_MOSI', 'SD_MISO', 'SD_DAT1', 'SD_DAT2', 'SD_CD')):
    R('47k', '+3V3', net, 150 + 10 * k, 135)
C('10u', '+3V3', 'GND', 150, 170)
C('100n', '+3V3', 'GND', 160, 170)
text("microSD in SPI mode: DAT3 = CS, CMD = MOSI, DAT0 = MISO; DAT1/DAT2 pulled\n"
     "up (unused); card detect to the FPGA. 47k pull-ups, 33R on the clock.",
     110, 185, 1.27)

# ---- two Sega Genesis (Mega Drive) pads on DE9 plugs, as on the console
# The console's DE9: 1 UP, 2 DOWN, 3 LEFT, 4 RIGHT, 5 +5V, 6 TL (B / A),
# 7 TH (select, from the console), 8 GND, 9 TR (C / Start).
SIG = ('UP', 'DOWN', 'LEFT', 'RIGHT', 'TL', 'TR')
DE9 = {'UP': '1', 'DOWN': '2', 'LEFT': '3', 'RIGHT': '4', 'TL': '6', 'TR': '9'}
for i in (1, 2):
    x = 230 + 50 * (i - 1)
    _n['F'] = _n.get('F', 0) + 1
    place('Device', 'Polyfuse', f"F{_n['F']}", '500mA', x - 15, 135, {'1': '+5V', '2': f'PAD{i}_5V'},
          "Fuse:Fuse_1206_3216Metric")
    nets = {'5': f'PAD{i}_5V', '8': 'GND', '7': f'PAD{i}_TH'}
    for sg in SIG:
        nets[DE9[sg]] = f'P{i}_{sg}'
    place('Connector', 'DE9_Pins', f"J{4 + i}", f'Genesis pad {i}', x, 150, nets,
          "Connector_Dsub:DSUB-9_Pins_Horizontal_P2.77x2.84mm_EdgePinOffset7.70mm_Housed_MountingHolesOffset9.12mm")
    R('100', 'PAD_SEL5', f'PAD{i}_TH', x - 15, 165)
    C('100n', f'PAD{i}_5V', 'GND', x + 15, 135)

# the pads' outputs (5 V) into the FPGA (3.3 V, not 5 V tolerant): two 74LVC245A
# at 3.3 V, A to B; their inputs take 5 V. Pull-ups to 3.3 V: no pad reads all 1s.
ins = [(i, sg) for i in (1, 2) for sg in SIG]
for u in (0, 1):
    nets = {'1': '+3V3', '19': 'GND', '20': '+3V3', '10': 'GND'}
    for k in range(8):
        n = 8 * u + k
        if n < len(ins):
            i, sg = ins[n]
            nets[str(2 + k)] = f'P{i}_{sg}'
            nets[str(18 - k)] = f'PAD{i}_{sg}'
            R('10k', '+3V3', f'P{i}_{sg}', 440 + 7 * k, 40 + 75 * u)
        else:
            nets[str(2 + k)] = 'GND'
            nets[str(18 - k)] = 'NC'
    place('74xx', '74LS245', f"U{5 + u}", '74LVC245A', 470, 80 + 75 * u, nets,
          "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm")
    C('100n', '+3V3', 'GND', 500, 60 + 75 * u)
# select out at 5 V levels: a 74AHCT1G125 at 5 V (TTL inputs take 3.3 V)
place('74xGxx', '74AHCT1G125', 'U7', '74AHCT1G125', 470, 225,
      {'1': 'GND', '2': 'PAD_SEL', '3': 'GND', '4': 'PAD_SEL5', '5': '+5V'},
      "Package_TO_SOT_SMD:SOT-23-5")
C('100n', '+5V', 'GND', 500, 215)
R('10k', '+3V3', 'PAD_SEL', 445, 215)
text("Sega Genesis / Mega Drive pads (3- and 6-button) on DE9 plugs, at 5 V\n"
     "(each through a 500 mA polyfuse). One select (TH) for both pads, driven\n"
     "at 5 V by U7; 100R to each port. The 12 inputs come back through two\n"
     "74LVC245A at 3.3 V (5 V-tolerant inputs): the Spartan-3's pins are not.\n"
     "TH high: UP DOWN LEFT RIGHT B C; TH low: UP DOWN 0 0 A Start. A 6-button\n"
     "pad answers its 3rd low with UP..RIGHT all 0, and its 4th high with\n"
     "Z Y X Mode; it resets 1.5 ms after the last pulse. Software reads both pads.\n"
     "With TH low, LEFT and RIGHT at 1 (the pull-ups): no pad.", 430, 255, 1.27)

# ---- WiFi: ESP32-WROOM-32E, an SPI slave on its own pins (VSPI), own 3.3 V
place('Regulator_Linear', 'AMS1117-3.3', 'U3', 'AMS1117-3.3', 330, 125,
      {'VI': '+5V', 'VO': '+3V3W', 'GND': 'GND'}, "Package_TO_SOT_SMD:SOT-223-3_TabPin2")
C('10u', '+5V', 'GND', 315, 125)
C('22u', '+3V3W', 'GND', 345, 125)
place('RF_Module', 'ESP32-WROOM-32E', 'U4', 'ESP32-WROOM-32E', 330, 175, {
    'VDD': '+3V3W', 'GND': 'GND', 'EN': 'WIFI_EN', 'IO0': 'WIFI_BOOT',
    'IO18': 'WIFI_SCK', 'IO23': 'WIFI_MOSI', 'IO19': 'WIFI_MISO', 'IO5': 'WIFI_CS',
    'IO4': 'WIFI_HS', 'IO16': 'WIFI_TXD', 'IO17': 'WIFI_RXD',
    'TXD0/IO1': 'ESP_TXD0', 'RXD0/IO3': 'ESP_RXD0'}, "RF_Module:ESP32-WROOM-32E", nc_rest=True)
R('10k', '+3V3W', 'WIFI_EN', 365, 150)
C('1u', 'WIFI_EN', 'GND', 375, 150)
R('10k', '+3V3W', 'WIFI_BOOT', 385, 150)
C('100n', '+3V3W', 'GND', 365, 200)
place('Connector_Generic', 'Conn_01x04', 'J8', 'ESP32 UART0 (firmware)', 380, 180,
      {'1': 'GND', '2': 'ESP_TXD0', '3': 'ESP_RXD0', '4': '+3V3W'},
      "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical")
text("WiFi: ESP32-WROOM-32E (e.g. ESP-AT firmware) on its own pins, not the SD bus:\n"
     "shared, the ESP's MISO would be GPIO12, a strap the SD pull-up would hold high\n"
     "(1.8 V flash: no boot). VSPI slave: SCK IO18, MOSI IO23, MISO IO19, CS IO5;\n"
     "IO4 = handshake/interrupt to the FPGA; EN (reset) and IO0 (download mode)\n"
     "driven by the FPGA (open drain; pull-ups, EN RC). UART1 (IO16 in, IO17 out)\n"
     "to the FPGA too; UART0 on J8 for flashing. Own AMS1117 from +5V: the module\n"
     "draws ~500 mA when transmitting. One SPI engine in the FPGA serves SD and WiFi.",
     318, 100, 1.0)

# ---------------------------------------------------------------- A1: NOR flash on the SRAM's bus
# A1 carries the board's SRAM address lines A0-A17, WE#, OE# and IC10's IO0-IO7
# (the FPGA's sram_d[7:0]); the flash sits on that bus with its own CE# (pin 21)
# and A18 (pin 20). The FPGA's SRAM PHY raises
# both SRAM chip enables during a flash cycle and waits out its 70 ns.
a1_sram_a = {6: 0, 8: 1, 10: 2, 12: 3, 14: 4, 24: 5, 23: 6, 26: 7, 25: 8, 28: 9, 27: 10, 30: 11,
             29: 12, 32: 13, 31: 14, 34: 15, 33: 16, 35: 17}
a1 = {'1': 'GND', '2': 'NC', '3': '+3V3', '4': 'NC', '16': 'SRAM_WE', '18': 'SRAM_OE',
      '20': 'ROM_A18', '21': 'ROM_CE', '22': 'NC'}  # as fpga/constraints/ramsys.ucf
for k, p in enumerate((5, 7, 9, 11, 13, 15, 17, 19)):
    a1[str(p)] = f'SRAM_IO{k}'
for p, k in a1_sram_a.items():
    a1[str(p)] = f'SRAM_A{k}'
for p in range(36, 41):
    a1[str(p)] = 'NC'                          # JTAG: leave alone
place('Connector_Generic', 'Conn_02x20_Odd_Even', 'J7', 'A1 (to the Spartan-3 board)', 40, 200, a1,
      "Connector_PinSocket_2.54mm:PinSocket_2x20_P2.54mm_Vertical")
text("A1 expansion connector (UG130 table 13-2): shared with the board's SRAM\n"
     "5-19 odd: IC10 IO0-IO7 (sram_d[7:0]); A0-A17: pins 6, 8, 10, 12, 14,\n"
     "24, 23, 26, 25, 28, 27, 30, 29, 32, 31, 34, 33, 35; 16 WE#, 18 OE#\n"
     "21 M7 ROM_CE (unshared), 20 P9 ROM_A18 (DOUT/BUSY: toggles while the FPGA\n"
     "configures, harmless on an address line); free: 4 N8, 22 M10\n"
     "36-40: JTAG (TMS, TCK, TDO): do not connect", 15, 245, 1.27)

fl = {'VCC': '+3V3', 'GND': 'GND', 'CE': 'ROM_CE', 'OE': 'SRAM_OE', 'PGM': 'SRAM_WE',
      'A18': 'ROM_A18'}
for k in range(18):
    fl[f'A{k}'] = f'SRAM_A{k}'
for k in range(8):
    fl[f'D{k}'] = f'SRAM_IO{k}'
place('Memory_Flash', 'SST39SF040', 'U2', 'SST39VF1601C (see the note)', 220, 228, fl,
      "Package_LCC:PLCC-32_THT-Socket")
R('10k', '+3V3', 'ROM_CE', 255, 215)
C('100n', '+3V3', 'GND', 265, 215)
text("NOR flash ROM, executed in place (a resident toolbox: no RAM spent on it).\n"
     "The part: SST39VF1601C-70 (1M x 16, 3.3 V, TSOP-48), DQ0-DQ7 used: 512 KB.\n"
     "Its A0-A18 as shown, A19 to GND, DQ8-DQ15 open (pulled up in the FPGA's\n"
     "simulation only), WP#/RST# to 3.3 V. (The symbol is a stand-in: KiCad has\n"
     "none for it; the connections are the same by name.) CE# pulled up:\n"
     "off while the FPGA configures. Writes (programming) use the JEDEC command\n"
     "sequences through the same bus, with a long WE# pulse.", 110, 225, 1.27)
text("J3 needs a mini-DIN-6 footprint (e.g. CUI MD-60S): none in KiCad's library.", 110, 240, 1.27)

text("fpga_cpu: Spartan-3 Starter Board expansion (rough). Serial and the keyboard\n"
     "are on the board itself (DB9: RXD T13, TXD R13; PS/2: M15/M16).", 15, 22, 2.0)

# ---------------------------------------------------------------- write
libs = '\n'.join('\t\t' + v.replace('\n', '\n\t') for v in used.values())
sch = f'''(kicad_sch
	(version 20250114)
	(generator "fpga_cpu gen_sch.py")
	(generator_version "1")
	(uuid "{ROOT}")
	(paper "A2")
	(title_block (title "fpga_cpu Spartan-3 expansion") (rev "0.1") (comment 1 "rough: sound, PS/2 mouse, microSD, Genesis pads, ESP32 WiFi (A2); NOR flash ROM on the SRAM bus (A1)"))
	(lib_symbols
{libs}
	)
{chr(10).join(items)}
	(sheet_instances (path "/" (page "1")))
	(embedded_fonts no)
)
'''
open(OUT, 'w').write(sch)
pro = os.path.join(HERE, "s3_expansion.kicad_pro")
if not os.path.exists(pro):
    open(pro, 'w').write('{\n  "meta": {"filename": "s3_expansion.kicad_pro", "version": 1}\n}\n')
print("wrote", OUT, len(items), "items")
