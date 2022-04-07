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

#define PLUGIN_NAME "pac_sw_plugin"
#define PLUGIN_VERSION "0.1"
#define PLUGIN_HELP PLUGIN_NAME ": Instrumentation for Software-Emulated PAC."
#define PLUGIN_GCC_REQ "10.2.1" // Required GCC version

#define EXCLUDE_ATTR "pac_exclude"

#define err(fmt, ...) fprintf(stderr, PLUGIN_NAME ": " fmt "\n", ##__VA_ARGS__)
#ifdef DEBUG
#define dbg(fmt, ...) fprintf(stderr, PLUGIN_NAME ": " fmt "\n", ##__VA_ARGS__)
#else
#define dbg(fmt, ...) ((void) 0)
#endif

int plugin_is_GPL_compatible;
static struct plugin_info inst_plugin_info = {
    .version  = PLUGIN_VERSION,
    .help     = PLUGIN_HELP,
};

enum { PROLOGUE, EPILOGUE };

char *prologue_s = NULL;
char *epilogue_s = NULL;
char *init_function = NULL;

extern gcc::context *g;

static unsigned int execute_inst_pac(void);
static unsigned int execute_init_pac(void);

static tree handle_exclude_attr(tree *node, tree name, tree args,
                                int flags, bool *no_add_attrs)
{
    return NULL_TREE;
}

// Structure describing an attribute and a function to handle it.
static struct attribute_spec exclude_attr =
{
    .name = EXCLUDE_ATTR,
    .handler = handle_exclude_attr,
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
pass_inst_pac ins_pass = pass_inst_pac(g);

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
pass_init_pac init_pass = pass_init_pac(g);

// Plugin callback called during attribute registration.
static void register_attributes(void *event_data, void *data)
{
    register_attribute(&exclude_attr);
    dbg("Registered attribute '%s'.", EXCLUDE_ATTR);
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

    dbg("Attached %s to %s.", init_function, fn_name);

    return 0;
}

// Recursively check for existence of protected declarations (arrays).
static bool has_protected_decls(tree decl_initial)
{
    if (!decl_initial)
        return false;

    if (TREE_CODE(decl_initial) == VAR_DECL) {
        if (TREE_CODE(TREE_TYPE(decl_initial)) == ARRAY_TYPE)
            return true;
        else
            return has_protected_decls(DECL_CHAIN(decl_initial));
    }

    if (TREE_CODE(decl_initial) == BLOCK)
        return (has_protected_decls(BLOCK_VARS(decl_initial)) ||
                has_protected_decls(BLOCK_CHAIN(decl_initial)) ||
                has_protected_decls(BLOCK_SUBBLOCKS(decl_initial)));

    return false;
}

// Take a string and expand it as ASM code at loc in prologue/epilogue.
static void expand_asm(tree string, int vol, rtx_insn *loc, int position)
{
    rtx body;

    body = gen_rtx_ASM_INPUT_loc(VOIDmode, ggc_strdup(TREE_STRING_POINTER(string)), INSN_LOCATION(loc));

    MEM_VOLATILE_P(body) = vol;

    // Non-empty basic ASM implicitly clobbers memory.
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
            targetm.md_asm_adjust(output_rvec, input_rvec, constraints, clobber_rvec, clobbered_regs);

        asm_op = body;
        nclobbers = clobber_rvec.length();
        body = gen_rtx_PARALLEL(VOIDmode, rtvec_alloc(1 + nclobbers));

        XVECEXP(body, 0, 0) = asm_op;
        for (i = 0; i < nclobbers; i++)
            XVECEXP(body, 0, i + 1) = gen_rtx_CLOBBER(VOIDmode, clobber_rvec[i]);
    }

    switch (position) {
    case PROLOGUE:
        emit_insn_before(body, get_first_nonnote_insn());
        break;
    case EPILOGUE:
        emit_insn_before(body, PREV_INSN(get_last_nonnote_insn()));
        break;
    default:
        abort();
    }
}

static void insert_prologue(void)
{
    tree string = build_string(strlen(prologue_s), prologue_s);
    basic_block bb = ENTRY_BLOCK_PTR_FOR_FN(cfun)->next_bb;

    expand_asm(string, 1, BB_END(bb), PROLOGUE);
}

static void insert_epilogue(void)
{
    tree string = build_string(strlen(epilogue_s), epilogue_s);
    basic_block bb = ENTRY_BLOCK_PTR_FOR_FN(cfun)->next_bb;

    expand_asm(string, 1, BB_END(bb), EPILOGUE);
}

// For each function lookup attributes and attach instrumentation.
static unsigned int execute_inst_pac(void)
{
    const char *fn_name = IDENTIFIER_POINTER(DECL_NAME(current_function_decl));

    tree attrlist = DECL_ATTRIBUTES(current_function_decl);
    tree attr = lookup_attribute(EXCLUDE_ATTR, attrlist);

    if (init_function && (strcmp(fn_name, init_function) == 0 ||
                          strcmp(fn_name, "main") == 0)) {
        dbg("%s: Excluded due to init function.", fn_name);
        return 0;
    }

    // Do not instrument the function if the exclude attribute is present
    if (attr != NULL_TREE) {
        dbg("%s: Excluded via attribute.", fn_name);
        return 0;
    }

    // Do not instrument if no stack overflow is possible
    if (!has_protected_decls(DECL_INITIAL(current_function_decl))) {
        dbg("%s: No protected declarations.", fn_name);
        return 0;
    }

    insert_prologue();
    insert_epilogue();

    dbg("%s: Successfully instrumented.", fn_name);

    return 0;
}

static char *read_code(const char *file)
{
    char *code;
    size_t fsize;
    FILE *f = fopen(file, "r");
    if (!f) {
        err("Cannot open %s.", file);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    rewind(f);

    code = (char *) xmalloc(fsize+1);
    if (fread(code, 1, fsize, f) != fsize) {
        err("Unable to read %s.", file);
        free(f);
        fclose(f);
        return NULL;
    }

    code[fsize] = 0;
    return code;
}

int plugin_init(struct plugin_name_args *info, struct plugin_gcc_version *ver)
{
    struct register_pass_info pass_inst = {
        .pass = &ins_pass,
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
        err("Incompatible GCC version.");
        return 1;
    }

    for (int i = 0; i < info->argc; i++) {
        if (strcmp(info->argv[i].key, "prologue") == 0)
            prologue_s = read_code(info->argv[i].value);
        else if (strcmp(info->argv[i].key, "epilogue") == 0)
            epilogue_s = read_code(info->argv[i].value);
        else if (strcmp(info->argv[i].key, "init-function") == 0)
            init_function = info->argv[i].value;
    }

    if (!prologue_s) {
        err("Unable fetch prologue source file.");
        return 1;
    }

    if (!epilogue_s) {
        err("Unable fetch epilogue source file.");
        return 1;
    }

    // Register info about this plugin.
    register_callback(PLUGIN_NAME, PLUGIN_INFO, NULL, &inst_plugin_info);
    // Get called at attribute registration.
    register_callback(PLUGIN_NAME, PLUGIN_ATTRIBUTES, register_attributes, NULL);
    // Add our pass into the pass manager.
    register_callback(PLUGIN_NAME, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_inst);

    if (init_function)
        register_callback(PLUGIN_NAME, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_init);

    dbg("Version " PLUGIN_VERSION " loaded.");

    return 0;
}
