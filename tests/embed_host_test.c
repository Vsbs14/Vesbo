#include <stdio.h>
#include <assert.h>
#include "../include/vesbo.h"

static VesboVM *g_vm1 = NULL;
static VesboVM *g_vm2 = NULL;

/* Native callback registered from C host */
static Value host_add_bonus(VesboVM *vm, int argc, Value *argv) {
    if (argc != 2 || argv[0].type != VAL_NUMBER || argv[1].type != VAL_NUMBER) {
        vesbo_error(vm, "host_add_bonus expects 2 numbers");
        return make_none_value();
    }
    double bonus = (vm == g_vm2) ? 100.0 : 5.0;
    return make_number_value(argv[0].as.number + argv[1].as.number + bonus);
}

int main(int argc, char **argv) {
    const char *script_path = (argc > 1) ? argv[1] : "tests/embed_script.vsb";
    VesboVM *vm = vesbo_new();
    VesboVM *vm2 = vesbo_new();
    assert(vm != NULL && vm2 != NULL);
    g_vm1 = vm;
    g_vm2 = vm2;

    vesbo_register_native(vm, "host_add_bonus", host_add_bonus);
    vesbo_register_native(vm2, "host_add_bonus", host_add_bonus);

    if (vesbo_load(vm, script_path) != 0) {
        fprintf(stderr, "Failed to load script in vm1: %s\n", vesbo_last_error(vm));
        vesbo_free(vm2);
        vesbo_free(vm);
        return 1;
    }
    if (vesbo_load(vm2, script_path) != 0) {
        fprintf(stderr, "Failed to load script in vm2: %s\n", vesbo_last_error(vm2));
        vesbo_free(vm2);
        vesbo_free(vm);
        return 1;
    }

    /* Verify vm2 uses its own VesboVM* context in host_add_bonus, then free vm2 */
    Value arg2 = make_number_value(1.0);
    Value co2 = vesbo_coroutine(vm2, "tick_task", 1, &arg2);
    Value yielded2 = make_none_value();
    int st2 = vesbo_resume(vm2, co2, &yielded2);
    assert(st2 == 1 && yielded2.as.number == 111.0);
    vesbo_free(vm2);
    g_vm2 = NULL;

    /* Create coroutine on vm1, run GC, and verify vm1's native callback & coroutine survive */
    Value start_arg = make_number_value(1.0);
    Value co = vesbo_coroutine(vm, "tick_task", 1, &start_arg);
    assert(co.type == VAL_COROUTINE);
    gc_collect();

    Value yielded = make_none_value();
    int status = vesbo_resume(vm, co, &yielded);
    printf("tick1 status=%d val=%.0f\n", status, yielded.as.number);

    gc_collect();
    status = vesbo_resume_with(vm, co, make_number_value(4.0), &yielded);
    printf("tick2 status=%d val=%.0f\n", status, yielded.as.number);

    status = vesbo_resume_with(vm, co, make_number_value(0.0), &yielded);
    printf("tick3 status=%d val=%.0f\n", status, yielded.as.number);

    Value ticks = vesbo_get_global(vm, "host_ticks");
    printf("host_ticks=%.0f\n", ticks.as.number);
    printf("EMBED_HOST_PASS\n");

    vesbo_free(vm);
    return 0;
}

