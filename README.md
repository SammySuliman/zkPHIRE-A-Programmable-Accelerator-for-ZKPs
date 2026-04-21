# zkPHIRE: A Programmable Accelerator for ZKPs

**Source**: [zkPHIRE: A Programmable Accelerator for ZKPs over HIgh-degRee, Expressive Gates](https://arxiv.org/pdf/2508.16738)

**Team members:** Sammy Suliman, Jui-Teng Huang, Ashesh Kaji  
**Topic**: We want to develop a hardware accelerator for one round (or full loop) of the Sum-check protocol over a finite field.

## Current HLS Implementation

The repository now includes a first functional Vitis HLS implementation for one programmable SumCheck round in [`hls/`](hls/). It follows the verification-first flow in [`SPEC.md`](SPEC.md):

```bash
python3 golden_sumcheck.py --test
make -C hls test
```

On a machine with Vitis HLS installed:

```bash
make -C hls vitis-csim
make -C hls vitis-csynth
```

See [`hls/README.md`](hls/README.md) for the stream protocol, host-facing API, testbench coverage, and IP packaging notes.

At a high level, each round does:

1. Fix previous variables  
2. Evaluate a polynomial over many points  
3. Sum results to produce a univariate polynomial  
4. Output coefficients → next round

**Motivation**: The Sum-Check protocol is well-suited for hardware acceleration because it relies on many parallelizable multiplications. We need to evaluate a function on many inputs**,** then sum them– all independently.

SumCheck repeatedly does **“sum over many values”** operations.

Hardware implements this as a **reduction tree**:

a1   a2   a3   a4   a5   a6   a7   a8

\\   /     \\   /     \\   /     \\   /

 s1        s2        s3        s4

   \\      /            \\      /

       t1                  t2

           \\            /

                total

This structure:

* Has low latency (logarithmic depth)  
* Is easy to pipeline  
* Is extremely efficient in hardware

SumCheck is **round-based**:

* Round 1 → compute 𝑔1g1​  
* Round 2 → compute 𝑔2g2​  
* …

Each round:

* Takes inputs  
* Produces a small output polynomial  
* Feeds into the next round

This is ideal for **pipelining**:

* While one round is finishing, the next can start  
* Data flows continuously through the hardware

So hardware can:

* Compute thousands of f(⋅)f(\\cdot)f(⋅) evaluations at the same time  
* Reduce them with parallel adders (tree reduction)  
* Combine them together in a pipeline

**SumCheck Protocol:**

SumCheck is a **protocol to verify that a sum over a big polynomial is correct** without computing the whole sum yourself.

Imagine you have a polynomial f(x\_1, x\_2, ..., x\_n) and you claim that:

![][image1]

You (the prover) want to convince someone else (the verifier) that SSS is correct **without actually sending all 2ⁿ evaluations** (which would be huge for large n).

SumCheck lets the verifier check this **efficiently** by interacting with the prover.

---

How it works (step by step)

1. **Start with the first variable x1x\_1x1​:**  
   * The prover computes a new polynomial g1(x1)=∑x2,...,xnf(x1,x2,...,xn)g\_1(x\_1) \= \\sum\_{x\_2,...,x\_n} f(x\_1, x\_2,...,x\_n)g1​(x1​)=∑x2​,...,xn​​f(x1​,x2​,...,xn​).  
   * This is a **polynomial in just x1x\_1x1​**.  
   * The prover sends g1g\_1g1​ to the verifier.  
2. **Verifier checks a sum:**  
   * Verifier picks a random value r1r\_1r1​ for x1x\_1x1​ and asks the prover to prove that g1(r1)=∑x2,...,xnf(r1,x2,...,xn)g\_1(r\_1) \= \\sum\_{x\_2,...,x\_n} f(r\_1, x\_2,...,x\_n)g1​(r1​)=∑x2​,...,xn​​f(r1​,x2​,...,xn​).  
3. **Repeat for each variable:**  
   * For x2x\_2x2​, the prover creates g2(x2)=∑x3,...,xnf(r1,x2,...,xn)g\_2(x\_2) \= \\sum\_{x\_3,...,x\_n} f(r\_1, x\_2, ..., x\_n)g2​(x2​)=∑x3​,...,xn​​f(r1​,x2​,...,xn​).  
   * Verifier picks r2r\_2r2​ and asks for g2(r2)g\_2(r\_2)g2​(r2​).  
   * Continue until all variables are “fixed” by random values.  
4. **Final check:**  
   * Eventually, you’re left with **a single number**, f(r1,r2,...,rn)f(r\_1, r\_2, ..., r\_n)f(r1​,r2​,...,rn​), which the verifier can compute directly.  
   * If all checks pass, the sum claim is probably correct.

Key idea: verifier only does **linear work in n**, not exponential 2n2^n2n.

**Summary (Sammy’s Transcribed Notes)**:

Zero Knowledge Proofs

* Enable one party to convince another of a statement’s validity **without revealing anything else**  
* High computational overhead → motivates **hardware acceleration**  
* ZKP protocols rely on **SumCheck protocol** *(O(N))*  
* Encode computations as **arithmetic circuits (AC)**

---

SumCheck

* Custom, high-degree gates  
  * Example:  
     f=a⋅b5+cf \= a \\cdot b^5 \+ cf=a⋅b5+c  
* Reduces the number of gates needed

---

Challenges

* Computations vary with:  
  * Each gate’s structure  
  * Polynomial degree  
* Cannot easily handle complex gates

---

Introducing zkPHIRE

* A novel **programmable SumCheck unit**  
* Supports:  
  * High-degree multilinear polynomials  
  * Compatible gate types

---

Zero Knowledge Proofs (Properties / Prereqs)

* Verifier learns nothing beyond validity of the statement  
* Verified quickly  
* No back-and-forth communication

---

Multi-Scalar Multiplication

* Compute:  
   S=∑siPiS \= \\sum s\_i P\_iS=∑si​Pi​  
   along an elliptic curve  
* Computationally intensive

---

SumCheck

![][image1]

* Prover (P) wants to convince Verifier (V):  
   f(X1,…,Xn)=Cf(X\_1, \\ldots, X\_n) \= Cf(X1​,…,Xn​)=C  
   where inputs are **Boolean**

---

Iterative Definition

For 1≤i≤n1 \\le i \\le n1≤i≤n:

si=∑xi∈{0,1}∑f(r1,r2,…,ri−1,xi,Xi+1,…,Xn)s\_i \= \\sum\_{x\_i \\in \\{0,1\\}} \\sum f(r\_1, r\_2, \\ldots, r\_{i-1}, x\_i, X\_{i+1}, \\ldots, X\_n)si​=xi​∈{0,1}∑​∑f(r1​,r2​,…,ri−1​,xi​,Xi+1​,…,Xn​)

* r1,…,ri−1r\_1, \\ldots, r\_{i-1}r1​,…,ri−1​ are **verifier’s random challenges** from previous rounds

---

Multilinear Extension (MLE)

* Multilinear extension \= polynomials with:  
  * Degree ≤ 1 in each variable

---

Plonk

f=qLw1+qRw2+qMw1w2−qOw3+qCf \= q\_L w\_1 \+ q\_R w\_2 \+ q\_M w\_1 w\_2 \- q\_O w\_3 \+ q\_Cf=qL​w1​+qR​w2​+qM​w1​w2​−qO​w3​+qC​

* Encodes an operation  
* Evaluated to compute fff

---

Encoding

X=(X1,X2,…)X \= (X\_1, X\_2, \\ldots)X=(X1​,X2​,…)

* Represents **binary-encoded gate index**  
* Example:  
   (1,1,0)→gate 3(1,1,0) \\rightarrow \\text{gate 3}(1,1,0)→gate 3  
* Functions:  
  * qL(X),w2(X),…q\_L(X), w\_2(X), \\ldotsqL​(X),w2​(X),…

---

Alternative Explanation of SumCheck

gj(t)=∑F(r1,…,rj−1,t,Xj+1,…,Xn)g\_j(t) \= \\sum F(r\_1, \\ldots, r\_{j-1}, t, X\_{j+1}, \\ldots, X\_n)gj​(t)=∑F(r1​,…,rj−1​,t,Xj+1​,…,Xn​)

---

Ex: (Sum/Check)  

n=2 (variables)  

k=2 (polynomials)  

f₁(x₁, x₂) \= 1 \+ 2x₁ \+ 3x₂ \+ 4x₁x₂  

f₂(x₁, x₂) \= 5 \+ 6x₁ \+ 7x₂ \+ 8x₁x₂  

E(x) \= f₁ \- f₂  

S \= ∑₍ₓ₁,ₓ₂ ∈ {0,1,3}₎ F(x₁, x₂)  

X \= {10, 0}, (0, 2), (1, 0), (1, 1, 3)}


S \= 5 \+ 33 \+ 48 \+ 260 \= 346


⭐ g₁(1) \= ∑₍ₓ₂ ∈ {0,3}₎ F(1, x₂) ⭐  WTS ∑₍₁₎ g(1) \= S  

Let x₂=0, ... f₁(1,0)=1+2t, f₂(1,0)=5+6t  

⇒ (1+2t)(5+6t)=5+16t+12t²  

Let x₂=1: f₁(1,1)=4+6t, f₂(1,1)=12+14t  

Now, g₁(0)=f₁(0,0)+f₂(0,0)=53  

g₁(1)=f₁(1,1)+f₂(1,1)=293  

S=53+293=346 ✓


\*Assuming x₁=r₁, what's the remaining partial sum over x₂?\*

New claim: S₂ \= ∑₍ₓ₂ ∈ {0,3}₎ F(r₁, x₂) \= g₁(r₁)

\= f₂(r₁, 0\) \+ f₂(r₁, 1\) \+ f₂(r₁, 1\) \+ f₂(r₁, 1\)  

\= f₂(2, 0\) \- f₂(2, 0\) \+ f₂(2, 1\) \- f₂(2, 1\)  

\= (5+12)(1+4)+(1+4+3+8)(5+12+7+16)  

\= (17 · 5\) \+ (16 · 40\)  

\= 85 \+ 640  

\= 725

Sumcheck of as opportunities for parallelism, but this voids opportunities for data reuse.

Instead of streaming all individuals, polynomials MLF ends simultaneously, we proceed one term at a time.

Hardware acceleration  

f₁(0, x₂, ...) → f₁(1, x₂, ...)  

f₁(1, x₂, ...) → f₁(2, x₂, ...)


→ f₁(2, 0\) \- f₁(2, 0\) \+ f₁(2, 1\) \- f₁(2, 1\)  

\= (5+12)(1+4)+(1+4+3+8)(5+12+7+16)  

\= (17 · 5\) \+ (16 · 40\)  

\= 85 \+ 640  

\= 725

ZKPHIRE advancements  

\- Implement product trees (parallel processing paths that independently compute and one f₁(x), f₂(x), f₃(x))  

\- These modular multiplies can be reconfigured for other protocol steps (not just sumcheck)

# **Architecture**

Input Stream →  | Input Buffer |

                                     ↓    (AXI-Stream FIFO)

                        | Polynomial Eval |  ← main compute

                                     ↓    (AXI-Stream FIFO)

                           | Accumulator |  ← sum over domain

                                     ↓    (AXI-Stream FIFO)

                         | Coeff Extractor |

                                     ↓    (AXI-Stream FIFO)

                        | Controller (FSM) |

---

Finite field unit

Everything runs over a field F\_p

Pick something hardware-friendly:

* 32-bit prime (easy FPGA)

---

## Polynomial Evaluation Unit (PEU)

This is your **main datapath**.

---

What it does

Evaluates something like:

g(x1,...,xn)g(x\_1, ..., x\_n)g(x1​,...,xn​)

But in practice:

* many terms  
* often multilinear

---

Hardware strategy

Use **Horner-like evaluation**:

Instead of:

ax2+bx+ca x^2 \+ b x \+ cax2+bx+c

Do:

y \= a

y \= y \* x \+ b

y \= y \* x \+ c

---

Implementation

* Chain of **MAC units** (multiply-accumulate)  
* Pipeline it:

Stage 1: multiply

Stage 2: add

Stage 3: reduce mod p

---

Parallelism

Instantiate multiple PEUs:

* 4, 8, 16 lanes (depending on FPGA)

Each evaluates a different input point.

---

## Input generation (very important)

You don’t want to store all inputs.

Instead:

Generate evaluation points **on the fly**

---

Example:

For Boolean hypercube:

* inputs ∈ {0,1}ⁿ

Use a **counter-based generator**:

* Treat index as bitmask  
* Each bit \= variable value

---

Benefit:

* Zero memory cost  
* Perfect streaming

---

## Accumulator (critical bottleneck)

This computes:

∑g(x)\\sum g(x)∑g(x)

---

Design

* Tree of adders OR  
* Running accumulator

---

Option A: Serial accumulator

* One adder  
* Low area  
* Slower

---

Option B: Parallel reduction tree

* Fast  
* More LUT usage

---

👉 Start with **serial**, then optimize.

---

## Coefficient extractor

In sumcheck, each round outputs a **univariate polynomial**:

h(t)=a0+a1t+a2t2+...h(t) \= a\_0 \+ a\_1 t \+ a\_2 t^2 \+ ...h(t)=a0​+a1​t+a2​t2+...

---

Trick

Instead of symbolic math:

* Evaluate at **multiple points**  
* Interpolate coefficients

---

Hardware approach:

1. Evaluate at:  
   * t \= 0  
   * t \= 1  
2. Solve small system (cheap)

---

## Controller (FSM)

Controls rounds:

IDLE

→ LOAD

→ EVALUATE (stream inputs)

→ ACCUMULATE

→ OUTPUT

→ NEXT ROUND

---

Responsibilities:

* Reset accumulators  
* Trigger PEU pipelines  
* Track iteration count

---

## External CPU (Host-PS Interaction)

1\. Physical connection

We need some way to send data and commands from the PC to the FPGA:

1. **USB** – simple, low-to-medium speed.  
2. Send messages using **UART** protocol  
3. Interface with hardware using a Python script and pynq library

**Expose hardware registers/memory in PL**:

* Use **AXI Lite / AXI DMA** interfaces to map registers or buffers that the PS can access.  
* Example:  
  * Register 0x00 → Start signal  
  * Register 0x04 → Round number / config  
  * DMA buffer → Polynomial data input/output

**USB communication**:

* PC talks to the PS (on FPGA) via **USB-UART bridge**.  
* The PS acts as a bridge between PC software and PL hardware.

**PYNQ on the PS**:

* Load your **bitstream (.bit file)** and overlay in PYNQ.  
* Map memory/registers from PL to Python-accessible objects.  
* Provide Python functions to **write commands, send input, read results**.

---

| Component | Role |
| ----- | ----- |
| **PC (Host)** | Generates input data (polynomial coefficients, circuit description), sends control commands, receives results, handles randomness for ZKP. |
| **PS (Processing System on FPGA)** | Acts as a bridge: receives PC commands over USB, writes data to PL (hardware accelerator), triggers computation, reads results from PL, sends results back to PC. |
| **PL (FPGA Fabric / Accelerator)** | Performs the heavy-lifting computations (SumCheck polynomial sums, reductions, etc.). |

---

Think of these as the **“conversation”**:

1. **Configuration / Initialization**  
   * Send metadata about the polynomial:  
     * Number of variables, polynomial degree, number of terms  
     * Memory addresses for input/output buffers  
   * Example: `CONFIG {num_vars=10, degree=3, buffer_addr=0x1000}`  
2. **Input Data Transfer**  
   * Send the actual **polynomial coefficients** (and any other data needed by SumCheck).  
   * For large data, use **DMA** to transfer blocks of coefficients efficiently.  
3. **Control Commands**  
   * Start computation: e.g., `START_SUMCHECK`  
   * Optional: Pause, reset, or stop signals  
   * Could be simple 1-bit register writes via AXI-Lite.  
4. **Verifier Random Challenges**  
   * In SumCheck, the verifier picks random values per round.  
   * Host sends the random challenge to PS → PL each round:  
     * `CHALLENGE {r1}`  
     * `CHALLENGE {r2}` …  
5. **Status Queries**  
   * Host can poll a **status register** to see if the accelerator is busy or finished.  
   * Example: `STATUS?` → returns `BUSY` or `DONE`  
6. **Retrieve Results**  
   * Once PL finishes computation, host reads results:  
     * SumCheck partial sums  
     * Final polynomial evaluation  
   * Example: `READ_RESULT` → returns array of results

---

## **3\. PC-side workflow (sending/receiving messages)**

1. **Send initialization/config command**  
2. **Transfer input data (coefficients, circuit description)**  
3. **Trigger computation (START command)**  
4. **Send random challenges for each round**  
5. **Poll or wait for done signal**  
6. **Read results from PL via PS**  
7. **Repeat next round if needed**

       **┌───────────────────┐**

       **│      PC Host      │**

       **│  (Python / PYNQ) │**

       **└────────┬──────────┘**

                **│ USB**

                **▼**

       **┌───────────────────┐**

       **│   PS (ARM CPU)    │**

       **│  on FPGA / Zynq   │**

       **└────────┬──────────┘**

                **│ AXI / DMA / Registers**

                **▼**

       **┌───────────────────┐**

       **│  PL (FPGA Logic)  │**

       **│  SumCheck Pipeline │**

       **└───────────────────┘**

9\) Memory design

Keep it minimal:

On-chip:

* small buffers for:  
  * coefficients  
  * partial sums

Avoid:

* large BRAM usage

---

10\) Dataflow (this is key)

You want:

**Fully streaming pipeline**

Input → PEU → Accumulator → Output

        ↑

    pipelined

Goal:

* 1 evaluation per cycle (after pipeline fill)

---

11\) Performance target (realistic)

For a good FPGA design:

* Throughput:  
   \~1–10 million evaluations/sec  
* Latency:  
   dominated by:  
  * number of inputs (2ⁿ if naive)

[image1]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAf8AAABVCAYAAABHAe2CAAAdz0lEQVR4Xu3dCVATZ/8H8KLigYiopR54FSnaQ22L+tYqtcq/FhG8EBRR3yqKgL4t3goYEBUVL6p4gQKiCHi0ilIdRcWiLYij1AN0UCoOyjEEmBCGZJKd7z8QVHIHSDAhv89MZjTZkN39ZZ/v7j5Pdj8AIYQQQgzKB9JPEEIIIaR1o/AnhBBCDAyFPyGEEGJgKPwJIYQQA0PhTwghhBgYCn9CCCHEwFD4E0IIIQaGwp8QQggxMBT+hBBCiIGh8CeEEEIMDIU/IYQQYmAo/AkhhBADQ+FPCCGEGBgKf0IIIcTAUPgTQgghBobCnxBCCDEwFP6EEEKIgaHwJ4QQQgwMhT8hhBBiYCj89RYft1cNhvEHH+CD2oeRMTqYdkGXLuo8TGDSqQOM2xiJ36vg0d42GNkC6c8l6qMaEUJ0E4W/HhM82Q8HizbiIGhjAYcDTyCUnkgpIXjlxfj3cRbSkuMQsWk55k4YDHPj+nBp+zG8r3Kl30QagWpECNFFFP56TYiX8W6wbCcOAqOuYxB6r0Z6okZiUJGTjJ1LxqN/x7boMT0OJYz0NER9VCNCiO6h8Nd3TAkuLP6k/tSyETp9tRbpHOmJmoIBO30bJlqPw46njTtWJVKoRoQQHaP18Be8TMaW4NPI13rbxKDkUhiCk56CJ/1SK8dUpmH1sE4wqgsXY9gsSUGpRo4EGVSmB8A98C+DW6eaZkg1EgoEULZoLdcmENJKiA4gLoUFI+mp5rZyrYY/U34d/m6/ILlIXlMgRF7COnh6Lob30mVY6usNL09PeG1PRXn95NXp4fCufc5nqeh1LyxetBrHHisZ3cSU4fLy6Vh27rXSxqc1qsnajG/N6geHtesL94TCRvYtK8BwcDvpHPKUrHaintZeI6Y8Hds93ODL8scCl+U4Wyi7dMrbBKI/tNB+6zFhXgLWiZZ1sfdSLFvqC28vT3h6bUfqu5WBcO/a53ywVPS61+JFWH3sseQfUYEpu4zl05fh3GvNbDvaC39REJ9bNAFLL5UrDWJeWSY22ZnCyKgz7EIyUCqxY1OJ9JVDYTbIGYHH05HHVqOvlJuJDT/OxrGXsg1P68ZH7r6J6NFG3Lfc5iNHHKJTwTqmNdeoCpeXWOOTZdeQd2AizNpaYP45qe1VzTaB6A+Nt9/6jFeGzE12MDUyQme7EGRIrgxUpq/EULNBcA48jvQ8NpqyNriZG/Dj7GPQRLxpLfyr09dgtFME/lU5kwzKz3igV5s26DPvN1Q0eEX4Ohmr5gbgcnFjmgoGZQlzMNIzGezGvK01EBbguKsl2tX3LZvbbcd9zZ0lIprQWmtUcRKu3c0x40QluA8SsSviEvKllkv9NoHoD0233/qNKT8Dj15t0KbPPPwmuTKQvGouAi4XN2/HV7QDnTBnJDyT2c37O9BW+ItmMN7VBrNOqjmDvD/hZ9MORl0n4XCh+B1M2XVsXLAG5181oaWovowlnzlgf0ET3qvnmJLzWGRtLD61bGSCr9ffgkbGlhGNaY014t1YBqsOttj4UMFp3ca2CUR/aLr91ms8/Olng3ZGXTHpcKH4uy767l/fuABrzr/SSDdf9eUl+MxhP5obb1oJf6b4CJwtpyFW7UNvAXJCv0EHow4Yuekh+JwM7Fz4P5x4rqAhUYmL8/8dCLuwpxpZ2fqFQWXaKgztVN+33N4GPinU4OqW1lYjIV7sHocOFvPwu4JzmY1vE4j+0HT7rd8EOaH4poMROozchId8DjJ2LsT/TjyHxtYG9zz+O9AOYc3sMlQc/kw57p/YgoCgLdi+IwxbtsXgtprDk7m/zYXlt6F40oh5Y14dhZO5EdpaTYP3Qi8cfqigFVELg9f7f0CPyUc0NKJa39Qga9NomBmJ+5bb9fNAksHtgeu61lAjIQovboGvlyemDu2KthYj4LrIC0v8E/BEqqVT3CYwKE07iKDA5fBaEYsH1Tw8T4nA1q1hCN0QiEN/V+jxTpFyTGkaDgYFYrnXCsQ+qAbveQoitm5FWOgGBB76GxV6tOCabb/1HPMKR53MYdTWCtO8F8Lr8MMm9e8rxLzG/h96YPKR0mZtGwrCn4/ssEmYGJyBKvF5CxT/sRQ/+KSgWnpSGQLc2/AlenqcVWPahji4sLAv2hqZwH5//emSZuCl+sDKygepKvpTGc4TXDuTiMRE9R9JyVkokmnEdAw/F/sm9kCbur7lNujpFIlnuj7PhqY11Ijhg1f1ACGjOqC/1x+orKkBTyC99SppE/i3sWllDAr5DxE8wgzDnb3gfzofvOJEzLE0Rle3JNn3tAp83N60EjGFfDwMHgGz4c7w8j+NfF4xEudYwrirG5L0asE1237rO86Fhejb1ggm9vtR3xOiQTyk+ljByie1WT/vlR/+NSnw7Dcc/ll88f+FL7D3h16wDUiX+TB+0VM8YzdssWpwbn5vfLryNurfrR7R3sx536F1IyW7TDygRn8GH0VPn0HioxsQPArBCLPJOFou/YokYdF1HAgS7WkHBqr9YIWexqNGLdz7ISyIw0zLdvV9y+YYF/aPTP30ieptSPUU6k3TclpFjapOYVa3TvjxkKLBTIrbBN71QKyIFx3B1FyEZx9j2Pim1o1/YMrTsMN3OSIzpI78hcXIyS3R/+483nUErohHKVODi559YGzji1TxgiNthy+WR2ZIHvnzi/D0GVsDyy1EcU4uSpr/hyRpuP3Wbwxen/fFUFMjGHWZiAOqV0YjCfAoRLTDOPkoVMSbUvLDn3sa7t3aoftXrli9Mw6X/imS2WjrMIU46NAPjgfzG3wpuYibZoavWNnq93EwbKRt/AkrzqZh65hOMGo/AhsfKH83U3gQDv0ccVDBlUKEL3ZjXOfvsMughxYzKD7vCev668AbmdhiQ4ZeHU6ICXKw37kvzK0W4oyCfhxBzn449zWH1cIzCrt61Jmm5el/jQR3AzCs4xCs+ktuKwFlbQI3+y/c5zDgZ67D5x0GY+VteX+DQWlmAsJDg+DnaAWzqbGokp5E33Cz8dd9Dhh+JtZ93gGD5ewYvcOg8KAD+jkebPKFkZjSTCSEhyLIzxFWZlMRq8kVqIX2W38xYKdtxE8rziJt6xh0MmqPERsfyHzvm0c8xqbzd7ua9csZ+eEv2lN/HLMQX3/YXnxFsrZdMdz7rPzfFjKM1N6+eEMfFnBXzQWuwt09nvCJeSr68ov2mGKmoJtRW1gvu6HydB/DKG7Bhc93YKzBh78IU4m0lUPR0egDtO05GQdyFDcxOkvwCLvtP0RHS3ckKPjZkODRbth/2BGW7glQMIla07wXel0jBuUxzjA1m464SunX3lDVJgiRv9MOHT+aj98VbPQ1pQV4yebi8aaRMJ3SCsK/njB/J+w6foT5ihb8Laa2qW26mlIUvGSD+3gTRppO0WD4a6f91ldVd/fA0ycGT0WbMPM6BlO6GaGt9TLcULUyGkWI5zvGaiv86wkqkP/3b9g5fzjMjIcj8F7DTZcBt6QQZTLnKGtwbl6vuuLLvCSDj9xoHyyOuI+39yWrugjPfm3RxsINCWUKvhwMFyWFZUr/vuDeBgzv4ogotvQrkpjiW4gOC0VoqPqPreHJeKI37TODot/+K9rb/xIrU/V5RDkfPFXrnM9TcvRUT51pWpw+14iHm79Yo8PXLPwjP9mhsk0QHTkem9YVppOiVNygSNDKwl90lHhsGrqaTkKUkgVnuCUolG1om0Sg0fDXXvutj/i50fBZHIH771YGLnr2Q9s2FnBLKFOyXTOoKi0FV/EEUmrH0AxHF8coqIg3paTCX7RHcXQWrCxGY3PDLbn6LDx6jcTGR/XPMSW4sms9tu3xwthvQ3BXojUVzVjgl+g5R87gHgl8PE9cDs+wv1ApsdB83PH/AsZGppiwt2F3ghhTcgW71m/DHq+x+DbkrsKGnHfNF4MGeuGKim8YU/kAKcdjERMTo+YjFseSbkPOlUt1UvW9MIzv1R8u0c8UHHWR902va8QUYu/4TughbzDfWyrahLr+/vYYuemxiuVXHf58TqX8z3iLQXUVV0lDXEtD0/A5qFQ6M+L+/vYjN0H+VW8ZlFzZhfXb9sBr7LcIkWxom0Rl+Kuc5ze01H6r8/lMNapUJCVTXaUyTFVPwwdH5cyI8Z8nYrlnGP6SXBng3/HHF8ZGMJ2wV3GXTdXvmN9vLLbl1U/AlOHsgi8w63jDqwQ1xMM130EY6HWlWTtQUuEv+jIu+hxfuu9FZoNbhHNu+MFuyuG3M8/PCMXKqHxw036GzcfeuCo1B9wzHugj92c99fgFSAmcgE+nReGVnJUvfLodo9t/AONhAbgj8e3gIyN0JaLyuUj72QYfe19VsPCijebwJHT/8ZBund5tYcKXpzDfugdGsW5JbaBEV+h9jWoHB/fpgDFhz2Qa+oaUtQn8zLX4vL01frkpf2t+R3n4C+6HYGTndug1I1bBgDMBsrfbwdy4J1ziXku/WE9D0wjuI2RkZ7TrNQOx8memdsGx9vP2sP7lpvx2jJ+B0JVRyOem4Webj+Et3dA2gdLwV2eea2mr/Vbn8wXZ2G5nDuOeLohTcI17QfZ22Jkbo6dLHBRMosY0AtwPGYnO7XphRmyB9IsN8FGQEogJn05DlPyVge2j2+MD42EIkFwZb/GuL4W19VJcf7MyuOcw33Io1mbKn7724PvwpO5KBtiqR+a0f82DI1jrtxkHk/5A6pXfERu2Bot8d+Fmg1M4DIeNcn7tKY2BGOL3p8wXlyk6DMfeshf0YCquYYubPWyte8HM1AQmvW2xKO5pg719BkUXAjDVtj+6du6CrmYfYfD30+Gx7nR93wYDDrsc/NpTSwOHwO9P6U9+oxoXFgzA6NAcFUcSrRfDuY2gbyxgNTdRQUNI3rfWUCPhk1B801F1cCtqE2q36dJIB3TutxiXVB5kqQj/3AhMsvwQA2wW4oS8CUS7J08jXTDIYgiWnC+TfrGehqYR5CJikiU+HGCDhfJnBkxpJBw698NiRQvOcMAu56PqoicGDvHDu+ZO1E4mB2LWtGmYpuThsjQWOVINoPLwVz7PWm+/VXx+HVGYRroMgsWQJTivoFtB+DQSLoMsMGTJeSiYRI1pBMiNmATLDwfAZuEJ6RdrVwaubXGDva01epmZwsSkN2wXxeFpg/XNFF1AwFRb9O/aGV26muGjwd9jusc6nBavjHoC/BNki56zk95e5ZOfvhyD+3nioqKLA1RfwIIBoxEqXdxGkgn/OkwVip7cxd+Z2XgudXOCN5iyeMwUFZ+VzUc1my15qk20ZxLnYoPZiYpOWzQHg7L4mehty0I2vxpstpwNp+YqfD8dj13P9bRFbS7Bc8S6DkDPcVuR1eAMDtEhraRGVYmz0L3PT0hWtQxK2gSGW4C8AhWn0OsoD38x0TS7t+CkwsGHLUvweDe2KJoZhouCvALlp55rL4s8UxQsrGzwq9mQ19w1htLwr6d0nptNdfut3c9vJMFj7N5yUvpZzWEKEWHfDeP3vqz//gvxJHQ0uk+NBZtfATZH9stRc9UXn47fhebGm/zwV0m0tx7tDAu7HcjjPcKvG6LwQmpGuNd/waiphzV/RMOUItrZAnY78sB79Cs2RL2QngBlp+ZhxPyzOvRzrhbEsHF9tS16DF6ssVs/Eg3T8xoxxZmIP3Qa/3B4uLViCAYsSFbr3gTNbxPEv29WGv6inYyTm/dC0S0GWhaDkpObsbcZM8OURsPZwg478nh49OsGREk3tI1Ud/0TpeHf/HlWSo32W6uf30hMyUls3vtQ+mnNqUyAWw9LeKbUH+ZX30PQCDOM2ZaHiuSNCJU+oybaGTw1bwTmn23e1f1qNTH8RfOc4gNbh9XYE8xCTLbs3lvtRnhqwQT4XeM0eyYlVSLFxxYOq/cgmBUDmY/mZSFo4gxEKhxd0ZrxkHPQCX16/YDwB4rOGTUWHzlXUpu9l0ne0Pca8XDzZ2u0rb2JT9ZfWD/qPwhSdyBaM9oEbkYMWAGr4DGqN7oMsofX2g3Ycka6W6/2ngkhWH+8UOn4g5bCVKYhZP3x5g0OrkyBj60DVu8JBismW8VgRiW4GYhhBWCVxyj07jII9l5rsWHLGZluAY3Ms1LK22/tf34j1P4EN2Q9jmtxZmqvRDuw60ewnbMDJ49HYHPAZmxeNhZjF/+KraxovBlj/3b6rCBMnBGpePBgIzQ5/GtVlxWhXMl2z5RewkrXVbik8UPwapQVlcuOFGU4uB3sBu9TL3Vi429Zoj3mlKX4oscw/Hy5+XuFbwifRWKBn+StOklTtYYaMShOXoHJbqsRtNQd3kceNeq65dprE1DXZxy/+7RGGsbmEyA3fjdOa2JmqstQpKyh1RgNzrNSCtrvFvt89Qhy47H7tOwvFjRHgPusr+r6+ys4RSh49WadCFD+ugiVUh9cO0Yo2M0bp+RecKfxmhX+6uA/O4VA/4QWuGY5g6IU0V5/zKOm7x3rsersXbDvaYmpkbkaG+QoKLyAX2w/x+JLqjp0iTpaVY14HHDkDwdSqeXaBEJ0GFOAX8d3g32EGvdCYIqQErIeMY80l25aD3+ifcLCs1j4SXeMWJeGcpXfIjUIS3HnyDJ8Z9kexjZ++FN2F500EtWIECKBdwN+tpOw783v+1sYhb++q8pAyFgLDJx1vOmnO4VcFD+7j7Tz0dju54rRA0zRtu5Oc8YYFijnQhykcahGhBAdQ+Gvz4T5ODFrAIyNOqB7f2tYWzfmYQWr/n3Rq0dXdGxnJL6rnPSj/X+wRf6lx4i6qEaEEB1E4a+3GLyKn4dPLS1hqaXHx9MPNuvGEYRqRAjRTRT+hBBCiIGh8CeEEEIMDIU/IYQQYmAo/AkhhBADQ+FPCCGEGBgKfyJByM7B1fhDiEjK0szFaOQRPEPK/j04mnwfxfQDdaWoHoQQbaDwJ2/V3NmK7z9zxObUAtlLJPNK8SQzDWl3nslcc1oZYXEOckvkvIFfinsx8/Hl0MX4vUhbqdY0TOlNHGJtAIvFAitwPQKPZtTfRY6P7LhArF0fiA2B/gg+qcW7fUFFPcBD6ZNMpKXdwbPGFQQ5uSWy1yvX4XoQQjSPwt9gCFFTxQGHI3pU8WQbfwjwMHgEus1MkAmamkeR8HRZir0X/kTaqe2if/si4ZmSC8swpchMCEdokB8crcwwVdH9QwX3EPhlL3j8Jv2JOoCfi/D/M0e7gUtwucFl86tvrcWEH1bjZFaxzHpqLGFNlbgenCrwZAqiuB6igiDS0wVL917An2mnsF30b9+EZ0ruF8CgNDMB4aFB8HO0gtlUBbfE1eV6EEI0isK/VWNQdOsAlrk6wdnJGdNcXOHqKnq4r0GSzHVmBfgnyBYWs09Lho0wD3vGD4bnhTdxwYB93AXWM+Kg+ACxBqUFL8HmPsamkaaYojD8/0GQrQVmn9bNsKm+6Qeb9j3hGl9cd+ONmsfxYLGSkNfEG9rUYopu4cAyVzg5i2oyzUVcD1d3rEnKl5pSQT1Eu215e8ZjsOeFtwHOsI/DxXoG4hQXBDWlBXjJ5uLxppEwnaIo/HW7HoQQzaHw16bqErwo4oiPshkuSl7Lu42ltgjxItET9tODceG5Oo25/LAR/rsH47qMw+4X73YWBNksfN1tKmLZisOmjkC/wx/CAkRONkf7Yf64efcY/Df+jheKD69VEr5IhKf9dARfeC57NC9Dfj1EBcGecV0wbveLd2dvBNlgfd0NU2PZDaeUQ0DhTwipQ+GvFVW4FxeMwO1RiGW5wsErAEHrdiKSNRlfzk2SnlgrmNcnMNcxCHfUvtm6APc3fAULd8mw4V1dgv4mToiufPecMH8n7EyGIeCuiiRUGf4PsXHEh5h9SnfDpub2KnxmbIKBs6ORr2JxlWJe48RcRwSpXRD59RAVBEv6m8BJsiDYaWeCYQF33z0nl6rw1/16EEI0g8JfC/hZYVgT9W/dkZngPgtfdXPAoVcCvDy1Bp67bktMy808jI3Hc+T21wpfJiPpeoXCez3z2Q9xfsd6HLojfT6BQUmUG2YeEZ+uVgv3AcLsB+KHfU8lxgPUnJsPi06io/wG/d7My3CMN7HB8nTpz5WiKvxRiWRPK3y2OAWlas9oy+Le34eZn3SB8acrka5ubsvBlETBbeYRFKu7nArqISoI5lt0Eh3lSxQE4eNNYLM8/d1zcqkIfz2oByFEMyj8tYDhlKG8LhcZlEZNRje7nXgu3cVeT/D8Fq7nVskNaX5WEH7eVyBncF7twd4lRB4+AZZDX8xKkj5SEyCbNRaD7aZg2rRpso+Zq5D49m4wAjxL3gSPEVYY6XcRr6U+jHfZC31FR/5HGx5o/rsL33UaCv+s5oa/CPcuwp2sYeOwAkczyqVffY8YcLIi4b/tEv69vQ5D25tjcqT8Wqijtqtk7GA7TJGuRd1jJlYl/vtmSqX1EBUEXn1FR/6SBcGu7zphqH/Wu+fkUhX+0OF6EEI0icJfq7j4fV4fDF2bKe7rZ6pQLt4rqP0Pyu4n48Tpv/FaXvJDefiLVeOMx0A54c+gOMoVbkdL5O5UyMNU3cVmu/6YuC9P4vOEz8IwxtQOOxoMEORn+WOYuTOiy1T8dZXhX4Wr/xuCLxYly4bceyUK/juRWB92WTyokSnCCZeP0N7GDzelV3UdBiWZV5Dx5rCe9xL/PC6VWPdMcRRc3Y6iRMUqe0NRPUQFQdgYU9jtyH/3PD8L/sPM4Rxd1nBKOVSFv67WgxCiaRT+GidEXsJyLNz5N/g1N7DM2gJzzohP0VambkbIefHRFO/eAQRH30Xc3G+w8pb8I+imh78oPF7FYY5TCO6pfaq69mzB1+jpcVayj1mQizC7T7Aw+d1o/+LoqRg45QgK32RdThKCAo7innSiCB4hZISS8K8bYNYDbnLm//0R4NWVzfh5c6rEKXrBPRa+7miOHw+Iu3MkCPOxa8IYhDwSd95wz/0E2yWXILHqmVeIm+OEELULoqAeoudzw+zwycLkd6P9i6MxdeAUHKkviMJ6iN77KGSE4vDXyXoQQrSBwl/j+MgIGoex3hE4smUjQgNmwmnpIUSHB8H/16tvj6h4LwvwqjQJHiO88Meb7lvBAxxb4wsfH5+6xxK3/+Cr7/8L7/r/+yzdiN/fnq6vpTj8a3dC8k8sgP3MUFx5oc5v0xSMLhfh3tsLd2cv7LuYibuXw7FgsidinzY4g3HqJ1j16IeFb38fzkVGDAsBqzwwqncXDLL3wtoNW3AmR2pkg06NLhcg77dgeLt8g37m/WAf+te7gGSKcW2HO2x7maHroB+whHUMdxvMMsM+hhlf/Yy0utXMw00/W8w8LnvKXJh/AgvsZyL0yguorojieogKgr3uzvDadxGZdy8jfMFkeMY+rf8libx6iN6SEQNWwCp4jOqNLoPs4bV2A7ackRprolP1IIRoE4W/ltSwS1BR37IKOSUokbkKmxAF+ydh1Lq/UXIrFZkNxm+90ZwjfzEBXt/YA6/pjnBymgqX2e5wdxc95q/HaYmdCPG0CsOmVvUrPEhPRertHJTISS7BgwiEX1T3qLZeKwmbmpRFGOxyHJza/4iWifXNeOzMe4wbNxr8HK+e4PUN7PGaDkcnJ0x1mS2uh/t8rD/9r/SUyushevbVg3Skpt5GjpyCGHI9CCGqUfi/N3zcYY2H85pfsTniqty+YGXhLyy6iZidIZj7dQ986R6CsKM38ErehG8JwK0oR3m56FHBlTziq3/94cYR6O6aqCBslKlCxt5w/FEhZyGUqbui3EeYc6bxn6g7+MhcNxwWY5YhMjEekbtYmDvGAb+EheCwip/1CbgV4nqUV4ArUxCqByFEeyj83yseKiuq5YZ7LWXhrw01GRsxxsYR29OLGnUxIuGrVJy6VqT24MI6wko8OrkItp/OQ2JhSy2hFgifY+f4sdj0uAbcyirxThVTjUqOTJo3GtWDEKItFP46TFhwDievKf6dvzYI2Y/xx5Fd2BGfqb27yIkC8/KB3YiIv4FnHG19SAupOonZtsvxZ2PSuRGoHoQQbaDwJ6Q5GC7KyrgtuoNGCCHNReFPCCGEGBgKf0IIIcTAUPgTQgghBobCnxBCCDEwFP6EEEKIgaHwJ4QQQgwMhT8hhBBiYCj8CSGEEAND4U8IIYQYGAp/QgghxMBQ+BNCCCEGhsKfEEIIMTAU/oQQQoiBofAnhBBCDAyFPyGEEGJgKPwJIYQQA/P/bQTFtLXrlF8AAAAASUVORK5CYII=>
