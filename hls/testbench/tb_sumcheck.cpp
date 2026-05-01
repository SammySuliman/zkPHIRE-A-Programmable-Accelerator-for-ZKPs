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
            printf("FAIL %s: sample[%d]\n", name, x); ok = false;
        }
    }
    for (int m = 0; m < degree && ok; ++m)
        for (int k = 0; k < size / 2 && ok; ++k)
            if (updated[m][k] != exp_updated[m][k]) {
                printf("FAIL %s: updated[%d][%d]\n", name, m, k); ok = false;
            }
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    return ok;
}

static bool check_chain(const char* name,
    field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int degree, int size,
    field_elem_t challenges[3], int rounds,
    field_elem_t final_expected
) {
    field_elem_t cur[MAX_DEGREE][MAX_TABLE_SIZE];
    int cs = size;
    for (int m = 0; m < degree; ++m)
        for (int i = 0; i < size; ++i) cur[m][i] = tables[m][i];
    bool ok = true;
    for (int rnd = 0; rnd < rounds; ++rnd) {
        field_elem_t s[MAX_SAMPLES] = {};
        field_elem_t u[MAX_DEGREE][MAX_TABLE_SIZE/2] = {};
        sumcheck_round_array(cur, challenges[rnd], degree, cs, s, u);
        int ns = cs / 2;
        for (int m = 0; m < degree; ++m)
            for (int i = 0; i < ns; ++i) cur[m][i] = u[m][i];
        cs = ns;
    }
    field_elem_t final = field_elem_t(1);
    for (int m = 0; m < degree; ++m) {
        ap_uint<512> p = ap_uint<512>(final) * ap_uint<512>(cur[m][0]);
        final = field_elem_t(p % field_elem_t(FIELD_P));
    }
    if (final != final_expected) {
        printf("FAIL %s: final=%llu expected=%llu\n", name,
               (unsigned long long)final.to_uint64(),
               (unsigned long long)final_expected.to_uint64());
        ok = false;
    }
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    return ok;
}

int main() {
    int p = 0, t = 0;
    printf("=== zkPHIRE RFSoC C-Simulation ===\n\n");

    // Case A: x1*x2*x3 r=5
    {
        field_elem_t tb[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x1[8]={0,0,0,0,1,1,1,1},x2[8]={0,0,1,1,0,0,1,1},x3[8]={0,1,0,1,0,1,0,1};
        for(int i=0;i<8;++i){tb[0][i]=x1[i];tb[1][i]=x2[i];tb[2][i]=x3[i];}
        field_elem_t es[MAX_SAMPLES]={0,1,2,3};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2]={};
        eu[0][0]=0;eu[0][1]=0;eu[0][2]=1;eu[0][3]=1;
        eu[1][0]=0;eu[1][1]=1;eu[1][2]=0;eu[1][3]=1;
        eu[2][0]=5;eu[2][1]=5;eu[2][2]=5;eu[2][3]=5;
        t++; if(check_round("Case-A r=5",tb,3,8,field_elem_t(5),es,eu)) p++;
    }
    // Case A: r=0
    {
        field_elem_t tb[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x1[8]={0,0,0,0,1,1,1,1},x2[8]={0,0,1,1,0,0,1,1},x3[8]={0,1,0,1,0,1,0,1};
        for(int i=0;i<8;++i){tb[0][i]=x1[i];tb[1][i]=x2[i];tb[2][i]=x3[i];}
        field_elem_t es[MAX_SAMPLES]={0,1,2,3};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2]={};
        for(int i=0;i<4;++i){eu[0][i]=tb[0][2*i];eu[1][i]=tb[1][2*i];eu[2][i]=tb[2][2*i];}
        t++; if(check_round("Case-A r=0",tb,3,8,field_elem_t(0),es,eu)) p++;
    }
    // Case A: r=1
    {
        field_elem_t tb[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x1[8]={0,0,0,0,1,1,1,1},x2[8]={0,0,1,1,0,0,1,1},x3[8]={0,1,0,1,0,1,0,1};
        for(int i=0;i<8;++i){tb[0][i]=x1[i];tb[1][i]=x2[i];tb[2][i]=x3[i];}
        field_elem_t es[MAX_SAMPLES]={0,1,2,3};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2]={};
        for(int i=0;i<4;++i){eu[0][i]=tb[0][2*i+1];eu[1][i]=tb[1][2*i+1];eu[2][i]=tb[2][2*i+1];}
        t++; if(check_round("Case-A r=1",tb,3,8,field_elem_t(1),es,eu)) p++;
    }
    // Case A chain: (5,7,11)
    {
        field_elem_t tb[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x1[8]={0,0,0,0,1,1,1,1},x2[8]={0,0,1,1,0,0,1,1},x3[8]={0,1,0,1,0,1,0,1};
        for(int i=0;i<8;++i){tb[0][i]=x1[i];tb[1][i]=x2[i];tb[2][i]=x3[i];}
        field_elem_t ch[3]={field_elem_t(5),field_elem_t(7),field_elem_t(11)};
        t++; if(check_chain("Case-A chain",tb,3,8,ch,3,field_elem_t(385))) p++;
    }
    // Case B: x1*x2
    {
        field_elem_t tb[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x1[8]={0,0,0,0,1,1,1,1},x2[8]={0,0,1,1,0,0,1,1};
        for(int i=0;i<8;++i){tb[0][i]=x1[i];tb[1][i]=x2[i];}
        field_elem_t es[MAX_SAMPLES]={1,1,1};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2]={};
        eu[0][0]=0;eu[0][1]=0;eu[0][2]=1;eu[0][3]=1;
        eu[1][0]=0;eu[1][1]=1;eu[1][2]=0;eu[1][3]=1;
        t++; if(check_round("Case-B x1*x2",tb,2,8,field_elem_t(5),es,eu)) p++;
    }
    // Case B: x2*x3
    {
        field_elem_t tb[MAX_DEGREE][MAX_TABLE_SIZE] = {};
        field_elem_t x2[8]={0,0,1,1,0,0,1,1},x3[8]={0,1,0,1,0,1,0,1};
        for(int i=0;i<8;++i){tb[0][i]=x2[i];tb[1][i]=x3[i];}
        field_elem_t es[MAX_SAMPLES]={0,2,4};
        field_elem_t eu[MAX_DEGREE][MAX_TABLE_SIZE/2]={};
        eu[0][0]=0;eu[0][1]=1;eu[0][2]=0;eu[0][3]=1;
        eu[1][0]=5;eu[1][1]=5;eu[1][2]=5;eu[1][3]=5;
        t++; if(check_round("Case-B x2*x3",tb,2,8,field_elem_t(5),es,eu)) p++;
    }
    // Case B combined: x1*x2 + x2*x3
    {
        field_elem_t t1[MAX_DEGREE][MAX_TABLE_SIZE]={},t2[MAX_DEGREE][MAX_TABLE_SIZE]={};
        field_elem_t x1[8]={0,0,0,0,1,1,1,1},x2[8]={0,0,1,1,0,0,1,1},x3[8]={0,1,0,1,0,1,0,1};
        for(int i=0;i<8;++i){t1[0][i]=x1[i];t1[1][i]=x2[i];t2[0][i]=x2[i];t2[1][i]=x3[i];}
        field_elem_t s1[MAX_SAMPLES]={},s2[MAX_SAMPLES]={},u1[MAX_DEGREE][MAX_TABLE_SIZE/2]={},u2[MAX_DEGREE][MAX_TABLE_SIZE/2]={};
        sumcheck_round_array(t1,field_elem_t(5),2,8,s1,u1);
        sumcheck_round_array(t2,field_elem_t(5),2,8,s2,u2);
        field_elem_t c[3]; for(int x=0;x<3;++x) c[x]=mod_add(s1[x],s2[x]);
        field_elem_t exp[3]={field_elem_t(1),field_elem_t(3),field_elem_t(5)};
        bool ok=true; for(int x=0;x<3;++x) if(c[x]!=exp[x]) ok=false;
        t++; if(ok) p++;
        printf("Case-B combined %s\n", ok?"PASS":"FAIL");
    }

    printf("\n=== Results: %d/%d tests passed ===\n", p, t);
    return (p == t) ? 0 : 1;
}
