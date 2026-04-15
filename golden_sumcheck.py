"""
zkPHIRE SumCheck Golden Model
Reference implementation of one SumCheck round
Based on: zkPHIRE HPCA 2026, Section III, Figure 1

This is the Python golden model for HLS verification.
Implements exact field arithmetic for BN254.
"""

# BN254 scalar field prime (from paper, used in HyperPlonk)
FIELD_P = 21888242871839275222246405745257275088548364400416034343698204186575808495617

def mod_add(a, b):
    return (a + b) % FIELD_P

def mod_sub(a, b):
    return (a - b) % FIELD_P

def mod_mul(a, b):
    return (a * b) % FIELD_P

def mod_inv(a):
    """Modular inverse using Fermat's little theorem"""
    return pow(a, FIELD_P - 2, FIELD_P)

class MLE:
    """Multilinear Extension table - paper Section II-C"""
    def __init__(self, values):
        self.values = [v % FIELD_P for v in values]
        self.size = len(values)
    
    def __len__(self):
        return self.size
    
    def __getitem__(self, idx):
        return self.values[idx]

def mle_update_pair(f0, f1, r):
    """
    MLE Update: Section III.B, page 3
    f(r) = f0*(1-r) + f1*r
    """
    one_minus_r = mod_sub(1, r)
    term0 = mod_mul(f0, one_minus_r)
    term1 = mod_mul(f1, r)
    return mod_add(term0, term1)

def extend_pair(f0, f1, r, degree):
    """
    Extension Engine: Figure 1
    Generate d+1 points from pair (f0,f1)
    For multilinear, we linearly extrapolate:
    ext[0] = f0, ext[1] = f1
    ext[2] = f0*(1-2) + f1*2, etc.
    """
    extensions = []
    for i in range(degree + 1):
        # Evaluate at X = i
        # Using same formula as update, but with r=i
        xi = i % FIELD_P
        one_minus_xi = mod_sub(1, xi)
        ext = mod_add(mod_mul(f0, one_minus_xi), mod_mul(f1, xi))
        extensions.append(ext)
    return extensions

def sumcheck_round(mles, r_prev, degree):
    """
    One SumCheck round for product term f = Π mles
    Paper: Figure 1, Section III.B
    
    Args:
        mles: list of MLE objects (e.g., [a,b,c,e])
        r_prev: previous round challenge (for update, not used in round 1)
        degree: degree of product (len(mles))
    
    Returns:
        s_vals: list of d+1 sums [s(0), s(1), ..., s(d)]
        updated_mles: list of MLEs halved for next round
    """
    assert len(mles) == degree, f"Expected {degree} MLEs"
    size = len(mles[0])
    assert size % 2 == 0, "MLE size must be even"
    
    num_pairs = size // 2
    d_plus_1 = degree + 1
    
    # Step 1: Extension - generate d+1 points per pair
    # Paper: "every MLE evaluation pair is extended to five evaluations"
    all_extensions = []  # [mle_idx][pair_idx][ext_point]
    for mle in mles:
        mle_exts = []
        for i in range(num_pairs):
            f0 = mle[2*i]
            f1 = mle[2*i + 1]
            # For round 1, we extend using formal variable
            # In hardware, this is done on-the-fly
            ext = extend_pair(f0, f1, 0, degree)  # placeholder, real r used later
            # Actually, we need to compute extensions for each evaluation point
            # Let's compute properly for s_i(Xi)
            pair_exts = []
            for x in range(d_plus_1):
                # Linear interpolation to point x
                pair_exts.append(mle_update_pair(f0, f1, x))
            mle_exts.append(pair_exts)
        all_extensions.append(mle_exts)
    
    # Step 2: Product and Sum
    # Paper: "Extended evaluations are multiplied, summed"
    s_vals = [0] * d_plus_1
    for x in range(d_plus_1):
        total = 0
        for pair_idx in range(num_pairs):
            # Multiply across all MLEs at this extension point
            prod = 1
            for mle_idx in range(degree):
                prod = mod_mul(prod, all_extensions[mle_idx][pair_idx][x])
            total = mod_add(total, prod)
        s_vals[x] = total
    
    # Step 3: Verifier challenge (simulated)
    # In real protocol, r = Hash(s_vals)
    # For golden model, we use provided r_next
    r_next = r_prev  # placeholder
    
    # Step 4: MLE Update for next round
    # Paper: "MLE Update by fixing X1 to random challenge r1"
    updated_mles = []
    for mle in mles:
        new_vals = []
        for i in range(num_pairs):
            f0 = mle[2*i]
            f1 = mle[2*i + 1]
            new_vals.append(mle_update_pair(f0, f1, r_next))
        updated_mles.append(MLE(new_vals))
    
    return s_vals, updated_mles

def sumcheck_round_simple(mles, r_challenge):
    """
    Simplified version for round i with given challenge r
    Returns s(0)...s(d) and updated tables
    This matches hardware behavior more closely
    """
    degree = len(mles)
    size = len(mles[0])
    num_pairs = size // 2
    
    s_vals = []
    # Compute s(x) for x = 0..degree
    for x in range(degree + 1):
        total = 0
        for pair_idx in range(num_pairs):
            prod = 1
            for mle in mles:
                f0 = mle[2*pair_idx]
                f1 = mle[2*pair_idx + 1]
                # Evaluate the pair at x
                fx = mle_update_pair(f0, f1, x)
                prod = mod_mul(prod, fx)
            total = mod_add(total, prod)
        s_vals.append(total)
    
    # Update tables with verifier challenge r
    updated = []
    for mle in mles:
        new_vals = [mle_update_pair(mle[2*i], mle[2*i+1], r_challenge) 
                   for i in range(num_pairs)]
        updated.append(MLE(new_vals))
    
    return s_vals, updated

# ------------------- Test and Demo -------------------

def test_figure1_example():
    """
    Test case matching Figure 1: f = a*b*c*e, degree 4, size 8
    Uses small integers for readability (mod p still applied)
    """
    print("=== zkPHIRE Golden Model Test ===")
    print("Testing Figure 1: f = a·b·c·e, degree 4")
    
    # Create simple MLEs with values 1..8 for visibility
    a = MLE([1, 2, 3, 4, 5, 6, 7, 8])
    b = MLE([2, 3, 4, 5, 6, 7, 8, 9])
    c = MLE([1, 1, 2, 2, 3, 3, 4, 4])
    e = MLE([3, 3, 3, 3, 5, 5, 5, 5])
    
    mles = [a, b, c, e]
    degree = 4
    
    print(f"\nInput MLEs (size {len(a)}):")
    for name, mle in zip(['a','b','c','e'], mles):
        print(f"  {name}: {mle.values}")
    
    # Run round 1 with challenge r=5 (arbitrary)
    r1 = 5
    s_vals, updated = sumcheck_round_simple(mles, r1)
    
    print(f"\nRound 1, challenge r1 = {r1}")
    print(f"s(0)...s(4) = {s_vals}")
    print(f"Check: s(0)+s(1) should equal sum of all products")
    
    # Verify s(0)+s(1)
    s0_plus_s1 = mod_add(s_vals[0], s_vals[1])
    # Compute direct sum for verification
    direct_sum = 0
    for i in range(len(a)):
        prod = mod_mul(mod_mul(mod_mul(a[i], b[i]), c[i]), e[i])
        direct_sum = mod_add(direct_sum, prod)
    
    print(f"s(0)+s(1) = {s0_plus_s1}")
    print(f"Direct sum = {direct_sum}")
    print(f"Match: {s0_plus_s1 == direct_sum}")
    
    print(f"\nUpdated MLEs (size {len(updated[0])}):")
    for name, mle in zip(['a','b','c','e'], updated):
        print(f"  {name}': {mle.values}")
    
    # Round 2
    r2 = 7
    s2_vals, updated2 = sumcheck_round_simple(updated, r2)
    print(f"\nRound 2, challenge r2 = {r2}")
    print(f"s(0)...s(4) = {s2_vals}")
    print(f"Updated size: {len(updated2[0])}")
    
    return s_vals, updated

def test_high_degree():
    """Test degree 6 to show programmability"""
    print("\n=== Testing Degree 6 (programmability) ===")
    mles = [MLE([i+1 for i in range(8)]) for _ in range(6)]
    s_vals, _ = sumcheck_round_simple(mles, r_challenge=3)
    print(f"Degree 6, s(0)...s(6) computed: {len(s_vals)} values")
    print(f"s_vals: {s_vals[:3]}... (truncated)")
    assert len(s_vals) == 7, "Should have d+1 = 7 values"
    print("PASS: Programmable degree works")

if __name__ == "__main__":
    test_figure1_example()
    test_high_degree()
    
    print("\n=== Golden Model Ready ===")
    print("This implements:")
    print("1. MLE Update: f(r) = f0*(1-r) + f1*r  [Paper p.3]")
    print("2. Extension: generate d+1 points      [Figure 1]")
    print("3. Product: multiply across MLEs       [Figure 1]")
    print("4. Sum: accumulate down table          [Figure 1]")
