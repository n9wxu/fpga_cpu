# fclk (67.5 MHz: memory, I/O, SDRAM, video DMA), T1's gated copy of it, and
# the 720p pixel clock (nets as yosys names them). nextpnr does not check
# paths between cpu.clk and fclk: T1's bus is taken two fclk cycles after
# its edge, and what it reads is held from at least one cycle before. A T1
# cycle is at least four fclk cycles.
create_clock -name fclk    -period 14.815 [get_nets {fclk}]
create_clock -name cpu_clk -period 59.259 [get_nets {cpu.clk}]
create_clock -name pclk    -period 13.468 [get_nets {pclk}]
