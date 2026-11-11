; Synthetic checker fixture, not a printer job.
; filament_flow_ratio = 1
; nozzle_temperature = 210
T0
M104 S210
G90
M83
; CHANGE_LAYER
; LAYER_HEIGHT: 0.2
; Z_HEIGHT: 0.2
; initial_layer_print_height = 0.2
; LINE_WIDTH: 0.45
; FEATURE: Inner wall
G1 Z0.2 F1200
G1 X10 Y0 E0.33846 F1200
G1 X10 Y10 E0.33846 F1200
