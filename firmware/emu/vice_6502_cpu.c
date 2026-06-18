#include "vice_6502_cpu.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

#include "6510core.h"

enum cpu_int {
    IK_NONE = 0,
    IK_IRQ = 1 << 0,
    IK_NMI = 1 << 1,
    IK_RESET = 1 << 2,
    IK_TRAP = 1 << 3,
    IK_MONITOR = 1 << 4,
    IK_DMA = 1 << 5,
    IK_IRQPEND = 1 << 6
};

typedef struct {
    unsigned int global_pending_int;
    uint64_t irq_pending_clk;
    uint64_t irq_clk;
    uint64_t nmi_clk;
    int num_dma_per_opcode;
} vice_cpu_int_status_t;

enum {
    INTERRUPT_DELAY = 2,
    TRAP_OPCODE = 0x02,
    JAM_RESET_CPU = 1,
    JAM_POWER_CYCLE = 2,
    JAM_MONITOR = 3,
    MI_STEP = 1 << 0,
    MI_BREAK = 1 << 1,
    MI_WATCH = 1 << 2,
    CALLER = 0,
    ORIGIN_MEMSPACE = 0
};

static int monitor_mask[1];
bool maincpu_profiling = false;

static void cpu_reset(void) {}
static int interrupt_check_nmi_delay(vice_cpu_int_status_t *status, uint64_t clk)
{
    (void)status;
    (void)clk;
    return 0;
}
static int interrupt_check_irq_delay(vice_cpu_int_status_t *status, uint64_t clk)
{
    return clk >= status->irq_clk + INTERRUPT_DELAY;
}
static void interrupt_ack_irq(vice_cpu_int_status_t *status)
{
    status->global_pending_int &= ~(IK_IRQ | IK_IRQPEND);
}
static void interrupt_ack_nmi(vice_cpu_int_status_t *status)
{
    status->global_pending_int &= ~IK_NMI;
}
static void interrupt_ack_reset(vice_cpu_int_status_t *status)
{
    status->global_pending_int &= ~IK_RESET;
}
static void interrupt_ack_dma(vice_cpu_int_status_t *status)
{
    status->global_pending_int &= ~IK_DMA;
}
static void interrupt_do_trap(vice_cpu_int_status_t *status, uint16_t pc)
{
    (void)pc;
    status->global_pending_int &= ~IK_TRAP;
}
void profile_int(uint16_t dest_addr, uint16_t handler, uint8_t sp, uint16_t cycles)
{
    (void)dest_addr;
    (void)handler;
    (void)sp;
    (void)cycles;
}
void profile_jsr(uint16_t dest_addr, uint16_t pc, uint8_t sp)
{
    (void)dest_addr;
    (void)pc;
    (void)sp;
}
static void profile_rtx(uint8_t sp)
{
    (void)sp;
}
void profile_sample_start(uint16_t pc)
{
    (void)pc;
}
void profile_sample_finish(uint16_t cycles, uint16_t stolen_cycles)
{
    (void)cycles;
    (void)stolen_cycles;
}
static void monitor_check_icount_interrupt(void) {}
static void monitor_check_icount(uint16_t pc)
{
    (void)pc;
}
static int monitor_check_breakpoints(int caller, uint16_t pc)
{
    (void)caller;
    (void)pc;
    return 0;
}
static void monitor_startup(int caller)
{
    (void)caller;
}
static void monitor_startup_trap(void) {}
static void monitor_check_watchpoints(unsigned int last_opcode_addr, uint16_t pc)
{
    (void)last_opcode_addr;
    (void)pc;
}
static void monitor_cpuhistory_store(uint64_t history_clk, unsigned int pc,
                                     uint8_t p0, uint8_t p1, uint8_t p2,
                                     uint8_t a, uint8_t x, uint8_t y,
                                     uint8_t sp, uint8_t status,
                                     int memspace)
{
    (void)history_clk;
    (void)pc;
    (void)p0;
    (void)p1;
    (void)p2;
    (void)a;
    (void)x;
    (void)y;
    (void)sp;
    (void)status;
    (void)memspace;
}
static void monitor_cpuhistory_fix_p2(unsigned int value)
{
    (void)value;
}
static void log_warning(int log_id, const char *format, ...)
{
    (void)log_id;
    (void)format;
}

uint32_t vice_6502_step(pet2001_t *pet, uint32_t cycle_budget)
{
    uint64_t clk = 0;
    uint64_t start_clk = 0;
    unsigned int last_opcode_info = 0;
    unsigned int last_opcode_addr = 0;
    int cpu_is_jammed = 0;
    vice_cpu_int_status_t int_status = {0};

    if (pet == NULL || cycle_budget == 0) {
        return 0;
    }

    int_status.global_pending_int = IK_IRQ;
    int_status.irq_clk = 0;
    int_status.irq_pending_clk = 0;

    while ((uint32_t)(clk - start_clk) < cycle_budget && !cpu_is_jammed) {
        uint8_t reg_a = pet->cpu.a;
        uint8_t reg_x = pet->cpu.x;
        uint8_t reg_y = pet->cpu.y;
        uint8_t reg_p = pet->cpu.p;
        uint8_t reg_sp = pet->cpu.sp;
        uint8_t flag_n = pet->cpu.n;
        uint8_t flag_z = pet->cpu.z;
        unsigned int reg_pc = pet->cpu.pc;
        uint8_t *bank_base = NULL;
        int bank_start = 0;
        int bank_limit = 0;

        if (pet2001_kernal_trap(pet)) {
            clk += 6;
            continue;
        }

#define CPU_LOG_ID 0
#define CPU_INT_STATUS (&int_status)
#define CPU_IS_JAMMED cpu_is_jammed
#define CLK clk
#define RMW_FLAG pet->cpu_rmw_flag
#define LAST_OPCODE_INFO last_opcode_info
#define LAST_OPCODE_ADDR last_opcode_addr
#define GLOBAL_REGS pet->cpu
#define CHECK_PENDING_ALARM() 0
#define CHECK_PENDING_INTERRUPT() 0
#define CYCLE_EXACT_ALARM 1
#define CPU_DELAY_CLK
#define CPU_REFRESH_CLK
#define TRACEFLG 0
#define ANE_LOG_LEVEL 0
#define LXA_LOG_LEVEL 0
#define STATIC_ASSERT(x) typedef char static_assertion_##__LINE__[(x) ? 1 : -1]
#define LOAD(addr) pet2001_read(pet, (uint16_t)(addr))
#define STORE(addr, value) pet2001_write(pet, (uint16_t)(addr), (uint8_t)(value))
#define LOAD_ZERO(addr) pet2001_read(pet, (uint16_t)((addr) & 0xff))
#define STORE_ZERO(addr, value) pet2001_write(pet, (uint16_t)((addr) & 0xff), (uint8_t)(value))
#define LOAD_DUMMY(addr) LOAD(addr)
#define STORE_DUMMY(addr, value) ((void)LOAD(addr), (void)(value))
#define LOAD_ZERO_DUMMY(addr) LOAD_ZERO(addr)
#define STORE_ZERO_DUMMY(addr, value) ((void)LOAD_ZERO(addr), (void)(value))
#define LOAD_ADDR(addr) (LOAD(addr) | (LOAD((uint16_t)((addr) + 1)) << 8))
#define LOAD_ZERO_ADDR(addr) (LOAD_ZERO(addr) | (LOAD_ZERO((uint8_t)((addr) + 1)) << 8))
#define LOAD_ADDR_DUMMY(addr) LOAD_ADDR(addr)
#define LOAD_ZERO_ADDR_DUMMY(addr) LOAD_ZERO_ADDR(addr)
#define JUMP(addr) do { reg_pc = (unsigned int)((addr) & 0xffff); } while (0)
#define ROM_TRAP_ALLOWED() 0
#define ROM_TRAP_HANDLER() ((uint32_t)-1)
#define JAM() do { CPU_IS_JAMMED = 1; } while (0)
#define DMA_ON_RESET
#define DMA_FUNC

#include "6510core.c"

        EXPORT_REGISTERS();
        pet->pc = (uint16_t)pet->cpu.pc;

#undef CPU_LOG_ID
#undef CPU_INT_STATUS
#undef CPU_IS_JAMMED
#undef CLK
#undef RMW_FLAG
#undef LAST_OPCODE_INFO
#undef LAST_OPCODE_ADDR
#undef GLOBAL_REGS
#undef CHECK_PENDING_ALARM
#undef CHECK_PENDING_INTERRUPT
#undef CYCLE_EXACT_ALARM
#undef CPU_DELAY_CLK
#undef CPU_REFRESH_CLK
#undef TRACEFLG
#undef ANE_LOG_LEVEL
#undef LXA_LOG_LEVEL
#undef STATIC_ASSERT
#undef LOAD
#undef STORE
#undef LOAD_ZERO
#undef STORE_ZERO
#undef LOAD_DUMMY
#undef STORE_DUMMY
#undef LOAD_ZERO_DUMMY
#undef STORE_ZERO_DUMMY
#undef LOAD_ADDR
#undef LOAD_ZERO_ADDR
#undef LOAD_ADDR_DUMMY
#undef LOAD_ZERO_ADDR_DUMMY
#undef JUMP
#undef ROM_TRAP_ALLOWED
#undef ROM_TRAP_HANDLER
#undef JAM
#undef DMA_ON_RESET
#undef DMA_FUNC
    }

    return (uint32_t)(clk - start_clk);
}
