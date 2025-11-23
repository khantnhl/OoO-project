#include "procsim.hpp"
#include <algorithm>
#include <map>

// Global variable DEFINITIONS
int32_t gfu[3] = {0};
int32_t g_reg[128] = {0};
std::deque<instr> q;
std::deque<dis_instr> d_q;
std::vector<schedule_entry> sched_q;
int64_t g_r = 0;
int64_t g_k0 = 0;
int64_t g_k1 = 0;
int64_t g_k2 = 0;
int64_t g_f = 0;
int64_t g_ret = 0;
int64_t g_rs = 0;
uint64_t g_tag = 0;
uint64_t g_cycle = 0;
std::priority_queue<n_retire, std::vector<n_retire>, std::greater<n_retire>> r_q;
std::vector<instr> instructions;
size_t f_tracker = 0;
uint64_t total_disp_size = 0;
uint64_t max_disp_size = 0;
uint64_t total_fired = 0;

// For output generation
std::map<uint64_t, schedule_entry> completed_instructions;
std::vector<std::pair<uint64_t, std::pair<std::string, uint64_t>>> log_entries; // (cycle, (operation, tag))
std::map<uint64_t, uint64_t> fetch_cycles;  // tag -> fetch cycle
std::map<uint64_t, uint64_t> dispatch_cycles;  // tag -> dispatch cycle

/**
 * Subroutine for initializing the processor.
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f)
{
    instr n;

    g_r = r;
    g_k0 = k0;
    g_k1 = k1;
    g_k2 = k2;
    g_f = f;

    g_tag = 0;
    f_tracker = 0;
    g_ret = 0;
    g_cycle = 0;
    total_disp_size = 0;
    max_disp_size = 0;
    total_fired = 0;

    g_rs = 2 * (k0 + k1 + k2);

    // Initialize all registers as ready (0 = ready)
    for(int32_t i = 0; i < 128; ++i)
    {
        g_reg[i] = 0;
    }

    // Initialize all FUs as free
    for(int32_t i = 0; i < 3; ++i)
    {
        gfu[i] = 0;
    }

    // Read instructions into vector
    while(std::cin >> std::hex >> n.pc >> std::dec >> n.fu >> n.dest >> n.source1 >> n.source2)
    {
        if(n.fu == -1) n.fu = 1;  // Type -1 uses FU type 1
        instructions.push_back(n);
    }
}

/**
 * Helper function to log events for .log file
 */
void log_event(const char* operation, uint64_t tag)
{
    log_entries.push_back({g_cycle, {std::string(operation), tag}});
}

/**
 * Fetch stage: Fetch N instructions per cycle
 */
void fetch()
{
    for(uint64_t i = 0; i < (uint64_t)g_f && f_tracker < instructions.size(); ++i)
    {
        fetch_cycles[f_tracker + 1] = g_cycle;  // Store fetch cycle for this instruction (1-indexed)
        q.push_back(instructions[f_tracker]);
        f_tracker++;
        log_event("FETCHED", f_tracker);  // Log with instruction number (1-indexed)
    }
}

/**
 * Dispatch stage: Move fetched instructions to dispatch queue with tags
 */
void dispatch()
{
    while(!q.empty())
    {
        dis_instr dis;
        dis.tag = ++g_tag;
        dis.instruction = q.front();
        dispatch_cycles[dis.tag] = g_cycle;  // Store dispatch cycle
        d_q.push_back(dis);
        q.pop_front();
        log_event("DISPATCHED", dis.tag);
    }

    // Track dispatch queue size for statistics
    total_disp_size += d_q.size();
    if(d_q.size() > max_disp_size)
    {
        max_disp_size = d_q.size();
    }
}

/**
 * Schedule stage: Move instructions from dispatch queue to reservation station
 * Only move if there's space in the RS
 */
void schedule()
{
    // Move from dispatch queue to scheduling queue if space available
    while(!d_q.empty() && sched_q.size() < (size_t)g_rs)
    {
        dis_instr c_inst = d_q.front();
        schedule_entry entry;
        entry.tag = c_inst.tag;
        entry.instruction = c_inst.instruction;
        entry.fetch_cycle = fetch_cycles[c_inst.tag];
        entry.dispatch_cycle = dispatch_cycles[c_inst.tag];
        entry.schedule_cycle = g_cycle;
        entry.execute_cycle = 0;
        entry.state_cycle = 0;
        entry.fired = false;

        // NOTE: Don't mark destination register as busy here
        // It will be marked busy when the instruction fires (in execute())
        // This avoids issues with self-dependencies (src == dest)

        sched_q.push_back(entry);
        d_q.pop_front();
        log_event("SCHEDULED", c_inst.tag);
    }
}

/**
 * Get the latency for a given FU type
 * All FU types have latency 1 based on the assignment
 */
int get_latency(int fu_type)
{
    (void)fu_type;  // Unused parameter
    return 1;  // All FU types have latency 1
}

/**
 * Get the maximum number of FUs for a given type
 */
int get_max_fu(int fu_type)
{
    switch(fu_type)
    {
        case 0: return g_k0;
        case 1: return g_k1;
        case 2: return g_k2;
        default: return 0;
    }
}

/**
 * Execute stage: Fire ready instructions
 * Check data dependencies and FU availability
 */
void execute()
{
    // Fire ready instructions (in tag order, oldest first)
    for(size_t i = 0; i < sched_q.size(); ++i)
    {
        schedule_entry& entry = sched_q[i];

        // Skip if already fired
        if(entry.fired)
            continue;

        // Check if source registers are ready
        bool src1_ready = (entry.instruction.source1 == -1) || (g_reg[entry.instruction.source1] == 0);
        bool src2_ready = (entry.instruction.source2 == -1) || (g_reg[entry.instruction.source2] == 0);

        if(!src1_ready || !src2_ready)
            continue;

        // Check if FU is available
        int fu_type = entry.instruction.fu;
        int max_fu = get_max_fu(fu_type);

        if(gfu[fu_type] >= max_fu)
            continue;

        // Fire the instruction
        gfu[fu_type]++;
        entry.fired = true;
        entry.execute_cycle = g_cycle;
        total_fired++;

        // Mark destination register as busy now that instruction is firing
        if(entry.instruction.dest != -1)
        {
            g_reg[entry.instruction.dest] = 1;
        }

        // Calculate completion cycle based on latency
        int latency = get_latency(fu_type);
        n_retire ret_entry;
        ret_entry.cycle = g_cycle + latency;
        ret_entry.tag = entry.tag;
        ret_entry.instruction = entry.instruction;

        r_q.push(ret_entry);
        log_event("EXECUTED", entry.tag);
    }
}

/**
 * State update/Retire stage: Update register file and remove from RS
 * Limited by number of result buses (R)
 */
void retire()
{
    int retired_this_cycle = 0;

    // Process up to R instructions that completed this cycle or earlier
    while(!r_q.empty() && retired_this_cycle < g_r)
    {
        n_retire ret_entry = r_q.top();

        // Can only retire if instruction has completed
        if(ret_entry.cycle > g_cycle)
            break;

        r_q.pop();

        // Free the destination register (if it exists)
        if(ret_entry.instruction.dest != -1)
        {
            g_reg[ret_entry.instruction.dest] = 0;
        }

        // Free the FU
        gfu[ret_entry.instruction.fu]--;

        // Remove from scheduling queue
        for(size_t i = 0; i < sched_q.size(); ++i)
        {
            if(sched_q[i].tag == ret_entry.tag)
            {
                sched_q[i].state_cycle = g_cycle;
                completed_instructions[ret_entry.tag] = sched_q[i];
                sched_q.erase(sched_q.begin() + i);
                break;
            }
        }

        g_ret++;
        retired_this_cycle++;
        log_event("STATE UPDATE", ret_entry.tag);
    }
}

/**
 * Main simulation loop
 */
void run_proc(proc_stats_t* p_stats)
{
    // Cycle 0 is the initialization, start at cycle 1
    g_cycle = 1;

    // Run until all instructions are fetched, processed, and retired
    while(f_tracker < instructions.size() || !q.empty() || !d_q.empty() ||
          !sched_q.empty() || !r_q.empty())
    {
        // Execute stages in reverse pipeline order for proper timing
        retire();
        execute();
        schedule();
        dispatch();
        fetch();

        g_cycle++;
    }

    // Adjust cycle count (we incremented one extra time)
    g_cycle--;

    p_stats->cycle_count = g_cycle;
    p_stats->retired_instruction = g_ret;
}

/**
 * Print the output in the required format
 */
void print_outputs()
{
    // Print .output format
    printf("INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\n");
    for(auto& pair : completed_instructions)
    {
        schedule_entry& entry = pair.second;
        printf("%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n",
               entry.tag,
               entry.fetch_cycle,
               entry.dispatch_cycle,
               entry.schedule_cycle,
               entry.execute_cycle,
               entry.state_cycle);
    }

    printf("\n");

    // Print .log format
    printf("CYCLE\tOPERATION\tINSTRUCTION\n");
    for(auto& log_entry : log_entries)
    {
        printf("%lu\t%s\t%lu\n",
               log_entry.first,
               log_entry.second.first.c_str(),
               log_entry.second.second);
    }
}

/**
 * Complete processing and calculate final statistics
 */
void complete_proc(proc_stats_t *p_stats)
{
    // Calculate average instructions retired per cycle (IPC)
    if(p_stats->cycle_count > 0)
    {
        p_stats->avg_inst_retired = (float)p_stats->retired_instruction / (float)p_stats->cycle_count;
        p_stats->avg_inst_fired = (float)total_fired / (float)p_stats->cycle_count;
        p_stats->avg_disp_size = (float)total_disp_size / (float)p_stats->cycle_count;
    }
    else
    {
        p_stats->avg_inst_retired = 0.0f;
        p_stats->avg_inst_fired = 0.0f;
        p_stats->avg_disp_size = 0.0f;
    }

    p_stats->max_disp_size = max_disp_size;
}
