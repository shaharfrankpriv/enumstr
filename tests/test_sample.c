#include <stdio.h>
#include <unity/unity.h>

#include "sample.h"

#define UNKNOWN "<?>"
#define ERROR "<ERROR>"

void setUp (void) {} /* Is run before every test, put unit init calls here. */
void tearDown (void) {} /* Is run after every test, put unit clean-up calls here. */

void TestEnumStrs(void)
{
    struct Tests {
        char *message;
        int value;
        char *exp;
        char *(*fn)(int v);
    } tests[] = {
        {"# Hello - enum, no type"},
        {"Zero 0", Zero, "Zero (0)", (char *(*)(int))EnumStr_Hello},
        {"Five 5", Five, "Five (5)", (char *(*)(int))EnumStr_Hello},
        {"(missing 7)", 7, UNKNOWN, (char *(*)(int))EnumStr_Hello},
        {"(out of range 11)", 11, ERROR, (char *(*)(int))EnumStr_Hello},
        {"# Blah - type enum, no enum name"},
        {"alpha",  alpha, "alpha (4)", (char *(*)(int))EnumStr_Blah},
        {"gamma1 (10)", gamma1, "gamma1 (10)", (char *(*)(int))EnumStr_Blah},
        {"b3 (out of range)", -1, ERROR, (char *(*)(int))EnumStr_Blah},
        {"(missing 17)", 17, UNKNOWN, (char *(*)(int))EnumStr_Blah},
        {"# Kiki - type enum, enum name Kuku"},
        {"single", single, "single (0)", (char *(*)(int))EnumStr_Kiki},
        {}
    };

    for (struct Tests *t = tests; t->message; t++) {
        if (t->fn) {
            printf("%s - '%s'\n", t->message, t->exp);
            TEST_ASSERT_EQUAL_STRING(t->exp, t->fn(t->value));
        } else {
            printf("%s\n", t->message);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(TestEnumStrs);
    return UNITY_END();
}