# zkPHIRE SumCheck HLS — RFSoC 4x2 Build Script
# Target: xczu48dr-ffvg1517-2-e @ 200 MHz
# Usage: vitis_hls -f run_hls.tcl

set script_dir [file dirname [file normalize [info script]]]

open_project -reset zkphire_rfsoc
set_top sumcheck_round_array
add_files -cflags "-I$script_dir" src/sumcheck_top.cpp
add_files -tb -cflags "-I$script_dir" testbench/tb_sumcheck.cpp

open_solution -reset "solution1"
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 5 -name default

puts "--- C-Simulation ---"
if {[catch {csim_design} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: C-Simulation passed."

puts "--- C-Synthesis ---"
if {[catch {csynth_design} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: C-Synthesis passed."

puts "===== zkPHIRE RFSoC Build Complete ====="
exit 0
