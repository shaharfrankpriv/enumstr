#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libbasic.h>
#include <argparse.h>
#include <peppa.h>

#define ENUMSTRFILE "./enumstr.peg"

/* The following is the enumstr.peg file translated into a c string file */
char* enumpeg = ""
#include "enumpeg.h"
        ;

/*********************************************/
/* EnumStr specifics                         */
/*********************************************/

typedef struct EnumParams {
    char* array_prefix;
    char* header;
    int max_enum_value;
    char* str_fn;
    char* fn_prefix;
    bool dump_enum;
    bool reuse;  // reuse str function
    bool skip_includes;
    char* use_header;  // to be added to includes
    char* grammar;     // override over the internal one
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
        lbPanic("[%s:%d %s] " fmt " <%s>", __FILE__, __LINE__, __func__, FormatNode(node, __buf, sizeof(__buf)), \
              ##__VA_ARGS__);                                                                                  \
    } while (0)

/**
 * @brief Generate an INFO DEBUG line.
 */
void DebugNode(char* msg, P4_Node* node)
{
    char buf[256];
    lb_DEBUG_INFO("%s: %s\n", msg, FormatNode(node, buf, sizeof(buf)));
}

/**
 * @brief Emits a lookup function for the given enum_node.
 * @details Emits a function declaration if header, eitherwise the full function.
 */
void EmitFunction(P4_Node* enum_node, char* identifier, FILE* out)
{
    EnumParams* params = (EnumParams*)enum_node->userdata;

    bool is_type = !strcmp(enum_node->rule_name, "enum_type");

    fprintf(out, "char *%s%s(%s%s val)", params->fn_prefix, identifier, is_type ? "" : "enum ", identifier);
    if (params->header) {
        fprintf(out, ";\n");
    } else {
        fprintf(out, "{ return %s(%s%s, (int)val, sizeof(%s%s) / sizeof(char*)); }\n", params->str_fn,
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

    lb_DEBUG_INFO("<");
    lb_ASSERT_STR(root->rule_name, "enum_value");
    EnumParams* params = (EnumParams*)root->userdata;
    P4_Node* node = root->head;

    if (params->header) {
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
    lb_DEBUG_INFO(">");
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

    lb_DEBUG_INFO("< root %p", root);
    EnumParams* params = (EnumParams*)root->userdata;
    int value = -1;     // such that first enum without a value will be 0

    if (params->header) {
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

    lb_DEBUG_INFO(">");
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

    lb_DEBUG_INFO("<");
    lb_ASSERT_STR(root->rule_name, "enum_type");

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
        lbWarn("Skip empty enum list <%s>", FormatNode(root, buf, sizeof(buf)));
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
        lb_DEBUG_INFO("(skip)>");
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

    lb_DEBUG_INFO("<");
    EnumParams* params = root->userdata;
    P4_Node* node = root->head;
    P4_Node* list = NULL;

    if (strcmp(node->rule_name, "identifier")) {
        char buf[256];
        lbWarn("Skip enum def without name <%s>]", FormatNode(node, buf, sizeof(buf)));
        lb_DEBUG_INFO("(skip)>");
        return P4_Ok;
    }
    char buf[256];
    char* identifier = CopyNodeText(node, buf, sizeof(buf));

    node = node->next;
    if (strcmp(node->rule_name, "LBRACE")) {
        lbPanic("Enum def without '{' pos[%d-%d]", root->slice.start, root->slice.stop);
    }

    node = node->next;
    if (!strcmp(node->rule_name, "enum_list")) {
        list = node;
    }

    node = node->next;
    if (strcmp(node->rule_name, "RBRACE")) {
        lbPanic("Enum def without '}' pos[%d-%d]", root->slice.start, root->slice.stop);
    }

    node = node->next;
    if (strcmp(node->rule_name, "SEMI")) {
        // Panic("Enum def without ';' pos[%d:%d]", root->slice.start.lineno,
        // root->slice.start.offset);
        PANIC_NODE(root, "Enum def without ';'");
    }
    if (!list) {
        lb_DEBUG_INFO("(skip)>");
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

    lb_DEBUG_INFO("<");
    P4_Error error = P4_Ok;

    for (P4_Node* node = root->head; node; node = node->next) {
        char buf[256];
        lb_DEBUG_INFO("Root child: rule '%s' text '%s'", node->rule_name, CopyNodeText(node, buf, sizeof(buf)));
        if (!strcmp(node->rule_name, "enum_def")) {
            if ((error = ProcessEnumDef(node, file, errstr))) {
                lb_DEBUG_INFO("(error)>");
                return error;
            }
        } else if (!strcmp(node->rule_name, "enum_type")) {
            if ((error = ProcessEnumType(node, file, errstr))) {
                lb_DEBUG_INFO("(error)>");
                return error;
            }
        } else {
            // Ignore
        }
    }
    lb_DEBUG_INFO(">");
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
void EmitStart(EnumParams* params, FILE* out)
{

    if (params->header) {
        char* header = NormalizeHeaderCopy(params->header);
        fprintf(out, "// Auto generated header for enum strings\n");
        fprintf(out, "#ifndef __%s\n", header);
        fprintf(out, "#define __%s\n", header);
        fprintf(out, "char *%s(char **arr, int val, int max);\n", params->str_fn);
        free(header);
    } else {
        fprintf(out, "// Start of generated enum strings section\n");
        if (params->use_header) {
            fprintf(out, "#include \"%s\"\n", params->use_header);
        }
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
    char* buf = enumpeg;

    if (params->grammar) {
        buf = lbReadFile(params->grammar, &errstr);
        if (!buf) {
            lbPanic("Can't read grammer file: %s", errstr.str);
        }
    }

    P4_Grammar* grammar = P4_LoadGrammar(buf);

    P4_SetUserDataFreeFunc(grammar, NULL);  // don't free user data == params on stack

    return grammar;
}

void ParseFile(const char* srcfile, P4_Grammar* grammar, EnumParams* params, FILE* out)
{
    ErrStr errstr;
    P4_Error error = P4_Ok;

    char* srcbuf = lbReadFile(srcfile, &errstr);
    if (!srcbuf) {
        lbPanic("Can't read src file: %s", errstr.str);
    }
    P4_Source* source = P4_CreateSource(srcbuf, "SourceFile");
    if ((error = P4_Parse(grammar, source)) != P4_Ok) {
        lbPanic("parse: %s <\n%s>", P4_GetErrorMessage(source), srcbuf);
    } else if ((error = P4_InspectSourceAst(P4_GetSourceAst(source), params, SetUserData)) != P4_Ok) {
        lbPanic("SerUserData failed");
    } else if ((error = ProcessRootEnums(P4_GetSourceAst(source), out, &errstr)) != P4_Ok) {
        lbPanic("eval: %d", error);
    }
    P4_DeleteSource(source);
}

void EmitIncludes(const char** sources, int nsources, FILE* out)
{
    for (int i = 0; i < nsources; i++) {
        fprintf(out, "#include \"%s\"\n", sources[i]);
    }
}

EnumParams _def_enum_params = {
        .array_prefix = "_str_",
        .header = NULL,
        .max_enum_value = 1024,
        .str_fn = "_EnumStr",  // (char*)(char **arr, int val, int max)
        .fn_prefix = "EnumStr_",
        .dump_enum = 0,
        .reuse = 0,
        .skip_includes = 0,
        .use_header = NULL,
        .grammar = NULL,
};

int InitArguments(EnumParams* params, int argc, const char** argv)
{
    char debug_buf[64], *debug_str = debug_buf;
    strcpy(debug_str, lbDbgLevelStr(DBG_CRIT));

    int use_debug = 0;

    struct argparse_option options[] = {
            OPT_HELP(),
            OPT_STRING('G', "grammar", &params->grammar, "override the internal peg grammer file", NULL, 0, 0),
            OPT_STRING('A', "array_prefix", &params->array_prefix, "prefix of per enum string array", NULL, 0, 0),
            OPT_STRING('S', "strfn", &params->str_fn, "internal enum string function to use", NULL, 0, 0),
            OPT_STRING('F', "strfn_prefix", &params->fn_prefix, "prefix of per enum string function", NULL, 0, 0),
            OPT_STRING('U', "use_header", &params->use_header, "include the given header file", NULL, 0, 0),
            OPT_STRING('H', "header", &params->header, "emit header for given name", NULL, 0, 0),
            OPT_BOOLEAN('E', "dump_enums", &params->dump_enum, "dump the source enums", NULL, 0, 0),
            OPT_BOOLEAN('R', "reuse", &params->reuse, "reuse an existing str function, don't emit one", NULL, 0, 0),
            OPT_BOOLEAN('K', "skip_includes", &params->skip_includes, "do not emit include source files", NULL, 0, 0),
            OPT_INTEGER('M', "max_enum", &params->max_enum_value, "ignore enum values above this max", NULL, 0, 0),
            OPT_STRING('D', "debug_level", &debug_str, lb_debug_levels, NULL, 0, 0),
            OPT_BOOLEAN('d', "debug", &use_debug, "show debug messages", NULL, 0, 0),

            OPT_END(),
    };

    struct argparse argparse;
    
    argparse_init(&argparse, options, NULL, 0);
    argparse_describe(&argparse, "\nParse c/cpp files and generate a string array per enum.",
                      "\nExamples:\n"
                      "\t./enumstr --header source.c > enums.h\n"
                      "\t./enumstr --use_header enums.h source.c > enums.c\n");
    argc = argparse_parse(&argparse, argc, argv);

    if (use_debug && lb_debug_level < DBG_INFO) {
        lb_debug_level = DBG_INFO;
    }
    lb_debug_level = lbDbgLevel(debug_str);

    lb_DEBUG_INFO("debug level: \"%s\" (%d)\n", lbDbgLevelStr(lb_debug_level), lb_debug_level);

    if (argc < 1) {
        lbWarn("At least one source file is expected.\n");
        argparse_usage(&argparse);
        exit(1);
    }

    return argc;
}

int main(int argc, const char** argv)
{
    lbSetProgName(argv[0]);
    EnumParams params = _def_enum_params;
    FILE* out = stdout;

    argc = InitArguments(&params, argc, argv);

    lb_DEBUG_INFO("Start (argc %d)", argc);

    P4_Grammar* grammar = InitGrammar(&params, out);

    EmitStart(&params, out);

    if (!params.skip_includes && !params.header) {
        EmitIncludes(argv, argc, out);
    }

    if (!params.reuse && !params.header) {
        EmitStrFunction(out);
    }

    for (int i = 0; i < argc; i++) {
        ParseFile(argv[i], grammar, &params, out);
    }
    EmitEnd(params.header, out);

    P4_DeleteGrammar(grammar);
    lb_DEBUG_INFO("Done.");
    return 0;
}
