#!/usr/bin/env python3
"""Generate doomframe_data.h: the map, its BSP tree, the textures and
every table the doomframe renderer reads.

  gen_data.py > doomframe_data.h

Everything a real Doom would load from the WAD or compute at start-up
is made here, so the C program does no set-up work worth timing and
every machine (and the host build) renders from identical data. The
integer arithmetic of Doom's start-up code (R_InitTextureMapping,
R_ExecuteSetViewSize, R_InitLightTables) is reproduced exactly.

The map is a small level: a hall with two pillars, a raised platform,
a sunken side room behind a low doorway and a bright skylight, plus
three things. The node builder below is a plain one in the style of
Doom's: pick the partition seg that splits the fewest others, split,
recurse until every subsector is convex.
"""
import math
import random

# ---- sizes, kept in step with doomframe.c ----
SCREENWIDTH, SCREENHEIGHT = 128, 80     # Doom's 320x200 aspect
ORIGWIDTH = 320                 # Doom's lighting tables are tuned for 320 columns
FRACBITS = 16
FRACUNIT = 1 << FRACBITS
FINEANGLES = 512                # Doom: 8192
ANGLETOFINESHIFT = 32 - int(math.log2(FINEANGLES))
FIELDOFVIEW = FINEANGLES // 4   # 90 degrees
SLOPEBITS = 8                   # Doom: 11
SLOPERANGE = 1 << SLOPEBITS
NUMCOLORMAPS = 16               # Doom: 32
LIGHTLEVELS = 16
MAXLIGHTSCALE = 20              # Doom: 48 at 320 columns
MAXLIGHTZ = 32                  # Doom: 128 with LIGHTZSHIFT 20
LIGHTZSHIFT = 22
LIGHTSCALESHIFT = 12
DISTMAP = 2
TEXW, TEXH = 32, 64             # wall textures: 32 columns of 64 texels
FLAT = 32                       # floor and ceiling textures, 32 x 32
SPRW, SPRH = 32, 48
ANG90 = 0x40000000
M32 = 0xFFFFFFFF


def s32(v):
    v &= M32
    return v - (1 << 32) if v & 0x80000000 else v


def fixed_mul(a, b):
    return s32((a * b) >> FRACBITS)


def fixed_div(a, b):
    if (abs(a) >> 14) >= abs(b):
        return -0x80000000 if (a ^ b) < 0 else 0x7FFFFFFF
    q = abs(a << 16) // abs(b)
    return s32(-q if (a < 0) != (b < 0) else q)


# ---- trig tables, as Doom's R_InitTables and R_InitPointToAngle make them ----
finesine = [int(FRACUNIT * math.sin((i + 0.5) * math.pi * 2 / FINEANGLES)) for i in range(5 * FINEANGLES // 4)]
finetangent = [int(FRACUNIT * math.tan((i - FINEANGLES / 4 + 0.5) * math.pi * 2 / FINEANGLES))
               for i in range(FINEANGLES // 2)]
tantoangle = [int(0xFFFFFFFF * (math.atan(i / SLOPERANGE) / (math.pi * 2))) for i in range(SLOPERANGE + 1)]
finecosine = finesine[FINEANGLES // 4:]

# ---- view tables: R_InitTextureMapping and R_ExecuteSetViewSize ----
viewwidth, viewheight = SCREENWIDTH, SCREENHEIGHT
centerx, centery = viewwidth // 2, viewheight // 2
centerxfrac = centerx << FRACBITS
focallength = fixed_div(centerxfrac, finetangent[FINEANGLES // 4 + FIELDOFVIEW // 2])
viewangletox = []
for i in range(FINEANGLES // 2):
    if finetangent[i] > FRACUNIT * 2:
        t = -1
    elif finetangent[i] < -FRACUNIT * 2:
        t = viewwidth + 1
    else:
        t = fixed_mul(finetangent[i], focallength)
        t = (centerxfrac - t + FRACUNIT - 1) >> FRACBITS
        t = max(-1, min(viewwidth + 1, t))
    viewangletox.append(t)
xtoviewangle = []
for x in range(viewwidth + 1):
    i = 0
    while viewangletox[i] > x:
        i += 1
    xtoviewangle.append(((i << ANGLETOFINESHIFT) - ANG90) & M32)
viewangletox = [0 if t == -1 else viewwidth if t == viewwidth + 1 else t for t in viewangletox]
yslope = []
for i in range(viewheight):
    dy = abs(((i - viewheight // 2) << FRACBITS) + FRACUNIT // 2)
    yslope.append(fixed_div(viewwidth // 2 * FRACUNIT, dy))
distscale = [fixed_div(FRACUNIT, abs(finecosine[xtoviewangle[i] >> ANGLETOFINESHIFT])) for i in range(viewwidth)]

# light: Doom's formulas at 32 colormaps and 320 columns, halved to our 16 maps
scalelight, zlight = [], []
for i in range(LIGHTLEVELS):
    startmap = ((LIGHTLEVELS - 1 - i) * 2) * 32 // LIGHTLEVELS
    row = []
    for j in range(MAXLIGHTSCALE):
        level = startmap - j * ORIGWIDTH // viewwidth // DISTMAP
        row.append(max(0, min(31, level)) >> 1)
    scalelight.append(row)
    row = []
    for j in range(MAXLIGHTZ):
        scale = fixed_div(ORIGWIDTH // 2 * FRACUNIT, (j + 1) << LIGHTZSHIFT) >> LIGHTSCALESHIFT
        level = startmap - scale // DISTMAP
        row.append(max(0, min(31, level)) >> 1)
    zlight.append(row)

# ---- palette and colormaps: 16 ramps of 16 shades ----
rng = random.Random(1993)
HUES = [(1.00, 0.55, 0.30), (0.60, 0.60, 0.60), (0.55, 0.35, 0.20), (0.35, 0.55, 0.30),
        (0.85, 0.80, 0.55), (0.40, 0.45, 0.75), (0.90, 0.25, 0.20), (0.70, 0.60, 0.45),
        (0.30, 0.30, 0.35), (0.95, 0.85, 0.30), (0.50, 0.70, 0.70), (0.75, 0.45, 0.65),
        (0.45, 0.40, 0.30), (0.95, 0.95, 0.95), (0.25, 0.50, 0.25), (0.65, 0.30, 0.25)]
palette = []
for r, g, b in HUES:
    for s in range(16):
        k = (16 - s) / 16
        palette.append(tuple(min(255, int(255 * c * k + 0.5)) for c in (r, g, b)))


def nearest(rgb):
    return min(range(256), key=lambda i: sum((a - b) ** 2 for a, b in zip(palette[i], rgb)))


colormaps = []
for lvl in range(NUMCOLORMAPS):
    k = 1 - lvl / NUMCOLORMAPS
    colormaps += [nearest(tuple(c * k for c in palette[i])) for i in range(256)]


# ---- textures, column-major like Doom's composite textures ----
def ramp(hue, shade):
    return hue * 16 + max(0, min(15, shade))


def wall_brick(c, y):
    row = y // 8
    mortar = y % 8 == 7 or (c + (4 if row % 2 else 0)) % 16 == 15
    return ramp(8, 9) if mortar else ramp(2, 3 + rng.randrange(4))


def wall_stone(c, y):
    return ramp(12, 3 + rng.randrange(6) + (2 if (c // 8 + y // 16) % 2 else 0))


walls = []
for fn in (wall_brick, wall_stone):
    walls.append([fn(c, y) for c in range(TEXW) for y in range(TEXH)])
flats = [
    [ramp(7, 4 + rng.randrange(3) + (3 if (x // 8 + y // 8) % 2 else 0)) for y in range(FLAT) for x in range(FLAT)],
    [ramp(4, 2) if (x % 16 in (7, 8) and y % 16 in (7, 8)) else ramp(8, 5 + rng.randrange(2) + (x // 16 + y // 16) % 2)
     for y in range(FLAT) for x in range(FLAT)],
]

# sprite: a column-post patch (Doom's patch_t), a pillar lamp with a hole
spr_cols, spr_data = [], []
for c in range(SPRW):
    dx = (c - SPRW / 2 + 0.5) / (SPRW / 2)
    solid = [False] * SPRH
    for y in range(SPRH):
        body = abs(dx) < 0.55 and y >= 6
        head = (dx * dx + ((y - 8) / 8) ** 2) < 1
        hole = abs(dx) < 0.25 and 20 <= y < 30
        solid[y] = (body or head) and not hole
    spr_cols.append(len(spr_data))
    y = 0
    while y < SPRH:
        if not solid[y]:
            y += 1
            continue
        y0 = y
        while y < SPRH and solid[y]:
            y += 1
        texels = [ramp(9 if y1 < 16 else 1, 2 + abs(c - SPRW // 2) // 3 + rng.randrange(2)) for y1 in range(y0, y)]
        spr_data += [y0, y - y0, 0] + texels + [0]
    spr_data.append(0xFF)

# ---- the map ----
# Sectors: (floor, ceiling, floorpic, ceilingpic, light, wall texture), and
# loops of vertices with the sector on the right of each edge (y up):
# clockwise outer boundaries, counter-clockwise holes.
SECTORS = [
    dict(name="hall", floor=0, ceil=160, fpic=0, cpic=1, light=176, tex=1, loops=[
        [(0, 0), (0, 512), (320, 512), (320, 768), (1024, 768), (1024, 448), (1024, 320), (1024, 128), (896, 0)],
        [(576, 128), (768, 128), (768, 320), (576, 320)],          # around the skylight
        [(448, 256), (512, 256), (512, 320), (448, 320)],          # square pillar
        [(704, 464), (752, 512), (704, 560), (656, 512)],          # diamond pillar
    ]),
    dict(name="platform", floor=32, ceil=160, fpic=1, cpic=1, light=208, tex=2, loops=[
        [(0, 512), (0, 768), (320, 768), (320, 512)],
    ]),
    dict(name="side room", floor=-16, ceil=112, fpic=1, cpic=0, light=128, tex=2, loops=[
        [(1024, 192), (1024, 320), (1024, 448), (1024, 576), (1536, 576), (1536, 192)],
        [(1280, 352), (1344, 352), (1344, 416), (1280, 416)],      # pillar
    ]),
    dict(name="skylight", floor=0, ceil=208, fpic=0, cpic=0, light=240, tex=1, loops=[
        [(576, 128), (576, 320), (768, 320), (768, 128)],
    ]),
]
THINGS = [(600, 640, 0), (200, 640, 0x40000000), (1408, 256, 0x90000000)]
# camera: (x, y, angle in degrees), one view per frame
VIEWS = [(128, 128, 45), (512, 64, 90), (880, 384, 0), (1460, 520, 215),
         (160, 380, 75), (672, 224, 140), (960, 700, 250), (300, 300, 20)]


def signed_area(loop):
    return sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(loop, loop[1:] + loop[:1])) / 2


vertexes, vindex = [], {}


def vtx(p):
    p = (int(round(p[0])), int(round(p[1])))
    if p not in vindex:
        vindex[p] = len(vertexes)
        vertexes.append(p)
    return vindex[p]


edges = {}
for si, s in enumerate(SECTORS):
    for li, loop in enumerate(s["loops"]):
        a = signed_area(loop)
        assert (a < 0) == (li == 0), f"{s['name']} loop {li} has the wrong winding"
        for p, q in zip(loop, loop[1:] + loop[:1]):
            assert (p, q) not in edges, f"edge {p}->{q} twice"
            edges[(p, q)] = si

# linedefs and sidedefs: an edge whose reverse belongs to another sector is two-sided
lines, sides, done = [], [], set()
for (p, q), si in edges.items():
    if (p, q) in done:
        continue
    done.add((p, q))
    back = edges.get((q, p))
    if back is not None:
        done.add((q, p))
    front_tex = SECTORS[si]["tex"]
    if back is None:
        sides.append(dict(sector=si, top=0, bottom=0, mid=front_tex, xoff=0, yoff=0))
        lines.append(dict(v1=vtx(p), v2=vtx(q), side=[len(sides) - 1, -1], flags=0))
    else:
        bt = SECTORS[back]["tex"]
        sides.append(dict(sector=si, top=bt, bottom=bt, mid=0, xoff=0, yoff=0))
        sides.append(dict(sector=back, top=front_tex, bottom=front_tex, mid=0, xoff=0, yoff=0))
        lines.append(dict(v1=vtx(p), v2=vtx(q), side=[len(sides) - 2, len(sides) - 1], flags=0))

# ---- node builder ----
EPS = 0.5


def cross(px, py, dx, dy, x, y):
    """< 0: (x, y) is on the right of the line (Doom's front side)."""
    return dx * (y - py) - dy * (x - px)


def classify(part, sg):
    px, py = part["p1"]
    dx, dy = part["p2"][0] - px, part["p2"][1] - py
    n = math.hypot(dx, dy)
    c1 = cross(px, py, dx, dy, *sg["p1"]) / n
    c2 = cross(px, py, dx, dy, *sg["p2"]) / n
    z1, z2 = abs(c1) < EPS, abs(c2) < EPS
    if z1 and z2:
        sdx, sdy = sg["p2"][0] - sg["p1"][0], sg["p2"][1] - sg["p1"][1]
        return "front" if sdx * dx + sdy * dy > 0 else "back"
    if (c1 <= 0 or z1) and (c2 <= 0 or z2):
        return "front"
    if (c1 >= 0 or z1) and (c2 >= 0 or z2):
        return "back"
    return "split"


def split(part, sg):
    px, py = part["p1"]
    dx, dy = part["p2"][0] - px, part["p2"][1] - py
    c1 = cross(px, py, dx, dy, *sg["p1"])
    c2 = cross(px, py, dx, dy, *sg["p2"])
    t = c1 / (c1 - c2)
    x = sg["p1"][0] + t * (sg["p2"][0] - sg["p1"][0])
    y = sg["p1"][1] + t * (sg["p2"][1] - sg["p1"][1])
    m = vertexes[vtx((x, y))]
    a = dict(sg, p2=m)
    b = dict(sg, p1=m, offset=sg["offset"] + math.hypot(m[0] - sg["p1"][0], m[1] - sg["p1"][1]))
    return (a, b) if c1 < 0 else (b, a)          # (front piece, back piece)


segs0 = []
for li, ln in enumerate(lines):
    p, q = vertexes[ln["v1"]], vertexes[ln["v2"]]
    segs0.append(dict(p1=p, p2=q, line=li, side=0, offset=0.0))
    if ln["side"][1] >= 0:
        segs0.append(dict(p1=q, p2=p, line=li, side=1, offset=0.0))

nodes, subsectors, segs_out = [], [], []


def bbox(sgs):
    xs = [p[0] for s in sgs for p in (s["p1"], s["p2"])]
    ys = [p[1] for s in sgs for p in (s["p1"], s["p2"])]
    return (max(ys), min(ys), min(xs), max(xs))        # BOXTOP, BOXBOTTOM, BOXLEFT, BOXRIGHT


def build(sgs):
    best = None
    for cand in sgs:
        nf = nb = ns = 0
        for sg in sgs:
            c = classify(cand, sg)
            nf += c == "front"
            nb += c == "back"
            ns += c == "split"
        if nb + ns == 0 or nf + ns == 0:
            continue
        score = ns * 8 + abs(nf - nb)
        if best is None or score < best[0]:
            best = (score, cand)
    if best is None:                       # convex: a subsector
        sec = sides[lines[sgs[0]["line"]]["side"][sgs[0]["side"]]]["sector"]
        assert all(sides[lines[s["line"]]["side"][s["side"]]]["sector"] == sec for s in sgs)
        subsectors.append(dict(first=len(segs_out), num=len(sgs), sector=sec))
        segs_out.extend(sgs)
        return 0x8000 | (len(subsectors) - 1)
    part = best[1]
    front, back = [], []
    for sg in sgs:
        c = classify(part, sg)
        if c == "split":
            f, b = split(part, sg)
            front.append(f)
            back.append(b)
        else:
            (front if c == "front" else back).append(sg)
    node = dict(x=part["p1"][0], y=part["p1"][1], dx=part["p2"][0] - part["p1"][0],
                dy=part["p2"][1] - part["p1"][1], bbox=[bbox(front), bbox(back)])
    node["children"] = [build(front), build(back)]
    nodes.append(node)
    return len(nodes) - 1


root = build(segs0)


def point_on_side(x, y, nd):
    return 0 if cross(nd["x"], nd["y"], nd["dx"], nd["dy"], x, y) < 0 else 1


def point_in_sector(x, y):
    n = root
    while not n & 0x8000:
        n = nodes[n]["children"][point_on_side(x, y, nodes[n])]
    return subsectors[n & 0x7FFF]["sector"]


def bam(dx, dy):
    return int(math.atan2(dy, dx) / (2 * math.pi) * (1 << 32)) & M32


# ---- write the header ----
out = []
w = out.append


def arr(ctype, name, vals, per=12, fmt=str):
    w(f"{ctype} {name}[{len(vals)}] = {{")
    for i in range(0, len(vals), per):
        w("    " + ", ".join(fmt(v) for v in vals[i:i + per]) + ",")
    w("};")


hx = lambda v: f"0x{v & M32:08x}"
w("/* Generated by gen_data.py: do not edit. */")
for k, v in (("SCREENWIDTH", SCREENWIDTH), ("SCREENHEIGHT", SCREENHEIGHT), ("FINEANGLES", FINEANGLES),
             ("ANGLETOFINESHIFT", ANGLETOFINESHIFT), ("SLOPERANGE", SLOPERANGE), ("SLOPEBITS", SLOPEBITS),
             ("NUMCOLORMAPS", NUMCOLORMAPS), ("LIGHTLEVELS", LIGHTLEVELS), ("MAXLIGHTSCALE", MAXLIGHTSCALE),
             ("LIGHTSCALESHIFT", LIGHTSCALESHIFT), ("MAXLIGHTZ", MAXLIGHTZ), ("LIGHTZSHIFT", LIGHTZSHIFT),
             ("TEXW", TEXW), ("TEXH", TEXH), ("NUMWALLTEX", len(walls)), ("FLAT", FLAT), ("SPRW", SPRW), ("SPRH", SPRH)):
    w(f"#define {k} {v}")
w(f"#define NUMVERTEXES {len(vertexes)}\n#define NUMSECTORS {len(SECTORS)}\n#define NUMSIDES {len(sides)}")
w(f"#define NUMLINES {len(lines)}\n#define NUMSEGS {len(segs_out)}\n#define NUMSUBSECTORS {len(subsectors)}")
w(f"#define NUMNODES {len(nodes)}\n#define ROOTNODE {root}\n#define NUMTHINGS {len(THINGS)}\n#define NUMVIEWS {len(VIEWS)}")
w("")
arr("const fixed_t", "finesine", finesine, 8)
arr("const fixed_t", "finetangent", finetangent, 8)
arr("const angle_t", "tantoangle", tantoangle, 6, hx)
arr("const short", "viewangletox", viewangletox, 16)
arr("const angle_t", "xtoviewangle", xtoviewangle, 6, hx)
arr("const fixed_t", "yslope", yslope, 8)
arr("const fixed_t", "distscale", distscale, 8)
arr("const byte", "scalelightmap", [v for r in scalelight for v in r], 24)
arr("const byte", "zlightmap", [v for r in zlight for v in r], 32)
arr("const byte", "colormaps", colormaps, 16)
w(f"const byte walltextures[{len(walls)}][{TEXW * TEXH}] = {{")
for t in walls:
    w("  {" + ",".join(map(str, t)) + "},")
w("};")
w(f"const byte flats[{len(flats)}][{FLAT * FLAT}] = {{")
for t in flats:
    w("  {" + ",".join(map(str, t)) + "},")
w("};")
arr("const unsigned short", "sprite_columnofs", spr_cols, 16)
arr("const byte", "sprite_posts", spr_data, 24)
w("#ifdef REFERENCE")
arr("const byte", "palette", [c for p in palette for c in p], 24)
w("#endif")
w("")
w("extern const mobj_t things[NUMTHINGS];")
w("const vertex_t vertexes[NUMVERTEXES] = {")
for x, y in vertexes:
    w(f"    {{{x} * FRACUNIT, {y} * FRACUNIT}},")
w("};")
thing_sec = [point_in_sector(x, y) for x, y, _ in THINGS]
w("const sector_t sectors[NUMSECTORS] = {")
for si, s in enumerate(SECTORS):
    first = next((f"&things[{i}]" for i, ts in enumerate(thing_sec) if ts == si), "0")
    w(f"    {{{s['floor']} * FRACUNIT, {s['ceil']} * FRACUNIT, {s['fpic']}, {s['cpic']}, {s['light']}, {si}, {first}}},"
      f"   /* {s['name']} */")
w("};")
w("const side_t sides[NUMSIDES] = {")
for sd in sides:
    w(f"    {{{sd['xoff']} * FRACUNIT, {sd['yoff']} * FRACUNIT, {sd['top']}, {sd['bottom']}, {sd['mid']}, &sectors[{sd['sector']}]}},")
w("};")
w("const line_t lines[NUMLINES] = {")
for ln in lines:
    (x1, y1), (x2, y2) = vertexes[ln["v1"]], vertexes[ln["v2"]]
    s0, s1 = ln["side"]
    w(f"    {{&vertexes[{ln['v1']}], &vertexes[{ln['v2']}], {x2 - x1} * FRACUNIT, {y2 - y1} * FRACUNIT, {ln['flags']}, "
      f"{{{s0}, {s1}}}, &sectors[{sides[s0]['sector']}], {'&sectors[%d]' % sides[s1]['sector'] if s1 >= 0 else '0'}}},")
w("};")
w("const seg_t segs[NUMSEGS] = {")
for sg in segs_out:
    ln = lines[sg["line"]]
    sd = ln["side"][sg["side"]]
    other = ln["side"][sg["side"] ^ 1]
    ang = bam(sg["p2"][0] - sg["p1"][0], sg["p2"][1] - sg["p1"][1])
    w(f"    {{&vertexes[{vindex[sg['p1']]}], &vertexes[{vindex[sg['p2']]}], {int(round(sg['offset']))} * FRACUNIT, {hx(ang)}, "
      f"&sides[{sd}], &lines[{sg['line']}], &sectors[{sides[sd]['sector']}], "
      f"{'&sectors[%d]' % sides[other]['sector'] if other >= 0 else '0'}}},")
w("};")
w("const subsector_t subsectors[NUMSUBSECTORS] = {")
for ss in subsectors:
    w(f"    {{&sectors[{ss['sector']}], {ss['num']}, {ss['first']}}},")
w("};")
w("const node_t nodes[NUMNODES] = {")
for nd in nodes:
    bb = ", ".join("{" + ", ".join(f"{v} * FRACUNIT" for v in b) + "}" for b in nd["bbox"])
    w(f"    {{{nd['x']} * FRACUNIT, {nd['y']} * FRACUNIT, {nd['dx']} * FRACUNIT, {nd['dy']} * FRACUNIT, {{{bb}}}, "
      f"{{{nd['children'][0]}, {nd['children'][1]}}}}},")
w("};")
w("const mobj_t things[NUMTHINGS] = {")
for i, (x, y, a) in enumerate(THINGS):
    nxt = next((f"&things[{j}]" for j in range(i + 1, len(THINGS)) if thing_sec[j] == thing_sec[i]), "0")
    w(f"    {{{x} * FRACUNIT, {y} * FRACUNIT, {SECTORS[thing_sec[i]]['floor']} * FRACUNIT, {hx(a)}, {nxt}}},")
w("};")
w("const struct view views[NUMVIEWS] = {")
for x, y, deg in VIEWS:
    w(f"    {{{x} * FRACUNIT, {y} * FRACUNIT, {hx(int(deg / 360 * (1 << 32)))}}},")
w("};")
print("\n".join(out))
