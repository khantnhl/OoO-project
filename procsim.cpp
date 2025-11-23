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

    for(int32_t i = 0; i < 3; ++i)
    {
        gfu[i] = 0;
    }

    // FIX: Read hex addresses correctly
    while(std::cin >> std::hex >> n.pc >> std::dec >> n.fu >> n.dest >> n.source1 >> n.source2)
    {
        // FIX: Convert FU type -1 to 1
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
    // FIX: Remove q.size() < g_rs limit (dispatch queue is unlimited)
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
    // FIX: APPEND to existing list instead of recreating it
    Node* tail = nullptr;
    if(head != nullptr)
    {
        tail = head;
        while(tail->next != nullptr)
        {
            tail = tail->next;
        }
    }

    while(!d_q.empty() && g_cnt < g_rs)
    {
        dis_instr c_int = d_q.front();
        Node* next = new Node(nullptr, c_int.instruction, c_int.tag);

        if(c_int.instruction.dest != -1)
        {
            //1 is the busy value
            g_reg[c_int.instruction.dest] = 1;
        }

        if(head == nullptr)
        {
            head = next;
        }
        else
        {
            tail->next = next;
        }
        tail = next;

        d_q.pop_front();
        g_cnt++;
    }
}

void remove(Node* node)
{
    if (node != nullptr && node->next != nullptr)
    {
        Node* temp = node->next;
        node->instruction = temp->instruction;
        node->tag = temp->tag;
        node->next = temp->next;
        g_cnt--;
        delete temp;
    }
}

void execute()
{
    Node* curr = head;
    Node* prev = nullptr;

    g_cycle++;

    while (curr)
    {
        bool executed = false;
        int fu_type = curr->instruction.fu;

        // FIX: Check source register dependencies
        bool src1_ready = (curr->instruction.source1 == -1) ||
                          (curr->instruction.source1 == curr->instruction.dest) ||
                          (g_reg[curr->instruction.source1] == 0);
        bool src2_ready = (curr->instruction.source2 == -1) ||
                          (curr->instruction.source2 == curr->instruction.dest) ||
                          (g_reg[curr->instruction.source2] == 0);

        if(!src1_ready || !src2_ready)
        {
            prev = curr;
            curr = curr->next;
            continue;
        }

        // FIX: Support multiple FUs per type
        switch (fu_type)
        {
            case 0:
                if (gfu[0] < g_k0)
                {
                    gfu[0]++;
                    executed = true;
                }
                break;
            case 1:
                if (gfu[1] < g_k1)
                {
                    gfu[1]++;
                    executed = true;
                }
                break;
            case 2:
                if (gfu[2] < g_k2)
                {
                    gfu[2]++;
                    executed = true;
                }
                break;
        }

        if (executed)
        {
            r_q.push({g_cycle, curr->tag, curr->instruction});
            total_fired++;

            // Remove node from list
            Node* to_delete = curr;
            if(prev == nullptr)
            {
                head = curr->next;
                curr = head;
            }
            else
            {
                prev->next = curr->next;
                curr = curr->next;
            }
            delete to_delete;
            g_cnt--;
        }
        else
        {
            prev = curr;
            curr = curr->next;
        }
    }

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

        // Only retire if execution completed
        if(r_inst.cycle >= g_cycle)
        {
            break;
        }

        r_q.pop();

        if(r_inst.instruction.dest != -1)
        {
            g_reg[r_inst.instruction.dest] = 0;
        }

        g_ret++;
        retired_this_cycle++;
    }
}

void run_proc(proc_stats_t* p_stats)
{
    uint64_t max_disp_size = 0;

    // FIX: Check ALL pipeline stages
    while(f_tracker < instructions.size() || !q.empty() || !d_q.empty() ||
          g_cnt > 0 || !r_q.empty())
    {
        // Track statistics
        total_disp_size += d_q.size();
        if(d_q.size() > max_disp_size)
        {
            max_disp_size = d_q.size();
        }

        // FIX: Execute in REVERSE order
        retire();
        execute();
        schedule();
        dispatch();
        fetch();
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
    // Nothing needed
}
