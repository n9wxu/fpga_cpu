; Reset addresses the core cannot run from: 0x100000-0xEFFFFF (no memory,
; and not representable) and odd ones.  fsim faults on the first fetch, T1
; halts with FD.
; entries: 0x100000 0x200000 0xEFFFFE 0x1001
        .org    0x1000
        MOVI    r2, 0
        LA      p0, 0xF00002
        STB     r2, [p0]
