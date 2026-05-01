# zkPHIRE SumCheck IP Export — RFSoC 4x2
# Usage: vitis_hls -f run_axi.tcl

set script_dir [file dirname [file normalize [info script]]]

open_project -reset zkphire_rfsoc_axi
set_top sumcheck_round_array
add_files -cflags "-I$script_dir" src/sumcheck_top.cpp

open_solution -reset "solution1"
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 5 -name default

puts "--- C-Synthesis ---"
if {[catch {csynth_design} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: Synthesis passed."

puts "--- Export IP ---"
if {[catch {export_design -format ip_catalog} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: IP exported."

puts "===== zkPHIRE RFSoC IP Export Complete ====="
exit 0
