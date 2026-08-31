//-------------------------------------------------------------------
// Unity build of the emulator core, plus the inner step loop.
//
// Why this exists
// ---------------
// Modules are compiled -mlong-calls: every cross-file call is a literal-pool
// load plus an indirect branch, on a core with no branch prediction. Compiled
// separately, one emulated instruction costs a minimum of eight of those -
// cpu_step, lcd_step, mmu_step and cpu_timers_step from the driver, then
// mmu_read out of the decode loop, then the ROM hook, and more for every
// instruction that touches memory. None of it can be inlined because gcc
// cannot see across translation units.
//
// Compiling the four core files as one unit lets -O2 -finline-functions
// inline mmu_read straight into the decode loop, and putting the step loop
// here as well means the driver makes *one* long call per frame instead of
// four per instruction.
//
// Include order is not arbitrary
// ------------------------------
// cpu.c must come last. It defines single-character macros for the register
// file - A, B, C, D, E, F, H, L, plus AF/BC/DE/HL and CF/HF/NF/ZF - and never
// undefines them. Anything included afterwards that used one of those as an
// identifier would break in ways that do not read like a macro problem.
//
// For the same reason, nothing below this include block may use those names.
//-------------------------------------------------------------------

#include "state.c"
#include "mmu.c"
#include "lcd.c"
#include "cpu.c"        /* last - see above */

//-------------------------------------------------------------------
// The inner loop, moved here from gbc_emu.c so all four per-instruction calls
// are inlinable.
//
// Returns the number of instructions retired. `guard` bounds the loop: a ROM
// with the LCD switched off never reaches vblank, and without a ceiling this
// would spin inside the camera's GUI task with no way out. *hit_guard is set
// when that ceiling is what ended the frame rather than vblank.

// Three of the four per-instruction calls are counter bumps that only do real
// work when a counter expires. Profiling the camera ROM (tools/gbc_bench.c,
// gprof) put lcd_step at 32% of runtime and cpu_timers_step at 12% - 44%
// between them - each entered 39 million times over 4000 frames, once per
// emulated instruction, overwhelmingly to decrement something and return.
//
// So the expiry test is hoisted into this loop and the function is called only
// when it will actually do something. Each fast path below is the exact
// complement of the early exit inside the function it guards; the slow path is
// still the original function, so behaviour is unchanged and the two cannot
// drift apart in the way a reimplementation would.
//
// The win is larger on the A480 than the profile suggests: modules build
// -mlong-calls, so the call that is being avoided is a literal-pool load plus
// an indirect branch on a core with no branch prediction.

unsigned gbc_core_step_frame(struct gb_state *s, unsigned guard, int *hit_guard)
{
    struct emu_state *es = s->emu_state;
    unsigned retired = 0;

    /* Cached cycle count for a CPU sitting in HALT - see below. */
    u32 halt_cycles = 0;
    int halt_cycles_valid = 0;

    /* DIV's tick length, hoisted.
     *
     * perf annotate put the gb_div_cycles[] lookup - a PC-relative lea plus an
     * indexed load plus the compare - at ~12% of this function, paid on every
     * instruction to fetch a value that essentially never changes. It depends
     * only on s->double_speed, and the only thing that can write KEY1 is an
     * executed instruction, so it cannot change on an iteration where cpu_step
     * did not run. Refreshed after each real step and reused across the halted
     * half. */
    u32 div_ticks = gb_div_cycles[s->double_speed ? 1 : 0];

    *hit_guard = 0;

    /* Cleared once here rather than on every iteration of the fast path below.
     *
     * Only three places read these: mmu_step() reads hblank, lcd_step() reads
     * hblank to decide whether to rasterise, and this loop's exit test reads
     * vblank. The fast path calls neither function, so its hblank store was
     * dead - lcd_step() clears both itself before anything can observe them.
     * And vblank is necessarily still 0 at the top of any iteration, because
     * the loop only continues while it is; it just has to start that way, in
     * case the previous frame left it set. */
    es->lcd_entered_hblank = 0;
    es->lcd_entered_vblank = 0;

    do
    {
        u32 op_cycles;

        /* --- cpu_step, skipped entirely while idling in HALT -----------
         *
         * Roughly half of this ROM's emulated instructions are a CPU parked in
         * HALT waiting for an interrupt. That showed up as mmu_read being
         * called 68 million times against 39 million instructions: removing the
         * duplicate opcode fetch only took 19.5 million off it, exactly half of
         * one per instruction, because cpu_do_instruction() runs on only half
         * the steps.
         *
         * With halt set and nothing pending, cpu_step() is a no-op with one
         * side effect. cpu_handle_interrupts() returns immediately, the opcode
         * fetch reads the same byte as last time because pc cannot move,
         * cpu_do_instruction() is skipped, and the pc range check passes on an
         * unchanged pc. All it does is set last_op_cycles - to a value that
         * cannot have changed, for the same reason.
         *
         * So cache that value and skip the lot. Exactly equivalent, not an
         * approximation: the moment an interrupt is pending, or halt clears,
         * the real cpu_step() runs. Confirmed by framebuffer hash.
         *
         * (last_op_cycles here is the cycle count of the instruction *after*
         * the HALT, which is not what hardware does - but it is what upstream
         * has always done, and matching it keeps the timing identical.) */
        if (s->halt_for_interrupts && halt_cycles_valid &&
            !(s->interrupts_enable & s->interrupts_request))
        {
            es->last_op_cycles = halt_cycles;
        }
        else
        {
            cpu_step(s);
            halt_cycles_valid = s->halt_for_interrupts;
            if (halt_cycles_valid)
                halt_cycles = es->last_op_cycles;

            /* Only an executed instruction can have written KEY1. */
            div_ticks = gb_div_cycles[s->double_speed ? 1 : 0];
        }

        op_cycles = es->last_op_cycles;

        /* --- lcd_step ------------------------------------------------
         * It clears both entered_* flags, subtracts, and only advances the
         * mode machine if the result went negative. Modes last 80-4560
         * cycles and an instruction is at most ~24, so the subtraction alone
         * is the common case by a wide margin. */
        if (s->io_lcd_mode_cycles_left >= (int)op_cycles)
        {
            s->io_lcd_mode_cycles_left -= (int)op_cycles;
            /* No flag stores here - see the note above the loop. mmu_step()
             * only acts on entering hblank, which did not happen. */
        }
        else
        {
            lcd_step(s);
            mmu_step(s);
        }

        /* --- cpu_timers_step ----------------------------------------
         * DIV always runs; TIMA only when TAC bit 2 is set, and when it is
         * clear the original does not accumulate TIMA_cycles at all - so
         * neither does this. */
        {
            u32 div_next = s->io_timer_DIV_cycles + op_cycles;
            int timer_on = (s->io_timer_TAC & (1 << 2)) != 0;

            if (div_next < div_ticks &&
                (!timer_on ||
                 s->io_timer_TIMA_cycles + op_cycles <
                     gb_tima_cycles[s->double_speed ? 1 : 0]
                                   [s->io_timer_TAC & 0x3]))
            {
                s->io_timer_DIV_cycles = div_next;
                if (timer_on)
                    s->io_timer_TIMA_cycles += op_cycles;
            }
            else
                cpu_timers_step(s);
        }

        es->time_cycles += op_cycles;
        if (es->time_cycles >= GB_FREQ)
        {
            es->time_cycles %= GB_FREQ;
            es->time_seconds++;
        }

        retired++;

        if (gbc_panicked)
            return retired;

        if (retired >= guard)
        {
            *hit_guard = 1;
            return retired;
        }
    }
    while (!es->lcd_entered_vblank);

    return retired;
}
