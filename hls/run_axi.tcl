# zkPHIRE SumCheck AXI IP Build
# Usage: vitis_hls -f run_axi.tcl
# Synthesizes the m_axi board-ready version and exports IP

set script_dir [file dirname [file normalize [info script]]]

open_project -reset zkphire_axi
set_top sumcheck_round_axi
add_files -cflags "-I$script_dir" src/sumcheck_top.cpp

open_solution -reset "solution1"
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

puts "--- C-Synthesis (AXI) ---"
if {[catch {csynth_design} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: AXI synthesis passed."

puts "--- Export IP ---"
if {[catch {export_design -format ip_catalog} res]} { puts "FAIL: $res"; exit 1 }
puts "PASS: IP exported to zkphire_axi/solution1/impl/ip/"

puts "===== zkPHIRE AXI Build Complete ====="
exit 0
