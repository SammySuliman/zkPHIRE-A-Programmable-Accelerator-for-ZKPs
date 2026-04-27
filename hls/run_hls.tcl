# zkPHIRE SumCheck HLS Build Script
# Usage: vitis_hls -f run_hls.tcl
# Target: PYNQ-Z2 (xc7z020clg400-1) @ 100 MHz

set script_dir [file dirname [file normalize [info script]]]

# ==================================================================
# Solution 1: sumcheck_round_array — C-sim + synthesis
# ==================================================================
open_project -reset zkphire_sumcheck
set_top sumcheck_round_array
add_files -cflags "-I$script_dir" src/sumcheck_top.cpp
add_files -tb -cflags "-I$script_dir" testbench/tb_sumcheck.cpp

open_solution -reset "solution1"
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

puts "--- C-Simulation (array API) ---"
if {[catch {csim_design} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: C-Simulation passed."

puts "--- C-Synthesis (array API) ---"
if {[catch {csynth_design} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: C-Synthesis passed."

# ==================================================================
# Solution 2: sumcheck_round_axi — synthesis + IP export
# ==================================================================
open_solution -reset "solution2"
set_top sumcheck_round_axi
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

puts "--- C-Synthesis (AXI API) ---"
if {[catch {csynth_design} res]} {
    puts "WARNING: AXI synthesis failed."; puts $res
} else {
    puts "PASS: AXI synthesis passed."
}

puts "--- Exporting IP ---"
if {[catch {export_design -format ip_catalog} res]} {
    puts "WARNING: IP export failed."
} else {
    puts "PASS: IP exported."
}

puts "===== zkPHIRE SumCheck HLS Build Complete ====="
exit 0
