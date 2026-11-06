/* Renames the interpreter's globals so a second copy of interp.c can run beside the first
 * (the stepping comparison in tests/testrun.c). */
#define mem6502 smem6502
#define attr6502 sattr6502
#define cpu scpu
#define sidlog ssidlog
#define sid_n ssid_n
#define sid_dropped ssid_dropped
#define io_read_hook sio_read_hook
#define io_write_hook sio_write_hook
#define interp_nominal sinterp_nominal
#define cpu_getp scpu_getp
#define cpu_setp scpu_setp
#define sid_log_clear ssid_log_clear
#define sid_reset ssid_reset
#define sid_load ssid_load
#define io_read sio_read
#define io_write sio_write
#define interp_step sinterp_step
#define interp_run sinterp_run
#define smc_write ssmc_write
#define jit_flush sjit_flush
