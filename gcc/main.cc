#include <gcc-plugin.h>
#include <rtl.h>
#include <target.h>
#include <tree.h>
#include <tree-pass.h>
#include <stringpool.h>
#include <attribs.h>
#include <memmodel.h>
#include <emit-rtl.h>
#include <gimple.h>
#include <gimple-iterator.h>
#include <diagnostic.h>

#include "asm.h"

#define PLUGIN_NAME "pac_sw_plugin"
#define PLUGIN_VERSION "0.1"
#define PLUGIN_HELP PLUGIN_NAME ": Instrumentation for Software-Emulated PAC."
#define PLUGIN_GCC_REQ "10.2.1" // Required GCC version

#define SCOPE_ATTR "pac_scope"

#define err(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#ifdef DEBUG
#define dbg(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define dbg(fmt, ...) ((void) 0)
#endif

#define CURRENT_FN_NAME() IDENTIFIER_POINTER(DECL_NAME(current_function_decl))

int plugin_is_GPL_compatible;
static struct plugin_info inst_plugin_info = {
    .version = PLUGIN_VERSION,
    .help    = PLUGIN_HELP,
};

enum { PROLOGUE, EPILOGUE };

static const char *init_function = NULL;

enum {
    SIGN_SCOPE_nil = 0,         // None
    SIGN_SCOPE_char,            // Char/byte arrays bigger than ssp-buffer-size parameter
    SIGN_SCOPE_array,           // + Arrays of any size and type
    SIGN_SCOPE_strong,          // + Local variables that had their address taken
    SIGN_SCOPE_all              // All functions
};

static int global_scope = SIGN_SCOPE_array;
static bool include_leaf = false;

static struct {
    int total;
    int instrumented;
} inst_stat = { 0, 0 };
static const char *inst_stat_file = NULL;

extern gcc::context *g;

static unsigned int execute_inst_pac(void);
static unsigned int execute_init_pac(void);

// Structure describing an attribute
static struct attribute_spec scope_attr = {
    .name = SCOPE_ATTR,
    .min_length = 1,
    .max_length = 1,
};

// Metadata for the RTL instrumentation pass.  Attaches prologue and epilogue to
// functions where required.
const pass_data pass_data_inst_pac = {
    .type = RTL_PASS,
    .name = "inst_pac",
    .optinfo_flags = OPTGROUP_NONE,
    .tv_id = TV_NONE,
    .properties_required = 0,
    .properties_provided = 0,
    .properties_destroyed = 0,
    .todo_flags_start = 0,
    .todo_flags_finish = 0
};

class pass_inst_pac : public rtl_opt_pass {
public:
    pass_inst_pac (gcc::context *ctxt) : rtl_opt_pass (pass_data_inst_pac, ctxt) {}
    virtual unsigned int execute(function* exec_fun)
    {
        return execute_inst_pac();
    }
};

// Instantiate a new instrumentation RTL pass.
static pass_inst_pac inst_pass = pass_inst_pac(g);

// Metadata for the initialization pass working on GIMPLE.  Registered only if
// the init function is provided.  Attaches the init function to main.
const pass_data pass_data_init_pac = {
    .type = GIMPLE_PASS,
    .name = "init_pac",
    .optinfo_flags = OPTGROUP_NONE,
    .tv_id = TV_NONE,
    .properties_required = 0,
    .properties_provided = 0,
    .properties_destroyed = 0,
    .todo_flags_start = 0,
    .todo_flags_finish = 0,
};

class pass_init_pac : public gimple_opt_pass
{
public:
    pass_init_pac (gcc::context *ctxt) : gimple_opt_pass (pass_data_init_pac, ctxt) {}
    virtual unsigned int execute(function *)
    {
        return execute_init_pac();
    }
};

// Instantiate a new initialization pass.
static pass_init_pac init_pass = pass_init_pac(g);

// Plugin callback called during attribute registration.
static void register_attributes(void *event_data, void *data)
{
    register_attribute(&scope_attr);
}

// Attach initialization function to main
static unsigned int execute_init_pac(void)
{
    tree fn_type, function;
    basic_block bb;
    gimple *stmt;
    gimple_stmt_iterator gsi;
    gcall *call;

    const char *fn_name = IDENTIFIER_POINTER(DECL_NAME(current_function_decl));

    if (strcmp(fn_name, "main") != 0)
        return 0;

    fn_type = build_function_type_list(void_type_node, void_type_node, NULL_TREE);
    function = build_fn_decl(init_function, fn_type);

    bb = ENTRY_BLOCK_PTR_FOR_FN(cfun)->next_bb;
    stmt = gsi_stmt(gsi_start_bb(bb));
    gsi = gsi_for_stmt(stmt);

    call = gimple_build_call(function, 0);
    gsi_insert_before(&gsi, call, GSI_NEW_STMT);

    dbg(PLUGIN_NAME ": %s: Attached %s to %s.\n", main_input_filename, init_function, fn_name);

    return 0;
}

#ifdef GCC_AARCH64_H
static bool aarch64_signing_required(void)
{
    /* This function should only be called after frame laid out. */
    gcc_assert(cfun->machine->frame.laid_out);

    /* If the signing scope does not include leaf functions, we only sign a
       function if its LR is pushed onto stack. */
    return include_leaf ||
        known_ge(cfun->machine->frame.reg_offset[LR_REGNUM], 0);
}
#endif

#define SPCT_HAS_LARGE_CHAR_ARRAY	0x1
#define SPCT_HAS_SMALL_CHAR_ARRAY	0x2
#define SPCT_HAS_ARRAY			0x4
#define SPCT_HAS_AGGREGATE		0x8

static unsigned int stack_protect_classify_type(tree type)
{
    unsigned int ret = 0;
    tree t;

    switch (TREE_CODE(type)) {
    case ARRAY_TYPE:
        t = TYPE_MAIN_VARIANT(TREE_TYPE(type));
        if (t == char_type_node || t == signed_char_type_node || t == unsigned_char_type_node) {
            unsigned HOST_WIDE_INT max = param_ssp_buffer_size;
            unsigned HOST_WIDE_INT len;

            if (!TYPE_SIZE_UNIT(type) || !tree_fits_uhwi_p(TYPE_SIZE_UNIT(type)))
                len = max;
            else
                len = tree_to_uhwi(TYPE_SIZE_UNIT(type));

            if (len < max)
                ret = SPCT_HAS_SMALL_CHAR_ARRAY | SPCT_HAS_ARRAY;
            else
                ret = SPCT_HAS_LARGE_CHAR_ARRAY | SPCT_HAS_ARRAY;
	} else
            ret = SPCT_HAS_ARRAY;

        break;

    case UNION_TYPE:
    case QUAL_UNION_TYPE:
    case RECORD_TYPE:
        ret = SPCT_HAS_AGGREGATE;
        for (t = TYPE_FIELDS(type); t ; t = TREE_CHAIN(t))
            if (TREE_CODE(t) == FIELD_DECL)
                ret |= stack_protect_classify_type(TREE_TYPE(t));
        break;

    default:
        break;
    }

    return ret;
}

// Recursively check for existence of protected declarations (arrays).
static bool has_protected_decls(int scope, tree decl_initial)
{
    if (scope == SIGN_SCOPE_nil || !decl_initial)
        return false;
    if (scope == SIGN_SCOPE_all)
        return true;

    if (TREE_CODE(decl_initial) == VAR_DECL) {
        unsigned int ret = stack_protect_classify_type(TREE_TYPE(decl_initial));
        if (scope >= SIGN_SCOPE_char && (ret & SPCT_HAS_LARGE_CHAR_ARRAY))
            return true;
        else if (scope >= SIGN_SCOPE_array && (ret & SPCT_HAS_ARRAY))
            return true;
        else if (scope >= SIGN_SCOPE_strong && TREE_ADDRESSABLE(decl_initial))
            return true;
        else
            return has_protected_decls(scope, DECL_CHAIN(decl_initial));
    }

    if (TREE_CODE(decl_initial) == BLOCK)
        return (has_protected_decls(scope, BLOCK_VARS(decl_initial)) ||
                has_protected_decls(scope, BLOCK_CHAIN(decl_initial)) ||
                has_protected_decls(scope, BLOCK_SUBBLOCKS(decl_initial)));

    return false;
}

static int str_scope(const char *s)
{
    if (!strcmp(s, "nil"))
        return SIGN_SCOPE_nil;
    if (!strcmp(s, "char"))
        return SIGN_SCOPE_char;
    if (!strcmp(s, "array"))
        return SIGN_SCOPE_array;
    if (!strcmp(s, "strong"))
        return SIGN_SCOPE_strong;
    if (!strcmp(s, "all"))
        return SIGN_SCOPE_all;

    return -1;
}

static int get_current_scope(void)
{
    int scope = global_scope;
    tree decl = current_function_decl;
    tree alias = lookup_attribute(SCOPE_ATTR, DECL_ATTRIBUTES(decl));

    if (!alias)
        return scope;

    const char *val = TREE_STRING_POINTER(TREE_VALUE(TREE_VALUE(alias)));
    scope = str_scope(val);
    if (scope == -1)
        error("invalid %qs argument '%s'", SCOPE_ATTR, val);

    return scope;
}

static bool signing_required(void)
{
    int scope = get_current_scope();

    if (scope == SIGN_SCOPE_nil)
        return false;

    // Do not instrument init-related functions
    if (init_function && (strcmp(CURRENT_FN_NAME(), init_function) == 0 ||
                          strcmp(CURRENT_FN_NAME(), "main") == 0))
        return false;

    /* Turn return address signing off in any function that uses
       __builtin_eh_return.  The address passed to __builtin_eh_return
       is not signed so either it has to be signed (with original sp)
       or the code path that uses it has to avoid authenticating it.
       Currently eh return introduces a return to anywhere gadget, no
       matter what we do here since it uses ret with user provided
       address. An ideal fix for that is to use indirect branch which
       can be protected with BTI j (to some extent).  */
    if (crtl->calls_eh_return)
        return false;

#ifdef GCC_AARCH64_H
    if (!aarch64_signing_required())
        return false;
#endif

    if (cfun->calls_alloca)
        return true;
    return has_protected_decls(scope, DECL_INITIAL(current_function_decl));
}

/* Generate RTL for an asm statement (explicit assembler code).
   STRING is a STRING_CST node containing the assembler code text,
   or an ADDR_EXPR containing a STRING_CST.  VOL nonzero means the
   insn is volatile; don't optimize it.  */
static rtx expand_asm_loc(tree string, int vol, location_t locus)
{
    rtx body;

    body = gen_rtx_ASM_INPUT_loc(VOIDmode,
                                 ggc_strdup(TREE_STRING_POINTER(string)),
                                 locus);

    MEM_VOLATILE_P(body) = vol;

    /* Non-empty basic ASM implicitly clobbers memory.  */
    if (TREE_STRING_LENGTH(string) != 0) {
        rtx asm_op, clob;
        unsigned i, nclobbers;
        auto_vec<rtx> input_rvec, output_rvec;
        auto_vec<const char *> constraints;
        auto_vec<rtx> clobber_rvec;
        HARD_REG_SET clobbered_regs;
        CLEAR_HARD_REG_SET(clobbered_regs);

        clob = gen_rtx_MEM(BLKmode, gen_rtx_SCRATCH(VOIDmode));
        clobber_rvec.safe_push(clob);

        if (targetm.md_asm_adjust)
            targetm.md_asm_adjust(output_rvec, input_rvec,
                                  constraints, clobber_rvec,
                                  clobbered_regs);

        asm_op = body;
        nclobbers = clobber_rvec.length();
        body = gen_rtx_PARALLEL(VOIDmode, rtvec_alloc(1 + nclobbers));

        XVECEXP(body, 0, 0) = asm_op;
        for (i = 0; i < nclobbers; i++)
            XVECEXP(body, 0, i + 1) = gen_rtx_CLOBBER(VOIDmode, clobber_rvec[i]);
    }

    return body;
}

static void insert_prologue(void)
{
    tree string = build_string(strlen(prologue_s), prologue_s);
    rtx body = expand_asm_loc(string, 1, prologue_location);

    /*
     * Insert the prologue as the first non-note instruction to avoid clobbering
     * temporary registers used across the original prologue.  Disabling
     * shrink-wrap ensures that the stack frame is set up for any code path.
     */
    emit_insn_before(body, get_first_nonnote_insn());
}

static bool insert_epilogue(void)
{
    bool ret = false;
    tree string = build_string(strlen(epilogue_s), epilogue_s);
    rtx body = expand_asm_loc(string, 1, epilogue_location);
    rtx_insn *insn = get_insns();

    while (insn) {
        rtx_insn *last_frame_related = NULL;
        while (insn && !(NOTE_P(insn) && NOTE_KIND(insn) == NOTE_INSN_EPILOGUE_BEG))
            insn = NEXT_INSN(insn);

        while (insn && !BARRIER_P(insn)) {
            if (RTX_FRAME_RELATED_P(insn))
                last_frame_related = insn;
            insn = NEXT_INSN(insn);
        }

        if (last_frame_related) {
            emit_insn_after(body, last_frame_related);
            ret = true;
        }
    }

    return ret;
}

// For each function lookup attributes and attach instrumentation.
static unsigned int execute_inst_pac(void)
{
    inst_stat.total++;

    if (signing_required() && insert_epilogue()) {
        /* Include the prologue only if we managed to generate at least one
           epilogue.  Epilogue can be omitted due to optimization or when a
           tail/sibling call is proven to never return. */
        insert_prologue();

        dbg(PLUGIN_NAME ": %s: %s instrumented.\n", main_input_filename, CURRENT_FN_NAME());
        inst_stat.instrumented++;
    }

    return 0;
}

static void inst_stat_dump(void *event_data, void *data)
{
    FILE *f;
    if (!inst_stat_file)
        return;

    f = fopen(inst_stat_file, "a");
    if (!f) {
        err(PLUGIN_NAME ": Unable to open %s.\n", inst_stat_file);
        return;
    }

    fprintf(f, "%s,%d,%d\n",
            main_input_filename, inst_stat.instrumented, inst_stat.total);

    fclose(f);
}

int plugin_init(struct plugin_name_args *info, struct plugin_gcc_version *ver)
{
    struct register_pass_info pass_inst = {
        .pass = &inst_pass,
        .reference_pass_name = "*free_cfg", // Insert after the first instance
        .ref_pass_instance_number = 1,      // of CFG cleanup pass.
        .pos_op = PASS_POS_INSERT_AFTER,
    };

    struct register_pass_info pass_init = {
        .pass = &init_pass,
        .reference_pass_name = "fixup_cfg",
        .ref_pass_instance_number = 1,
        .pos_op = PASS_POS_INSERT_BEFORE,
    };

    if (strncmp(PLUGIN_GCC_REQ, ver->basever, sizeof(PLUGIN_GCC_REQ))) {
        err(PLUGIN_NAME ": GCC %s required.\n", PLUGIN_GCC_REQ);
        return 1;
    }

    // Parse arguments
    for (int i = 0; i < info->argc; i++) {
        const char *key = info->argv[i].key;
        const char *value = info->argv[i].value;

        if (!strcmp(key, "scope")) {
            global_scope = str_scope(value);
            if (global_scope == -1) {
                err(PLUGIN_NAME ": Invalid scope '%s'.\n", value);
                return 1;
            }
        } else if (!strcmp(key, "init")) {
            if (value[0])
                init_function = value;
        } else if (!strcmp(key, "dump")) {
            if (value[0])
                inst_stat_file = value;
        } else if (!strcmp(key, "leaf")) {
            if (TOLOWER(value[0]) == 'y')
                include_leaf = true;
            else if (TOLOWER(value[0]) == 'n')
                include_leaf = false;
            else {
                err(PLUGIN_NAME ": Unknown value for '%s'.\n", key);
                return 1;
            }
        } else {
            err(PLUGIN_NAME ": Unknown argument '%s'.\n", key);
            return 1;
        }
    }

    // Disable incompatible optimizations.  Multiple epilogues cause the code
    // size to inflate too much, the optimization value is questionable:
    flag_reorder_blocks_and_partition = 0;
    flag_reorder_blocks = 0;

    // Temporary registers might get clobbered before prologue in case of
    // delayed frame setup:
    flag_shrink_wrap = 0;

    // Note that tail and sibling call optimization inserts additional epilogues
    // too: flag_optimize_sibling_calls;

    // Register info about this plugin.
    register_callback(PLUGIN_NAME, PLUGIN_INFO, NULL, &inst_plugin_info);

    // Get called at attribute registration.
    register_callback(PLUGIN_NAME, PLUGIN_ATTRIBUTES, register_attributes, NULL);
    // Add our pass into the pass manager.
    register_callback(PLUGIN_NAME, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_inst);
    // Save statistics at the end.
    register_callback(PLUGIN_NAME, PLUGIN_FINISH_UNIT, inst_stat_dump, NULL);

    if (init_function)
        register_callback(PLUGIN_NAME, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_init);

    return 0;
}
