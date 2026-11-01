# T1 and its memory/I/O clock (16.875 MHz, 180 degrees apart), the 720p
# pixel clock (nets as yosys names them: cpu.clk, ios.clk). nextpnr does not
# check paths between cpu.clk and ios.clk, half a period apart: see "Max
# delay" in the log (under 29.6 ns).
create_clock -name cpu_clk -period 59.259 [get_nets {cpu.clk}]
create_clock -name mem_clk -period 59.259 [get_nets {ios.clk}]
create_clock -name pclk    -period 13.468 [get_nets {pclk}]
