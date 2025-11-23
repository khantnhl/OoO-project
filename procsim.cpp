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

    // Read instructions into vector instead of linked list
    while(std::cin >> n.pc >> n.fu >> n.dest >> n.source1 >> n.source2)
    {
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
    for(uint64_t i = 0; i < g_f && f_tracker < instructions.size() && q.size() < g_rs; ++i)
    {
        q.push_back(instructions[f_tracker]);
        f_tracker++;
    }

 };

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
};

void schedule()
{
    Node* dummy = new Node();

    Node* curr = dummy;

    while(!d_q.empty() && g_cnt < g_rs)
    {
        dis_instr c_int = d_q.front();
        Node* next = new Node(nullptr, c_int.instruction, c_int.tag);
        if(c_int.instruction.dest != -1)
        {
            //1 is the busy value
            g_reg[c_int.instruction.dest] = 1;
        }
        curr->next = next;
        curr = curr->next;
        d_q.pop_front();
        g_cnt++;
    }

    head = dummy->next;
};

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
};

void execute()
{
    Node* curr = head;

    g_cycle++;
    
    while (curr)
    {
        bool executed = false;
        
        switch (curr->instruction.fu)
        {
            case 0:
                if (gfu[0] == 0)
                {
                    gfu[0]++;
                    executed = true;
                }
                break;
            case -1:
            case 1:
                if (gfu[1] == 0)
                {
                    gfu[1]++;
                    executed = true;
                }
                break;
            case 2:
                if (gfu[2] == 0)
                {
                    gfu[2]++;
                    executed = true;
                }
                break;
        }
        
        if (executed)
        {
            r_q.push({g_cycle, curr->tag, curr->instruction});
            remove(curr);
        }
        else
        {
            curr = curr->next;
        }
    }

    for(u_int32_t i = 0; i < 3; ++i)
    {
        gfu[i] = 0;
    }
};

void retire()
    {   
        for(int32_t i = 0; i < g_r; ++i)
        {
            n_retire r_inst = r_q.top();
            r_q.pop();

            if(r_inst.instruction.dest != -1)
            {
                g_reg[r_inst.instruction.dest] = 0;
            }
            
            g_ret++;
        }
    };

void run_proc(proc_stats_t* p_stats)
{
    // Keep cycling until all instructions are fetched and processed
    while(f_tracker < instructions.size() || !q.empty() || !d_q.empty())
    {
        fetch();    
        dispatch();
        schedule();
        execute();
        retire();
    }
    
    p_stats->cycle_count = g_cycle;
    p_stats->retired_instruction = g_ret;
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


}
