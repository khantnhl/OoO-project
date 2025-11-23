#ifndef PROCSIM_HPP
#define PROCSIM_HPP

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>
#include <deque>
#include <queue>

#define DEFAULT_K0 1
#define DEFAULT_K1 2
#define DEFAULT_K2 3
#define DEFAULT_R 8
#define DEFAULT_F 4

typedef struct _proc_inst_t
{
    uint32_t instruction_address;
    int32_t op_code;
    int32_t src_reg[2];
    int32_t dest_reg;
} proc_inst_t;

typedef struct _instr
{
    long pc;
    int fu;
    int dest;
    int source1;
    int source2;
} instr;

typedef struct dis_instr
{
    uint64_t tag;
    instr instruction;
} dis_instr;

typedef struct n_retire
{
    uint64_t cycle;
    uint64_t tag;
    instr instruction;
    
    bool operator>(const n_retire& other) const {
        if (cycle != other.cycle) return cycle > other.cycle;
        return tag > other.tag;
    }
} n_retire;

typedef struct _proc_stats_t
{
    float avg_inst_retired;
    float avg_inst_fired;
    float avg_disp_size;
    unsigned long max_disp_size;
    unsigned long retired_instruction;
    unsigned long cycle_count;
} proc_stats_t;

class Node {
    public:
        Node* next;
        instr instruction;
        uint64_t tag;
        uint64_t schedule_cycle;  // Cycle when entered schedule queue

        Node(Node* next = nullptr, instr instruction = {-1, -1, -1, -1, -1}, uint64_t tag = 1, uint64_t sc = 0)
            : next(next), instruction(instruction), tag(tag), schedule_cycle(sc) {}
};

extern int32_t gfu[3];
extern int32_t g_reg[128];
extern std::deque<instr> q;
extern std::deque<dis_instr> d_q;
extern int64_t g_r;
extern int64_t g_k0;
extern int64_t g_k1;
extern int64_t g_k2;
extern int64_t g_f;
extern int64_t g_ret;
extern int64_t g_rs;
extern uint64_t g_tag;
extern uint64_t g_cnt;
extern uint64_t g_cycle;
extern std::priority_queue<n_retire, std::vector<n_retire>, std::greater<n_retire>> r_q;
extern std::vector<instr> instructions;
extern size_t f_tracker;
extern Node* head;

bool read_instruction(proc_inst_t* p_inst);
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f);
void run_proc(proc_stats_t* p_stats);
void complete_proc(proc_stats_t* p_stats);

#endif /* PROCSIM_HPP */
