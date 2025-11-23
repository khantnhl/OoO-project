#include "procsim.hpp"

// Global variable DEFINITIONS (only here, not in header)
int32_t gfu[3] = {0};
int32_t g_reg[128] = {0};
std::deque<instr> q;
std::deque<dis_instr> d_q;
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

// Statistics tracking
uint64_t total_disp_size = 0;
uint64_t total_fired = 0;

/**
 * Subroutine for initializing the processor. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @r number of result busses
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
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
    total_fired = 0;

    g_rs = 2 * (k0 + k1 + k2);

    for(int32_t i = 0; i < 128; ++i)
    {
        //0 is not busy
        g_reg[i] = 0;
    }

    // Read instructions into vector - FIX: need hex parsing for addresses
    while(std::cin >> std::hex >> n.pc >> std::dec >> n.fu >> n.dest >> n.source1 >> n.source2)
    {
        // Handle FU type -1 -> use FU type 1
        if(n.fu == -1) n.fu = 1;
        instructions.push_back(n);
    }
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */

void fetch()
{
    for(uint64_t i = 0; i < g_f && f_tracker < instructions.size(); ++i)
    {
        q.push_back(instructions[f_tracker]);
        f_tracker++;
    }
}

void dispatch()
{
    while(!q.empty())
    {
        dis_instr dis;
        dis.tag = ++g_tag;
        dis.instruction = q.front();
        d_q.push_back(dis);
        q.pop_front();
    }
}

void schedule()
{
    // FIX: Need to APPEND to existing list, not create new one
    // Find the tail of the current list
    Node* tail = nullptr;
    if(head != nullptr)
    {
        tail = head;
        while(tail->next != nullptr)
        {
            tail = tail->next;
        }
    }

    // Add new entries from dispatch queue
    while(!d_q.empty() && g_cnt < g_rs)
    {
        dis_instr c_int = d_q.front();
        Node* next = new Node(nullptr, c_int.instruction, c_int.tag, g_cycle);

        if(c_int.instruction.dest != -1)
        {
            //1 is the busy value
            g_reg[c_int.instruction.dest] = 1;
        }

        if(head == nullptr)
        {
            head = next;
            tail = next;
        }
        else
        {
            tail->next = next;
            tail = next;
        }

        d_q.pop_front();
        g_cnt++;
    }
}

void execute()
{
    Node* curr = head;

    // FIX: Need to check dependencies before executing
    while (curr)
    {
        bool can_execute = false;
        int fu_type = curr->instruction.fu;

        // Skip if already fired
        if(curr->fired) {
            curr = curr->next;
            continue;
        }

        // Instruction must spend at least 1 cycle in schedule queue
        if(curr->schedule_cycle >= g_cycle) {
            curr = curr->next;
            continue;
        }

        // Check if source registers are ready
        bool src1_ready = (curr->instruction.source1 == -1) ||
                          (curr->instruction.source1 == curr->instruction.dest) ||
                          (g_reg[curr->instruction.source1] == 0);
        bool src2_ready = (curr->instruction.source2 == -1) ||
                          (curr->instruction.source2 == curr->instruction.dest) ||
                          (g_reg[curr->instruction.source2] == 0);

        if(!src1_ready || !src2_ready)
        {
            curr = curr->next;
            continue;
        }

        // Check if FU is available
        switch (fu_type)
        {
            case 0:
                if (gfu[0] < g_k0)
                {
                    gfu[0]++;
                    can_execute = true;
                }
                break;
            case 1:
                if (gfu[1] < g_k1)
                {
                    gfu[1]++;
                    can_execute = true;
                }
                break;
            case 2:
                if (gfu[2] < g_k2)
                {
                    gfu[2]++;
                    can_execute = true;
                }
                break;
        }

        if (can_execute)
        {
            // Mark as fired and add to retire queue
            // But DON'T remove from schedule queue yet!
            curr->fired = true;
            curr->fire_cycle = g_cycle;
            r_q.push({g_cycle, curr->tag, curr->instruction});
            total_fired++;
        }

        curr = curr->next;
    }

    // Reset FU counters for next cycle
    for(uint32_t i = 0; i < 3; ++i)
    {
        gfu[i] = 0;
    }
}

void retire()
{
    // FIX: Check if queue is empty before accessing
    int retired_this_cycle = 0;
    while(!r_q.empty() && retired_this_cycle < g_r)
    {
        n_retire r_inst = r_q.top();

        // Only retire if execution completed (cycle has passed)
        if(r_inst.cycle >= g_cycle)
        {
            break;  // Not ready yet
        }

        r_q.pop();

        // Free the register
        if(r_inst.instruction.dest != -1)
        {
            g_reg[r_inst.instruction.dest] = 0;
        }

        // Remove from schedule queue (linked list)
        Node* curr = head;
        Node* prev = nullptr;
        while(curr != nullptr)
        {
            if(curr->tag == r_inst.tag)
            {
                // Found the instruction to remove
                if(prev == nullptr)
                {
                    // Removing head
                    head = curr->next;
                }
                else
                {
                    prev->next = curr->next;
                }
                delete curr;
                g_cnt--;
                break;
            }
            prev = curr;
            curr = curr->next;
        }

        g_ret++;
        retired_this_cycle++;
    }
}

void run_proc(proc_stats_t* p_stats)
{
    uint64_t max_disp_size = 0;

    // FIX: Keep cycling until all instructions are fetched and processed
    while(f_tracker < instructions.size() || !q.empty() || !d_q.empty() ||
          g_cnt > 0 || !r_q.empty())
    {
        g_cycle++;  // Increment cycle counter at start of each cycle

        // Track dispatch queue size for statistics
        total_disp_size += d_q.size();
        if(d_q.size() > max_disp_size)
        {
            max_disp_size = d_q.size();
        }

        // Execute stages in REVERSE order (spec requirement)
        retire();     // Retire completed instructions
        execute();    // Execute ready instructions
        schedule();   // Schedule to reservation station
        dispatch();   // Dispatch fetched instructions
        fetch();      // Fetch new instructions each cycle
    }

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
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats)
{
    // Nothing additional needed
}
