#!/usr/bin/python3

#
# Parse c/cpp enum declartions and typedefs and produce matching string arrys, lookup functions and 
# optionally header files.
#
# Based on cpp_enum_parser.py {https://github.com/pyparsing/pyparsing/blob/master/examples/cpp_enum_parser.py}
# And ChunMinChang/remove_c_style_comments.py {https://gist.github.com/ChunMinChang/}
#
#
# Example:
#    -- Generate a c header file for the lookup function for source.c
#    ${prog} --gen_header enumstr.h source.c
#    -- Generate a c enum str body for source.c, make it to include the generated header file
#    ${prog} --use_header enumstr.h source.c
#

from pyparsing import *

import re
import sys


array_prefix = "_strarr_"
enum_fn = "_EnumStr"
fn_prefix = "_str_"
max_enum_value = 256


def remove_comments(text):
    """ remove c-style comments.
        text: blob of text with comments (can include newlines)
        returns: text with comments removed
    """
    def replacer(match):
        s = match.group(0)
        if s.startswith('/'):
            return " " # note: a space and not an empty string
        else:
            return s
    pattern = re.compile(
        r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
        re.DOTALL | re.MULTILINE
    )
    return re.sub(pattern, replacer, text)



# sample string with enums and other stuff
sample = """
    stuff before
    enum Hello {
        Zero,
        One, /* a c sytle comment */
        Two,
        Three, // cpp style comment 
        Five=/* embedded */ 5,
        Six,
        Ten=10,
        Large=10000,    // expected to be skipped    
        };
    in the middle
    typedef enum 
        {
        alpha,
        beta,
        gamma 
        = 10 ,
        zeta = 50
        } Blah;
    typedef enum Kuku
        {
        single
        } Kiki;
    at the end
    """

def emit_sample_main():
    print(f"""
#include <stdio.h>
int main(int argc, char **argv)
{{
    enum Hello h1 = Zero, h2 = Five, h3 = 7, h4 = 11;
    Blah b1 = alpha, b2 = gamma, b3 = -1, b4 = 17;
    Kiki k1 = single;
    printf("\\n## Test enum str utility:\\n\\n");
    printf("# Hello - enum, no type\\n");
    printf("h1 Zero %d - %s\\n", h1, {fn_prefix}Hello(h1));
    printf("h2 Five %d - %s\\n", h2, {fn_prefix}Hello(h2));
    printf("h3 (missing 7) %d - %s\\n", h3, {fn_prefix}Hello(h3));
    printf("h4 (out of range 11) %d - %s\\n", h4, {fn_prefix}Hello(h4));
    printf("# Blah - type enum, no enum name\\n");
    printf("b1 alpha %d - %s\\n", b1, {fn_prefix}Blah(b1));
    printf("b2 gamma %d - %s\\n", b2, {fn_prefix}Blah(b2));
    printf("b3 (out of range -1) %d - %s\\n", b3, {fn_prefix}Blah(b3));
    printf("b4 (missing 17) %d - %s\\n", b4, {fn_prefix}Blah(b4));
    printf("# Kiki - type enum, enum name Kuku\\n");
    printf("k1 single %d - %s\\n", k1, {fn_prefix}Kiki(k1));
    return 0;
}}
""")

# syntax we don't want to see in the final parse tree
LBRACE, RBRACE, EQ, COMMA, SEMI = map(Suppress, "{}=,;")
_enum = Suppress("enum")
_typedef = Suppress("typedef")

identifier = Word(alphas + "_", alphanums + "_")
integer = Word(nums)

enumValue = Group(identifier("name") + Optional(EQ + integer("value")))
enumList = Group(enumValue + ZeroOrMore(COMMA + enumValue) + Optional(COMMA))
enumDef = _enum + identifier("enum") + LBRACE + \
    enumList("names") + RBRACE + Optional(identifier("var"))
enumType = _typedef + _enum + \
    Optional(identifier("enum")) + LBRACE + Optional(enumList("names")) + \
    RBRACE + identifier("enum_type")
enum = (enumDef | enumType) + SEMI

def emit_includes(sources : list, header : str = ""):
    if header:
        print(f'#include "{header}"')
    for s in sources:
        print(f'#include "{s}"')


str_function = """

char *_EnumStr(char **arr, int val, int max) {
    if (val < 0 || val >= max) {
        return "<ERROR>";
    }
    if (arr[val] == (void*)0) {
        return "<?>";
    }
    return arr[val];
}

"""

def emit_str_function(is_header: bool = False):
    if is_header:
        return
    print(str_function)


def start_array(ename : str, is_header: bool = False):
    if is_header:
        return
    print(f"char *{array_prefix}{ename}[] = {{")


def end_array(ename : str, is_header: bool = False):
    if is_header:
        return
    print(f"}};    // end of {array_prefix}{ename}\n")


def emit_string(name : str, value : int, is_header: bool = False):
    if is_header:
        return
    if value >= 0 and value < max_enum_value: 
        print(f'\t[{value}] "{name} ({value})",')


def emit_function(ename : str, is_type: bool, is_header: bool = False):
    print(f'char *{fn_prefix}{ename}({"" if is_type else "enum"} {ename} val)', end="");
    print(";" if is_header else f'{{ return {enum_fn}({array_prefix}{ename}, '
                                f'(int)val, sizeof({array_prefix}{ename}) / sizeof(char*)); }}')


def parse_text(text : str, is_header: bool = False, dump : bool = False):
    # find instances of enums ignoring other syntax
    text = remove_comments(text)
    for item, start, stop in enum.scanString(text):
        if dump:
            print("\n// Source text section:\n", text[start:stop], "\n// End of source section\n")
        id = 0
        last_ename : str = ""
        last_was_type : bool = False
        for entry in item.names:
            if entry.value != "":
                id = int(entry.value)
            ename = item.enum_type if item.enum_type else item.enum
            if ename != last_ename:
                if last_ename != "":
                    end_array(last_ename, is_header)
                    emit_function(last_ename, last_was_type, is_header)
                last_ename = ename
                last_was_type = item.enum_type != ""
                start_array(ename, is_header)

            #print("%s_%s = %d" % (item.enum.upper(), entry.name.upper(), id))
            emit_string(entry.name.upper(), id, is_header)
            id += 1
        if last_ename != "":
            end_array(last_ename, is_header)
            emit_function(last_ename, last_was_type, is_header)


def emit_start(header: str):
    if bool(header):
        print("// Auto generated header for enum strings")
        print(f"#ifndef __{header}")
        print(f"#define __{header}")
    else:
        print("// Start of generated enum strings section\n")
        print(f"char *{enum_fn}(char **arr, int val, int max);")


def emit_end(header: str):
    if header:
        print(f"#endif   // {header}")
    else:
        print("// End of generated enum strings section\n")

if __name__ == "__main__":
    import argparse
    import sys

    parser = argparse.ArgumentParser(description="Generate enum strings for c/cpp headers.")
    parser.add_argument('source', type=str, nargs='*', help="c/ccp source file to parse")
    parser.add_argument('--sample', action='store_true',
                        help='if set, use internal sample text instead of the source files')
    parser.add_argument('--dump', action='store_true',
                        help='if set, dump the enum sections')
    parser.add_argument('--reuse', action='store_true',
                        help='if set, no str function is emitted')
    parser.add_argument('--skip_includes', action='store_true',
                        help='if set, source files are not #included')
    parser.add_argument('--max', type=int, default=max_enum_value,
                        help='max entries and max value per enum')
    parser.add_argument('--array_prefix', type=str, default=array_prefix, metavar="STR",
                        help=f'prefix of string arrays [default={array_prefix}]')
    parser.add_argument('--enum_fn', type=str, default=enum_fn, metavar="FN",
                        help='set the internal enum lookup fn: char *fn(char **arr, int val, int max)')
    parser.add_argument('--fn_prefix', type=str, default=fn_prefix, metavar="STR",
                        help='prefix of enum to string fn')
    parser.add_argument('--gen_header', type=str, metavar="FILE",
                        help='emit headr c declarations file instead of the c file')
    parser.add_argument('--use_header', type=str, metavar="FILE",
                        help='include the given header file')

    args = parser.parse_args()
    
    max_enum_value = args.max
    array_prefix = args.array_prefix
    enum_fn = args.enum_fn
    fn_prefix = args.fn_prefix
    is_header = bool(args.gen_header)
    header = str(args.gen_header).replace("/","_").replace(".", "").upper() if is_header else ""

    emit_start(header)

    if not args.skip_includes:
        emit_includes(args.source, args.use_header)

    if not args.reuse and not is_header:
        emit_str_function()

    if args.sample:
        parse_text(sample, is_header=is_header, dump=args.dump)
        if not is_header:
            emit_sample_main()        
        emit_end(header)
        sys.exit(0)

    for f in args.source:
        try:
            parse_text(open(f, "r").read(), is_header=is_header, dump=args.dump)
        except IOError as e:
            print(f"Can't parse {f}", e)
    
    emit_end(header)
