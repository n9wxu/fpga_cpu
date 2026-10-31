; Shared tail for the directed tests.
; exit(r2): stop the simulation with code r2.
exit:   LA      p0, 0xF00002
        STB     r2, [p0]
halt:   BRA     halt
