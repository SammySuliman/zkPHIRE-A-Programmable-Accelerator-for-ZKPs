#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "include/types.hpp"
#include "include/field_arithmetic.hpp"

status_t sumcheck_round_array(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    field_elem_t r, int degree, int size,
    field_elem_t samples[MAX_SAMPLES],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
);

static field_elem_t compute_claim(
    field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int degree, int size
) {
    field_elem_t claim = field_elem_t(0);
    for (int i = 0; i < size; ++i) {
        ap_uint<512> prod = ap_uint<512>(1);
        for (int m = 0; m < degree; ++m) {
            prod = prod * ap_uint<512>(tables[m][i]);
        }
        ap_uint<512> sum = ap_uint<512>(claim) + prod;
        claim = field_elem_t(sum % field_elem_t(FIELD_P));
    }
    return claim;
}

static bool check_round(const char* name,
    field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int degree, int size, field_elem_t r,
    field_elem_t exp_samples[MAX_SAMPLES],
    field_elem_t exp_updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
) {
    field_elem_t samples[MAX_SAMPLES] = {};
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2] = {};
    sumcheck_round_array(tables, r, degree, size, samples, updated);

    bool ok = true;
    for (int x = 0; x <= degree; ++x) {
        if (samples[x] != exp_samples[x]) {
            printf("FAIL %s: sample[%d] mismatch\n", name, x);
            ok = false;
        }
    }
    for (int m = 0; m < degree; ++m) {
        for (int k = 0; k < size / 2; ++k) {
            if (updated[m][k] != exp_updated[m][k]) {
                printf("FAIL %s: updated[%d][%d] mismatch\n", name, m, k);
                ok = false; break;
            }
        }
        if (!ok) break;
    }
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    return ok;
}

int main() {
    int passed = 0, total = 0;

    printf("=== zkPHIRE RFSoC C-Simulation Testbench ===\n\n");

    // Case A: x1*x2*x3 r=5
    {
        field_elem_t tbl[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x1[8] = {0,0,0,0,1,1,1,1};
        field_elem_t x2[8] = {0,0,1,1,0,0,1,1};
        field_elem_t x3[8] = {0,1,0,1,0,1,0,1};
        for (int i=0; i<8; ++i) { tbl[0][i]=x1[i]; tbl[1][i]=x2[i]; tbl[2][i]=x3[i]; }
        field_elem_t es[MAX_SAMPLES] = {0,1,2,3};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2] = {};
        eu[0][0]=0;eu[0][1]=0;eu[0][2]=1;eu[0][3]=1;
        eu[1][0]=0;eu[1][1]=1;eu[1][2]=0;eu[1][3]=1;
        eu[2][0]=5;eu[2][1]=5;eu[2][2]=5;eu[2][3]=5;
        total++; if (check_round("Case A: x1*x2*x3 r=5",tbl,3,8,field_elem_t(5),es,eu)) passed++;
    }

    // Case A: r=0
    {
        field_elem_t tbl[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x1[8] = {0,0,0,0,1,1,1,1};
        field_elem_t x2[8] = {0,0,1,1,0,0,1,1};
        field_elem_t x3[8] = {0,1,0,1,0,1,0,1};
        for (int i=0; i<8; ++i) { tbl[0][i]=x1[i]; tbl[1][i]=x2[i]; tbl[2][i]=x3[i]; }
        field_elem_t es[MAX_SAMPLES] = {0,1,2,3};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2] = {};
        for (int i=0;i<4;++i){eu[0][i]=tbl[0][2*i];eu[1][i]=tbl[1][2*i];eu[2][i]=tbl[2][2*i];}
        total++; if (check_round("Case A: x1*x2*x3 r=0",tbl,3,8,field_elem_t(0),es,eu)) passed++;
    }

    // Case A: r=1
    {
        field_elem_t tbl[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x1[8] = {0,0,0,0,1,1,1,1};
        field_elem_t x2[8] = {0,0,1,1,0,0,1,1};
        field_elem_t x3[8] = {0,1,0,1,0,1,0,1};
        for (int i=0; i<8; ++i) { tbl[0][i]=x1[i]; tbl[1][i]=x2[i]; tbl[2][i]=x3[i]; }
        field_elem_t es[MAX_SAMPLES] = {0,1,2,3};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2] = {};
        for (int i=0;i<4;++i){eu[0][i]=tbl[0][2*i+1];eu[1][i]=tbl[1][2*i+1];eu[2][i]=tbl[2][2*i+1];}
        total++; if (check_round("Case A: x1*x2*x3 r=1",tbl,3,8,field_elem_t(1),es,eu)) passed++;
    }

    // Case B: x1*x2
    {
        field_elem_t tbl[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x1[8] = {0,0,0,0,1,1,1,1};
        field_elem_t x2[8] = {0,0,1,1,0,0,1,1};
        for (int i=0; i<8; ++i) { tbl[0][i]=x1[i]; tbl[1][i]=x2[i]; }
        field_elem_t es[MAX_SAMPLES] = {1,1,1};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2] = {};
        eu[0][0]=0;eu[0][1]=0;eu[0][2]=1;eu[0][3]=1;
        eu[1][0]=0;eu[1][1]=1;eu[1][2]=0;eu[1][3]=1;
        total++; if (check_round("Case B: x1*x2 r=5",tbl,2,8,field_elem_t(5),es,eu)) passed++;
    }

    // Case B: x2*x3
    {
        field_elem_t tbl[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x2[8] = {0,0,1,1,0,0,1,1};
        field_elem_t x3[8] = {0,1,0,1,0,1,0,1};
        for (int i=0; i<8; ++i) { tbl[0][i]=x2[i]; tbl[1][i]=x3[i]; }
        field_elem_t es[MAX_SAMPLES] = {0,2,4};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2] = {};
        eu[0][0]=0;eu[0][1]=1;eu[0][2]=0;eu[0][3]=1;
        eu[1][0]=5;eu[1][1]=5;eu[1][2]=5;eu[1][3]=5;
        total++; if (check_round("Case B: x2*x3 r=5",tbl,2,8,field_elem_t(5),es,eu)) passed++;
    }

    printf("\n=== Results: %d/%d tests passed ===\n", passed, total);
    return (passed == total) ? 0 : 1;
}
