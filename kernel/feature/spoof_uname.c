#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/utsname.h>

#include "feature/spoof_uname.h"
#include "hook/patch_memory.h"
#include "infra/symbol_resolver.h"
#include "policy/feature.h"
#include "uapi/feature.h"
#include "klog.h" // IWYU pragma: keep

/*
 * Spoofed strings come from module params so one .ko fits many devices:
 *   spoof_release=<stock release> spoof_version=<stock version>
 * e.g.
 *   spoof_release=13.2.0-b5f5c8a1a2b3-gki
 *   spoof_version="#1 SMP PREEMPT Wed Sep 4 19:47:35 UTC 2024"
 * An empty field is left untouched. Fields are __NEW_UTS_LEN + 1 bytes.
 */
static char spoof_release[__NEW_UTS_LEN + 1] = "";
module_param_string(spoof_release, spoof_release, sizeof(spoof_release), 0);

static char spoof_version[__NEW_UTS_LEN + 1] = "";
module_param_string(spoof_version, spoof_version, sizeof(spoof_version), 0);

static bool ksu_spoof_uname_enabled __read_mostly;

/*
 * Why we patch init_uts_ns.name instead of hooking the syscall:
 *
 * 1) Coverage (fatal for the hook approach). The dispatcher only runs from
 *    the sys_enter tracepoint, and hook/syscall_hook_manager.c's
 *    ksu_mark_running_process_locked() marks only ksu-root / zygote / shell /
 *    init / allow-listed uids. An ungranted detection app thread has no
 *    TIF_SYSCALL_TRACEPOINT, so sys_enter never fires and a hook would never
 *    execute for the exact process we need to fool.
 * 2) Latency side channel. A hook costs tracepoint + redirection + dispatcher
 *    lookup on every call, which contradicts the goal of removing syscall
 *    timing anomalies. Patching the data is off the read path entirely.
 * 3) Cross-checking. /proc/sys/kernel/osrelease and .../version point
 *    directly at the same init_uts_ns.name arrays
 *    (kernel/utsname_sysctl.c). Spoofing only the syscall return value makes
 *    uname() and a plain cat disagree.
 *
 * Patching the structs themselves solves all three: zero per-call cost, all
 * uids agree (Android apps do not create a new CLONE_NEWUTS, so they share
 * init_uts_ns), and syscall and sysctl stay consistent by construction.
 *
 * ksu_patch_text() is used rather than memcpy(): it goes through fixmap, so
 * it works even if the target ends up in a read-only section on a given
 * vendor kernel, and it reports failure instead of faulting. It only runs at
 * init and on toggle, never per read.
 */

static struct uts_namespace *init_uts;
static char orig_release[__NEW_UTS_LEN + 1];
static char orig_version[__NEW_UTS_LEN + 1];
static bool armed;

static int uts_write(char *dst, const char *value)
{
    char fixed[__NEW_UTS_LEN + 1] = { 0 };
    size_t len = strlen(value);

    if (len > __NEW_UTS_LEN)
        len = __NEW_UTS_LEN;
    memcpy(fixed, value, len);

    /*
     * Always overwrite the whole field: sys_newuname() copies sizeof(*name),
     * so a partial write leaves the tail of the stock string in userspace and
     * a detector can reassemble the real version from the leftover bytes.
     */
    return ksu_patch_text(dst, fixed, sizeof(fixed), KSU_PATCH_TEXT_FLUSH_DCACHE);
}

static int spoof_uname_apply(bool spoof)
{
    int ret = 0;

    if (spoof_release[0])
        ret |= uts_write(init_uts->name.release, spoof ? spoof_release : orig_release);
    if (spoof_version[0])
        ret |= uts_write(init_uts->name.version, spoof ? spoof_version : orig_version);

    return ret ? -EIO : 0;
}

static int spoof_uname_feature_get(u64 *value)
{
    *value = (armed && READ_ONCE(ksu_spoof_uname_enabled)) ? 1 : 0;
    return 0;
}

static int spoof_uname_feature_set(u64 value)
{
    int ret;

    if (!armed) {
        pr_warn("spoof_uname: not armed (no spoof params or init_uts_ns missing), cannot toggle\n");
        return -EINVAL;
    }

    ret = spoof_uname_apply(value != 0);
    if (ret) {
        pr_err("spoof_uname: write uts fields failed: %d\n", ret);
        return ret;
    }

    WRITE_ONCE(ksu_spoof_uname_enabled, value != 0);
    return 0;
}

static const struct ksu_feature_handler spoof_uname_handler = {
    .feature_id = KSU_FEATURE_SPOOF_UNAME,
    .name = "spoof_uname",
    .get_handler = spoof_uname_feature_get,
    .set_handler = spoof_uname_feature_set,
};

void __init ksu_spoof_uname_init(void)
{
    unsigned long sym;
    size_t len;

    if (!spoof_release[0] && !spoof_version[0]) {
        pr_warn("spoof_uname: spoof_release/spoof_version both empty, feature inert\n");
        goto register_feature;
    }

    sym = find_kernel_symbol_exact("init_uts_ns");
    if (!sym) {
        pr_warn("spoof_uname: init_uts_ns not found, feature inert\n");
        goto register_feature;
    }

    init_uts = (struct uts_namespace *)sym;

    /* Back up the stock values first so we can restore the exact bytes. */
    len = strnlen(init_uts->name.release, __NEW_UTS_LEN);
    memcpy(orig_release, init_uts->name.release, len);
    orig_release[len] = '\0';

    len = strnlen(init_uts->name.version, __NEW_UTS_LEN);
    memcpy(orig_version, init_uts->name.version, len);
    orig_version[len] = '\0';

    if (spoof_uname_apply(true)) {
        pr_err("spoof_uname: initial spoof write failed, feature inert\n");
        init_uts = NULL;
        goto register_feature;
    }

    armed = true;
    WRITE_ONCE(ksu_spoof_uname_enabled, true);
    pr_info("spoof_uname: uts spoofed (release='%s' version='%s')\n", spoof_release, spoof_version);

register_feature:
    if (ksu_register_feature_handler(&spoof_uname_handler))
        pr_err("spoof_uname: failed to register feature handler\n");
}

void __exit ksu_spoof_uname_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_SPOOF_UNAME);

    if (armed) {
        if (spoof_uname_apply(false))
            pr_err("spoof_uname: restore uts fields failed\n");
        armed = false;
        WRITE_ONCE(ksu_spoof_uname_enabled, false);
        init_uts = NULL;
        pr_info("spoof_uname: real uts restored\n");
    }
}
