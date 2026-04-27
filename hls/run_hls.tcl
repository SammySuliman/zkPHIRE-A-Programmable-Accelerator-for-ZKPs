# zkPHIRE SumCheck HLS Build Script
# Usage: vitis_hls -f run_hls.tcl
# Target: PYNQ-Z2 (xc7z020clg400-1) @ 100 MHz

set script_dir [file dirname [file normalize [info script]]]

open_project -reset zkphire_sumcheck
set_top sumcheck_round_array
add_files -cflags "-I$script_dir" src/sumcheck_top.cpp
add_files -tb -cflags "-I$script_dir" testbench/tb_sumcheck.cpp

open_solution -reset "solution1"
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

puts "--- C-Simulation ---"
if {[catch {csim_design} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: C-Simulation passed."

puts "--- C-Synthesis ---"
if {[catch {csynth_design} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: C-Synthesis passed."

puts "===== zkPHIRE SumCheck HLS Build Complete ====="
exit 0
