#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "peppapeg/peppa.h"
#include "argparse/argparse.h"

#define ENUMSTRFILE "./enumstr.peg"

const char* _progname = ""; /** global program name for debug/warn/panic */

/**
 * @brief Returns the basename of the unix path.
 */
const char* basename(const char* path)
{
    char* s = strrchr(path, '/');
    return s ? s + 1 : path;
}

/**
 * @brief Sets the global Prog Name as the basename of the given path.
 */
void SetProgName(const char* path)
{
    _progname = basename(path);
}

/**
 * @brief Prints the printf style message to the stderr and exit the program
 * with error.
 */
void Panic(const char* fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    fprintf(stderr, "[%s] Panic: %s\n", _progname, buffer);
    exit(1);
}

/**
 * @brief Prints the printf style message to the stderr.
 */
void Warn(const char* fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    fprintf(stderr, "[%s] Warning: %s\n", _progname, buffer);
}

enum _dbg_levels {
    DBG_NONE,
    DBG_CRIT,
    DBG_MIN,
    DBG_INFO,
    DBG_VERB,
    DBG_MAX,

    DBG_LEVELS
};

char* _str_dbg_levels[] = {
        [DBG_NONE] "NONE" /* not to be used*/,
        [DBG_CRIT] "CRIT",
        [DBG_MIN] "MIN",
        [DBG_INFO] "INFO",
        [DBG_VERB] "VERB",
        [DBG_MAX] "MAX",
};

char* dbg_str(enum _dbg_levels level)
{
    if (level < 0 || level >= DBG_LEVELS) {
        return "<ERROR>";
    }
    return _str_dbg_levels[level];
}

int _debug_level = DBG_INFO;

/**
 * @brief Prints the printf style message to the stderr.
 */
void Debug(int level, const char* fmt, ...)
{
    if (level > _debug_level)
        return;

    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    fprintf(stderr, "[%s] %s\n", dbg_str(level), buffer);
}

#define _DEBUG_LEVEL(level, fmt, ...) Debug(level, "[%s:%d %s] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define DEBUG_CRIT(fmt, ...) _DEBUG_LEVEL(DBG_CRIT, fmt, ##__VA_ARGS__)
#define DEBUG_MIN(fmt, ...) _DEBUG_LEVEL(DBG_MIN, fmt, ##__VA_ARGS__)
#define DEBUG_INFO(fmt, ...) _DEBUG_LEVEL(DBG_INFO, fmt, ##__VA_ARGS__)
#define DEBUG_VERB(fmt, ...) _DEBUG_LEVEL(DBG_VERB, fmt, ##__VA_ARGS__)
#define DEBUG_MAX(fmt, ...) _DEBUG_LEVEL(DBG_MAX, fmt, ##__VA_ARGS__)

#define ASSERT(cond, fmt, ...)         \
    do {                               \
        if (!(cond)) {                 \
            Panic(fmt, ##__VA_ARGS__); \
        }                              \
    } while (0)

#define ASSERT_STR(s, expected) ASSERT((!strcmp((s), (expected))), "%s != %s", (s), (expected))
#define ASSERT_INT(v, expected, fmt, ...) \
    ASSERT((v) == (expected), "%lld != %lld", (long long)(v), (long long)(expected))
#define ASSERT_NE_INT(v, expected, fmt, ...) \
    ASSERT((v) != (expected), "%lld != %lld", (long long)(v), (long long)(expected))

/**
 * @brief Read nbytes from the an opened file to the given buf.
 *
 * @param fd - opended file
 * @param buf - buffer large enough for nbytes
 * @param nbytes
 * @return ssize_t - nbytes on success, 0 if EOF is encounted, and -1 on errors.
 *
 * @attention buf content may be modified even if NRead didn't complete.
 */
ssize_t NRead(int fd, void* buf, size_t nbytes)
{
    ssize_t left = nbytes;
    while (left > 0) {
        int n = read(fd, buf, nbytes);
        if (n <= 0) {
            return n;  // 0 == EOF, -1 == error
        }
        left -= n;
        buf = (void*)((char*)buf + n);
    }
    return nbytes;
}

#define ERRSTRSZ 256
/**
 * @brief A standard error string object.
 */
typedef struct ErrStr {
    char str[ERRSTRSZ];
} ErrStr;

/**
 * @brief Formats a message and sets errstr.
 */
void* FormatErrStr(ErrStr* errstr, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(errstr->str, sizeof(errstr->str), fmt, args);
    va_end(args);
    return NULL;
}

/**
 * @brief Reads a file and returns its content.
 *
 * @param path - the unix path to the file
 * @param errstr - ErrStr structure. Modified only on failures
 * @return on success a malloced buffer with the content. NULL, on failure
 * @attention the returned buffer needs to be free() by the caller.
 */
char* ReadFile(const char* path, ErrStr* errstr)
{
    DEBUG_INFO("path '%s'", path);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return FormatErrStr(errstr, "can't open '%s' - %m", path);
    }

    struct stat sbuf;
    fstat(fd, &sbuf);
    off_t size = sbuf.st_size;

    char* buf = malloc(size);
    if (!buf) {
        return FormatErrStr(errstr, "can't open alloc buffer for '%s' size %lld", path, size);
    }

    if (NRead(fd, buf, size) != size) {
        return FormatErrStr(errstr, "can't read grammer file '%s' - %m", path);
    }
    return buf;
}

/*********************************************/
/* EnumStr specifics                         */
/*********************************************/

typedef struct EnumParams {
    char *array_prefix;
    bool is_header;
    int max_enum_value;
    char* str_fn;
    char* fn_prefix;
    bool dump_enum;
} EnumParams;

char* _def_str_function =
        "\n\
\n\
char *_EnumStr(char **arr, int val, int max) { \n\
    if (val < 0 || val >= max) {        \n\
        return \"<ERROR>\";             \n\
    }                                   \n\
    if (arr[val] == (void*)0) {         \n\
        return \"<?>\";                 \n\
    }                                   \n\
    return arr[val];                    \n\
}                                       \n\
\n\
";

/**
 * @brief Copy the nodes text to the given buffer.
 * @details Assume buf is zero terminated. Buf must be at least one byte in
 * size.
 */
char* CopyNodeText(P4_Node* node, char* buf, int nbytes)
{
    if (!node) {
        buf[0] = 0;
        return buf;
    }
    int b = node->slice.stop.pos - node->slice.start.pos;
    if (b > nbytes - 1) {
        b = nbytes - 1;
    }
    strncpy(buf, node->text + node->slice.start.pos, b);
    buf[b] = 0;
    return buf;
}

/**
 * @brief Formats a Node debug string.
 */
char* FormatNode(P4_Node* node, char* buf, int nb)
{
    char tbuf[256];
    snprintf(buf, nb, "Node '%s' [ln:%lu col:%lu] text '%s'", node->rule_name, (node)->slice.start.lineno,
             (node)->slice.start.offset, CopyNodeText(node, tbuf, sizeof(tbuf)));
    return buf;
}

#define PANIC_NODE(node, fmt, ...)                                                                             \
    do {                                                                                                       \
        char __buf[512];                                                                                       \
        Panic("[%s:%d %s] " fmt " <%s>", __FILE__, __LINE__, __func__, FormatNode(node, __buf, sizeof(__buf)), \
              ##__VA_ARGS__);                                                                                  \
    } while (0)

/**
 * @brief Generate an INFO DEBUG line.
 */
void DebugNode(char* msg, P4_Node* node)
{
    char buf[256];
    DEBUG_INFO("%s: %s\n", msg, FormatNode(node, buf, sizeof(buf)));
}

/**
 * @brief Emits a lookup function for the given enum_node.
 * @details Emits a function declaration if header, eitherwise the full function.
 */
void EmitFunction(P4_Node* enum_node, char* identifier, FILE* file)
{
    EnumParams* params = (EnumParams*)enum_node->userdata;

    bool is_type = !strcmp(enum_node->rule_name, "enum_type");

    fprintf(file, "char *%s%s(%s%s val)", params->fn_prefix, identifier, is_type ? "" : "enum ", identifier);
    if (params->is_header) {
        fprintf(file, ";\n");
    } else {
        fprintf(file, "{ return %s(%s%s, (int)val, sizeof(%s%s) / sizeof(char*)); }\n", params->str_fn,
                params->array_prefix, identifier, params->array_prefix, identifier);
    }
}

/**
 * @brief Emits an enum item string line.
 *
 * @param root - enum_value node.
 * @param file - file to output line to.
 * @param value - pointer to last value seen in enum. If this item do not
 * contain explicit value then the *value +1 is used. Anyhow *value is set to
 * the current used value.
 * @return P4_Error
 * @attention values bigger then params->max_enum_value are ignored.
 */
P4_Error EmitEnumItem(P4_Node* root, FILE* file, int* value)
{
    // enum_value = (identifier EQ integer) / identifier;
    char buf[256];

    DEBUG_INFO("<");
    ASSERT_STR(root->rule_name, "enum_value");
    EnumParams* params = (EnumParams*)root->userdata;
    P4_Node* node = root->head;

    if (params->is_header) {
        return P4_Ok;
    }

    DebugNode("item ident", node);
    if (strcmp(node->rule_name, "identifier")) {
        PANIC_NODE(node, "Enum item without a name");
    }
    char* name = CopyNodeText(node, buf, sizeof(buf));

    node = node->next;
    if (!node) {
        ++*value;
    } else {
        char val[256];
        node = node->next;  // skip '='
        *value = atoi(CopyNodeText(node, val, sizeof(buf)));
    }

    if (*value >= 0 && *value < params->max_enum_value) {
        fprintf(file, "\t[%d] \"%s (%d)\",\n", *value, name, *value);
    }
    DEBUG_INFO(">");
    return P4_Ok;
}

/**
 * @brief Emits a enum string array containing all enum items in the list.
 *
 * @param identifier - name of the enum / enum def
 * @param root - enum_list node
 * @param file - file to emit to
 * @param errstr
 * @return P4_Error
 */
P4_Error EmitEnumList(char* identifier, P4_Node* root, FILE* file, ErrStr* errstr)
{
    // enum_list = enum_value (COMMA enum_value)* COMMA?;

    DEBUG_INFO("< root %p", root);
    EnumParams* params = (EnumParams*)root->userdata;
    int value = 0;

    if (params->is_header) {
        return P4_Ok;
    }

    fprintf(file, "char *%s%s[] = {\n", params->array_prefix, identifier);

    for (P4_Node* node = root->head; node; node = node->next) {
        if (!strcmp(node->rule_name, "enum_value")) {
            EmitEnumItem(node, file, &value);
        }
        // Skip 'COMMA'
    }

    fprintf(file, "}; \t// %s%s\n\n", params->array_prefix, identifier);

    DEBUG_INFO(">");
    return P4_Ok;
}

/**
 * @brief Emit the original enum. Probably for debugging.
 */
void EmitEnums(P4_Node* node, FILE* file)
{
    char* copy = P4_CopyNodeString(node);
    fputs(copy, file);
    fputs("\n\n", file);
    free(copy);
}

/**
 * @brief Processes a single enum_type node.
 *
 * @param root - enum_type node
 * @param file - the file to emit to
 * @param errstr
 * @return P4_Error
 */
P4_Error ProcessEnumType(P4_Node* root, FILE* file, ErrStr* errstr)
{
    // enum_type = "typedef" "enum" identifier? LBRACE enum_list? RBRACE
    // identifier SEMI;
    EnumParams* params = root->userdata;
    P4_Node* node = root->head;
    P4_Node* list = NULL;

    DEBUG_INFO("<");
    ASSERT_STR(root->rule_name, "enum_type");

    if (!strcmp(node->rule_name, "identifier")) {
        // Skip Enum Identifier, as we need to use the type identifier
        node = node->next;
    }

    if (strcmp(node->rule_name, "LBRACE")) {
        PANIC_NODE(node, "Enum type without '{'");
    }

    node = node->next;
    if (!strcmp(node->rule_name, "enum_list")) {
        list = node;
        node = node->next;
    } else {
        char buf[256];
        Warn("Skip empty enum list <%s>", FormatNode(root, buf, sizeof(buf)));
    }

    if (strcmp(node->rule_name, "RBRACE")) {
        PANIC_NODE(node, "Enum type without '}'");
    }

    node = node->next;
    char buf[256];
    char* identifier = CopyNodeText(node, buf, sizeof(buf));
    if (strcmp(node->rule_name, "identifier")) {
        PANIC_NODE(node, "Enum type without type identifier");
    }

    node = node->next;
    if (strcmp(node->rule_name, "SEMI")) {
        PANIC_NODE(node, "Enum type without ';'");
    }
    if (!list) {
        DEBUG_INFO("(skip)>");
        return P4_Ok;
    }
    if (params->dump_enum) {
        EmitEnums(root, file);
    }
    P4_Error err = EmitEnumList(identifier, list, file, errstr);
    if (err == P4_Ok) {
        EmitFunction(root, identifier, file);
    }
    return err;
}

/**
 * @brief Processes a single enum_def node.
 *
 * @param root - enum_type node
 * @param file - the file to emit to
 * @param errstr
 * @return P4_Error
 */
P4_Error ProcessEnumDef(P4_Node* root, FILE* file, ErrStr* errstr)
{
    // enum_def = "enum" identifier? LBRACE enum_list? RBRACE identifier? SEMI;

    DEBUG_INFO("<");
    EnumParams* params = root->userdata;
    P4_Node* node = root->head;
    P4_Node* list = NULL;

    if (strcmp(node->rule_name, "identifier")) {
        char buf[256];
        Warn("Skip enum def without name <%s>]", FormatNode(node, buf, sizeof(buf)));
        DEBUG_INFO("(skip)>");
        return P4_Ok;
    }
    char buf[256];
    char* identifier = CopyNodeText(node, buf, sizeof(buf));

    node = node->next;
    if (strcmp(node->rule_name, "LBRACE")) {
        Panic("Enum def without '{' pos[%d-%d]", root->slice.start, root->slice.stop);
    }

    node = node->next;
    if (!strcmp(node->rule_name, "enum_list")) {
        list = node;
    }

    node = node->next;
    if (strcmp(node->rule_name, "RBRACE")) {
        Panic("Enum def without '}' pos[%d-%d]", root->slice.start, root->slice.stop);
    }

    node = node->next;
    if (strcmp(node->rule_name, "SEMI")) {
        // Panic("Enum def without ';' pos[%d:%d]", root->slice.start.lineno,
        // root->slice.start.offset);
        PANIC_NODE(root, "Enum def without ';'");
    }
    if (!list) {
        DEBUG_INFO("(skip)>");
        return P4_Ok;
    }
    if (params->dump_enum) {
        EmitEnums(root, file);
    }
    P4_Error err = EmitEnumList(identifier, list, file, errstr);
    if (err == P4_Ok) {
        EmitFunction(root, identifier, file);
    }
    return err;
}

/**
 * @brief Processes all root enums under the root node
 *
 * @param root - root node
 * @param file - to emit to
 * @param errstr
 * @return P4_Error
 */
P4_Error ProcessRootEnums(P4_Node* root, FILE* file, ErrStr* errstr)
{
    // enum_value = (identifier EQ integer) / identifier;

    DEBUG_INFO("<");
    P4_Error error = P4_Ok;

    for (P4_Node* node = root->head; node; node = node->next) {
        char buf[256];
        DEBUG_INFO("Root child: rule '%s' text '%s'", node->rule_name, CopyNodeText(node, buf, sizeof(buf)));
        if (!strcmp(node->rule_name, "enum_def")) {
            if ((error = ProcessEnumDef(node, file, errstr))) {
                DEBUG_INFO("(error)>");
                return error;
            }
        } else if (!strcmp(node->rule_name, "enum_type")) {
            if ((error = ProcessEnumType(node, file, errstr))) {
                DEBUG_INFO("(error)>");
                return error;
            }
        } else {
            // Ignore
        }
    }
    DEBUG_INFO(">");
    return 0;
}

/**
 * @brief Returns a normalized copy of the header string.
 * @details Converts '/' and '.' to '_', and copy/convert all alphanum to Upper.
 * @param header 
 * @return a new string to be free() by the caller.
 */
char* NormalizeHeaderCopy(char* header)
{
    char* new_header = malloc(strlen(header) + 1);
    char* n = new_header;

    for (char* c = header; *c; c++) {
        if (*c == '/' || *c == '.') {
            *n++ = '_';
        } else if isalnum (*c) {
            *n++ = toupper(*c);
        }
    }
    *n = 0;
    return new_header;
}

/**
 * @brief Emit start section text.
 */
void EmitStart(EnumParams* params, char* header, FILE* file)
{

    if (header) {
        header = NormalizeHeaderCopy(header);
        fprintf(file, "// Auto generated header for enum strings\n");
        fprintf(file, "#ifndef __%s\n", header);
        fprintf(file, "#define __%s\n", header);
        free(header);
    } else {
        fprintf(file, "// Start of generated enum strings section\n");
        fprintf(file, "char *%s(char **arr, int val, int max);\n", params->str_fn);
    }
}

/**
 * @brief Emit end of section text.
 */
void EmitEnd(char* header, FILE* file)
{
    if (header) {
        header = NormalizeHeaderCopy(header);
        fprintf(file, "#endif   // %s\n", header);
        free(header);
    } else {
        fprintf(file, "// End of generated enum strings section\n");
    }
}

/** Emit the default str function. */
void EmitStrFunction(FILE* file)
{
    fputs(_def_str_function, file);
}

/**
 * @brief Sets the User Data object field in the given node.
 * @details This is a callback function to be used by P4_InspectSourceAst() (see
 * main).
 */
P4_Error SetUserData(P4_Node* node, void* userdata)
{
    node->userdata = userdata;
    return P4_Ok;
}

P4_Grammar* InitGrammar(EnumParams* params, FILE* out)
{
    ErrStr errstr;
    char* buf = ReadFile(ENUMSTRFILE, &errstr);
    if (!buf) {
        Panic("Can't read grammer file: %s", errstr.str);
    }

    P4_Grammar* grammar = P4_LoadGrammar(buf);

    P4_SetUserDataFreeFunc(grammar, NULL);  // don't free user data == params on stack

    return grammar;
}

void ParseFile(const char* srcfile, P4_Grammar* grammar, EnumParams* params, FILE* out)
{
    ErrStr errstr;
    P4_Error error = P4_Ok;

    char* srcbuf = ReadFile(srcfile, &errstr);
    if (!srcbuf) {
        Panic("Can't read src file: %s", errstr.str);
    }
    P4_Source* source = P4_CreateSource(srcbuf, "SourceFile");
    if ((error = P4_Parse(grammar, source)) != P4_Ok) {
        Panic("parse: %s", P4_GetErrorMessage(source));
    } else if ((error = P4_InspectSourceAst(P4_GetSourceAst(source), &params, SetUserData)) != P4_Ok) {
        Panic("SerUserData failed");
    } else if ((error = ProcessRootEnums(P4_GetSourceAst(source), out, &errstr)) != P4_Ok) {
        Panic("eval: %d", error);
    }
    P4_DeleteSource(source);
}

void DoneParsing(EnumParams* params)
{
}

EnumParams _def_enum_params = {
        .array_prefix = "_str_",
        .is_header = 0,
        .max_enum_value = 1024,
        .str_fn = "_EnumStr",  // (char*)(char **arr, int val, int max)
        .fn_prefix = "EnumStr_",
        .dump_enum = 1,
};

int InitArguments(EnumParams* params, int argc, const char **argv)
{
    struct argparse_option options[] = {
            OPT_HELP(),
            OPT_STRING('A', "array_prefix", &params->array_prefix, "prefix of per enum string array", NULL, 0, 0),
            OPT_STRING('S', "strfn", &params->str_fn, "internal enum string function to use", NULL, 0, 0),
            OPT_STRING('F', "strfn_prefix", &params->fn_prefix, "prefix of per enum string function", NULL, 0, 0),
            OPT_BOOLEAN('H', "header", &params->is_header, "emit headers only", NULL, 0, 0),
            OPT_BOOLEAN('E', "dump_enums", &params->dump_enum, "dump the source enums", NULL, 0, 0),
            OPT_INTEGER('M', "max_enum", &params->max_enum_value, NULL, 0, 0),

            OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argparse_describe(&argparse, "\nA brief description of what the program does and how it works.",
                      "\nAdditional description of the program after the description of the arguments.");
    argc = argparse_parse(&argparse, argc, argv);

    printf("A %s\n", params->array_prefix);
    printf("S %s\n", params->str_fn);
    printf("F %s\n", params->fn_prefix);

    return argc;
}

int main(int argc, const char** argv)
{
    SetProgName(argv[0]);
    EnumParams params = _def_enum_params;
    FILE* out = stdout;

    argc = InitArguments(&params, argc, argv);

    if (argc < 1) {
        Panic("At least one source file must be specified.");
    }

    DEBUG_INFO("Start (argc %d)", argc);
    for (int i; i < argc; i++) {
        printf("%d %s\n", i, argv[i]);
    }
    exit(0);

    P4_Grammar* grammar = InitGrammar(&params, out);

    char* header = NULL;  //"/tmp/test.h";
    params.is_header = header != NULL;
    EmitStart(&params, header, out);

    if (!params.is_header) {
        EmitStrFunction(out);
    }

    for (int i = 0; i < argc; i++) {
        ParseFile(argv[i], grammar, &params, out);
    }
    EmitEnd(header, out);

    P4_DeleteGrammar(grammar);
    DEBUG_INFO("Done.");
    return 0;
}
