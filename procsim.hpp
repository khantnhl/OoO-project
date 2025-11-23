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

typedef struct schedule_entry
{
    uint64_t tag;
    instr instruction;
    uint64_t fetch_cycle;
    uint64_t dispatch_cycle;
    uint64_t schedule_cycle;
    uint64_t execute_cycle;
    uint64_t state_cycle;
    bool fired;  // Has been issued to execute
} schedule_entry;

typedef struct _proc_stats_t
{
    float avg_inst_retired;
    float avg_inst_fired;
    float avg_disp_size;
    unsigned long max_disp_size;
    unsigned long retired_instruction;
    unsigned long cycle_count;
} proc_stats_t;

extern int32_t gfu[3];  // Number of FUs of each type currently busy
extern int32_t g_reg[128];  // Register ready bits (0=ready, 1=busy)
extern std::deque<instr> q;  // Fetch queue
extern std::deque<dis_instr> d_q;  // Dispatch queue
extern std::vector<schedule_entry> sched_q;  // Scheduling queue (reservation station)
extern int64_t g_r;  // Number of result buses
extern int64_t g_k0;  // Number of k0 FUs
extern int64_t g_k1;  // Number of k1 FUs
extern int64_t g_k2;  // Number of k2 FUs
extern int64_t g_f;  // Fetch rate
extern int64_t g_ret;  // Number of retired instructions
extern int64_t g_rs;  // Reservation station size
extern uint64_t g_tag;  // Instruction tag counter
extern uint64_t g_cycle;  // Current cycle
extern std::priority_queue<n_retire, std::vector<n_retire>, std::greater<n_retire>> r_q;  // Retire queue
extern std::vector<instr> instructions;  // All instructions from trace
extern size_t f_tracker;  // Fetch tracker
extern uint64_t total_disp_size;  // For tracking average dispatch queue size
extern uint64_t max_disp_size;  // Maximum dispatch queue size
extern uint64_t total_fired;  // Total instructions fired

bool read_instruction(proc_inst_t* p_inst);
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f);
void run_proc(proc_stats_t* p_stats);
void complete_proc(proc_stats_t* p_stats);

// Helper functions
void log_event(const char* operation, uint64_t tag);
void print_outputs();

#endif /* PROCSIM_HPP */
