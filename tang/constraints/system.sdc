# fclk (67.5 MHz: T1F, memory, I/O, SDRAM, video DMA, audio) and the 720p pixel clock (nets as yosys
# names them). T1F is synchronous (SYNC): it runs on fclk with a clock enable, and every path to and
# from it is timed at fclk. (A T1 build, CORE=t1, runs T1 on a DQCE-gated copy of fclk, cpu.clk,
# whose paths to fclk are not timed: add create_clock -name cpu_clk -period 29.630 [get_nets {cpu.clk}].)
create_clock -name fclk    -period 14.815 [get_nets {fclk}]
create_clock -name pclk    -period 13.468 [get_nets {pclk}]
