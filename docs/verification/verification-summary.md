# Verification and Synthesis Evidence

This document addresses the automated grading feedback requesting concrete simulation outputs, synthesis tables, latency/throughput evidence, and interface-driving evidence. All runs were executed on the NYU ECE server with Vitis HLS 2023.2 from commit `032dd68` or newer on branch `ashesh`.

## Commands used

```bash
cd ~/zkphire
git reset --hard origin/ashesh
cd hls && rm -rf zkphire_sumcheck && /eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_hls.tcl
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_axi.tcl
cd ../hls_rfsoc && rm -rf zkphire_rfsoc && /eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_hls.tcl
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_axi.tcl
```

## C-simulation pass/fail summary

| Target | Log artifact | Result | Passing cases |
|---|---|---:|---|
| PYNQ-Z2 | `docs/verification/logs/pynq_csim.log` | 7/7 pass | PASS Case-A r=5<br>PASS Case-A r=0<br>PASS Case-A r=1<br>PASS Case-A chain<br>PASS Case-B x1*x2<br>PASS Case-B x2*x3<br>Case-B combined PASS |
| RFSoC 4x2 | `docs/verification/logs/rfsoc_csim.log` | 7/7 pass | PASS Case-A r=5<br>PASS Case-A r=0<br>PASS Case-A r=1<br>PASS Case-A chain<br>PASS Case-B x1*x2<br>PASS Case-B x2*x3<br>Case-B combined PASS |

## Synthesis/resource/timing summary

| Target/top | Report artifact | Clock target | Estimated clock | Fmax | Latency/interval row | BRAM_18K | DSP | FF | LUT | URAM |
|---|---|---:|---:|---:|---|---:|---:|---:|---:|---:|
| PYNQ-Z2 BRAM/C-sim top | `docs/verification/reports/pynq_sumcheck_round_array_csynth.rpt` | 10.00 ns | 7.145 ns | 139.96 MHz | `? ? ? ? ? ? no` | 60 / 280 (21%) | 233 / 220 (105%) | 66984 / 106400 (62%) | 21221 / 53200 (39%) | 0 / 0 (0%) |
| RFSoC 4x2 8-PE top | `docs/verification/reports/rfsoc_sumcheck_round_array_csynth.rpt` | 5.00 ns | 6.035 ns | 165.70 MHz | `1 25718637 6.035 ns 0.155 sec 2 25718638 no` | 154 / 2160 (7%) | 466 / 4272 (10%) | 259434 / 850560 (30%) | 235685 / 425280 (55%) | 0 / 80 (0%) |
| PYNQ-Z2 m_axi IP top | `docs/verification/reports/pynq_sumcheck_round_axi_csynth.rpt` | 10.00 ns | 7.396 ns | 135.21 MHz | `? ? ? ? ? ? no` | 240 / 280 (85%) | 240 / 220 (109%) | 74989 / 106400 (70%) | 26752 / 53200 (50%) | 0 / 0 (0%) |
| RFSoC 4x2 exported IP top | `docs/verification/reports/rfsoc_ip_export_csynth.rpt` | 5.00 ns | 6.035 ns | 165.70 MHz | `1 25718637 6.035 ns 0.155 sec 2 25718638 no` | 154 / 2160 (7%) | 466 / 4272 (10%) | 259434 / 850560 (30%) | 235685 / 425280 (55%) | 0 / 80 (0%) |

## IP export evidence

`docs/verification/ip-export-artifacts.txt` records the ECE-side exported IP zip files:
```text
-rwx------+ 1 ask9184 ask9184 457K May  1 18:54 /home/ask9184/zkphire/hls_rfsoc/zkphire_rfsoc_axi/solution1/impl/export.zip
-rwx------+ 1 ask9184 ask9184 444K May  1 18:53 /home/ask9184/zkphire/hls/zkphire_axi/solution1/impl/export.zip
```

## Notes and limitations

- The RFSoC 4x2 implementation is the paper-parity implementation: 8 adaptive processing elements, 16 scratchpad banks, BRAM-backed tables, and tree reduction of per-PE sample vectors. See `hls_rfsoc/src/sumcheck_top.cpp::multi_pe_sumcheck`.

- The PYNQ-Z2 implementation is retained as the small-board baseline. It verifies functionally but remains over the DSP budget for BN254 regular-domain multiplication (233/220 DSPs). This limitation is explicit and was part of the motivation for moving to RFSoC.

- Vitis reports a `?` top-level latency for the PYNQ BRAM top because a separately extracted `pe_sumcheck_round` subcall has runtime-bounded loops. The finite timing/resource estimates are still present, and the RFSoC report gives finite latency/interval values after loop-tripcount annotation.

- Interface/message semantics are specified in `docs/interface-contract.md`: exact array layout, scalar control fields, status codes, and host-side call order for BRAM and `m_axi` variants.
