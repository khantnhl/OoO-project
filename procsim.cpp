#include "procsim.hpp"
#include <algorithm>

// Global variable DEFINITIONS
int32_t gfu[3] = {0};
int32_t g_reg[128] = {0};  // 0 = ready, 1+ = busy (tag that will write to it)
std::deque<instr> q;  // Fetch queue
std::deque<dis_instr> d_q;  // Dispatch queue
int64_t g_r = 0;
int64_t g_k0 = 0;
int64_t g_k1 = 0;
int64_t g_k2 = 0;
int64_t g_f = 0;
int64_t g_ret = 0;
int64_t g_rs = 0;
uint64_t g_tag = 0;
uint64_t g_cnt = 0;
uint64_t g_cycle = 0;
std::priority_queue<n_retire, std::vector<n_retire>, std::greater<n_retire>> r_q;
std::vector<instr> instructions;
size_t f_tracker = 0;
Node* head = nullptr;

// Additional tracking structures
struct ScheduleEntry {
    uint64_t tag;
    instr instruction;
    bool fired;  // Has this instruction been dispatched to FU?
    uint64_t schedule_cycle;  // Cycle when instruction entered schedule queue
    uint64_t fire_cycle;  // Cycle when instruction fired
    bool completed;  // Has execution completed?

    ScheduleEntry(uint64_t t, instr i, uint64_t sc) : tag(t), instruction(i), fired(false),
                                         schedule_cycle(sc), fire_cycle(0), completed(false) {}
};

std::vector<ScheduleEntry> schedule_queue;  // Reservation station
uint64_t total_disp_size = 0;  // For average calculation
uint64_t total_fired = 0;  // Total instructions fired

/**
 * Setup processor
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
    g_cnt = 0;
    total_disp_size = 0;
    total_fired = 0;

    g_rs = 2 * (k0 + k1 + k2);

    for(int32_t i = 0; i < 128; ++i)
    {
        g_reg[i] = 0;  // All registers ready
    }

    for(int32_t i = 0; i < 3; ++i)
    {
        gfu[i] = 0;  // All FUs free
    }

    // Read all instructions
    // Note: Address is in hex format, so need to read it specially
    while(std::cin >> std::hex >> n.pc >> std::dec >> n.fu >> n.dest >> n.source1 >> n.source2)
    {
        // Handle FU type -1 -> use FU type 1
        if(n.fu == -1) n.fu = 1;
        instructions.push_back(n);
    }
}

/**
 * State Update stage - remove completed instructions from schedule queue
 */
void state_update()
{
    // Collect completed instructions (those that got onto result bus last cycle)
    std::vector<uint64_t> to_remove;

    for(size_t i = 0; i < schedule_queue.size(); ++i)
    {
        if(schedule_queue[i].completed && schedule_queue[i].fire_cycle < g_cycle)
        {
            to_remove.push_back(i);
        }
    }

    // Remove in reverse order to maintain indices
    for(int i = to_remove.size() - 1; i >= 0; --i)
    {
        schedule_queue.erase(schedule_queue.begin() + to_remove[i]);
        g_cnt--;
        g_ret++;
    }
}

/**
 * Execute stage - handle instruction completion and result bus allocation
 */
void execute_complete()
{
    // Find all instructions that completed execution this cycle
    std::vector<size_t> completed_indices;

    for(size_t i = 0; i < schedule_queue.size(); ++i)
    {
        ScheduleEntry& entry = schedule_queue[i];

        // If fired and execution latency complete (1 cycle for all FUs)
        if(entry.fired && !entry.completed && (g_cycle > entry.fire_cycle))
        {
            completed_indices.push_back(i);
        }
    }

    // Sort by tag (oldest first for result bus priority)
    std::sort(completed_indices.begin(), completed_indices.end(),
              [](size_t a, size_t b) {
                  return schedule_queue[a].tag < schedule_queue[b].tag;
              });

    // Allocate result buses (limited to R buses)
    uint64_t buses_used = 0;
    for(size_t idx : completed_indices)
    {
        if(buses_used >= (uint64_t)g_r) break;

        ScheduleEntry& entry = schedule_queue[idx];
        entry.completed = true;

        // Free the register (mark as ready)
        if(entry.instruction.dest != -1)
        {
            g_reg[entry.instruction.dest] = 0;
        }

        // Free the FU
        int fu_type = entry.instruction.fu;
        if(fu_type >= 0 && fu_type <= 2)
        {
            gfu[fu_type]--;
        }

        buses_used++;
    }
}

/**
 * Execute stage - fire ready instructions
 */
void execute_fire()
{
    // Count available FUs
    int avail_fu[3];
    avail_fu[0] = g_k0 - gfu[0];
    avail_fu[1] = g_k1 - gfu[1];
    avail_fu[2] = g_k2 - gfu[2];

    // Find ready instructions (not yet fired, dependencies met)
    std::vector<size_t> ready_indices;

    for(size_t i = 0; i < schedule_queue.size(); ++i)
    {
        ScheduleEntry& entry = schedule_queue[i];

        if(entry.fired) continue;  // Already executing

        // Instruction must spend at least 1 cycle in schedule queue
        // If entered in cycle J, can't fire until cycle J+1
        if(g_cycle <= entry.schedule_cycle) continue;

        // Check if source registers are ready
        // Note: If source == dest, the instruction doesn't depend on itself
        bool src1_ready = (entry.instruction.source1 == -1) ||
                          (entry.instruction.source1 == entry.instruction.dest) ||
                          (g_reg[entry.instruction.source1] == 0);
        bool src2_ready = (entry.instruction.source2 == -1) ||
                          (entry.instruction.source2 == entry.instruction.dest) ||
                          (g_reg[entry.instruction.source2] == 0);

        if(src1_ready && src2_ready)
        {
            ready_indices.push_back(i);
        }
    }

    // Sort by tag (fire in program order)
    std::sort(ready_indices.begin(), ready_indices.end(),
              [](size_t a, size_t b) {
                  return schedule_queue[a].tag < schedule_queue[b].tag;
              });

    // Fire instructions to available FUs
    for(size_t idx : ready_indices)
    {
        ScheduleEntry& entry = schedule_queue[idx];
        int fu_type = entry.instruction.fu;

        // Check if FU is available
        if(avail_fu[fu_type] > 0)
        {
            entry.fired = true;
            entry.fire_cycle = g_cycle;
            gfu[fu_type]++;
            avail_fu[fu_type]--;
            total_fired++;
        }
    }
}

/**
 * Schedule stage - move instructions from dispatch queue to scheduling queue
 */
void schedule()
{
    // Move from dispatch queue to schedule queue (in order)
    while(!d_q.empty() && g_cnt < (uint64_t)g_rs)
    {
        dis_instr& entry = d_q.front();

        // Add to schedule queue (with current cycle)
        schedule_queue.push_back(ScheduleEntry(entry.tag, entry.instruction, g_cycle));
        g_cnt++;

        // Mark destination register as busy
        if(entry.instruction.dest != -1)
        {
            g_reg[entry.instruction.dest] = 1;
        }

        d_q.pop_front();
    }
}

/**
 * Dispatch stage - move instructions from fetch queue to dispatch queue
 */
void dispatch()
{
    // Move all fetched instructions to dispatch queue (unlimited size)
    while(!q.empty())
    {
        dis_instr entry;
        entry.tag = ++g_tag;
        entry.instruction = q.front();
        d_q.push_back(entry);
        q.pop_front();
    }
}

/**
 * Fetch stage - fetch new instructions
 */
void fetch()
{
    // Fetch up to F instructions per cycle
    for(uint64_t i = 0; i < (uint64_t)g_f && f_tracker < instructions.size(); ++i)
    {
        q.push_back(instructions[f_tracker]);
        f_tracker++;
    }
}

/**
 * Main simulation loop
 */
void run_proc(proc_stats_t* p_stats)
{
    uint64_t max_disp_size = 0;

    // Simulate until all instructions are done
    while(f_tracker < instructions.size() || !q.empty() || !d_q.empty() ||
          !schedule_queue.empty())
    {
        g_cycle++;

        // Track dispatch queue size for statistics
        total_disp_size += d_q.size();
        if(d_q.size() > max_disp_size)
        {
            max_disp_size = d_q.size();
        }

        // Process stages in reverse order (to avoid race conditions)
        // This models the half-cycle behavior described in spec

        state_update();      // Remove completed instructions
        execute_complete();  // Handle completion and result buses
        execute_fire();      // Fire ready instructions
        schedule();          // Move to schedule queue
        dispatch();          // Move to dispatch queue
        fetch();             // Fetch new instructions
    }

    // Calculate statistics
    p_stats->cycle_count = g_cycle;
    p_stats->retired_instruction = g_ret;
    p_stats->max_disp_size = max_disp_size;

    if(g_cycle > 0)
    {
        p_stats->avg_disp_size = (float)total_disp_size / (float)g_cycle;
        p_stats->avg_inst_fired = (float)total_fired / (float)g_cycle;
        p_stats->avg_inst_retired = (float)g_ret / (float)g_cycle;
    }
    else
    {
        p_stats->avg_disp_size = 0;
        p_stats->avg_inst_fired = 0;
        p_stats->avg_inst_retired = 0;
    }
}

/**
 * Cleanup and finalize statistics
 */
void complete_proc(proc_stats_t *p_stats)
{
    // Nothing additional needed
}
