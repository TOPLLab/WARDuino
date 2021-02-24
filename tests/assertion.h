#ifndef ASSERTION_H
#define ASSERTION_H

#include <stdint.h>

/*
 cmd:
  <module>                                   ;; define, validate, and initialize module
  ( register <string> <name>? )              ;; register module for imports
  <action>                                   ;; perform action and print results
  <assertion>                                ;; assert result of an action
  <meta>                                     ;; meta command

module:
  ...
  ( module <name>? binary <string>* )        ;; module in binary format (may be malformed)
  ( module <name>? quote <string>* )         ;; module quoted in text (may be malformed)

action:
  ( invoke <name>? <string> <expr>* )        ;; invoke function export
  ( get    <name>? <string> )                ;; get global export

assertion:
  ( assert_return <action> <result>* )       ;; assert action has expected results
  ( assert_trap <action> <failure> )         ;; assert action traps with given failure string
  ( assert_exhaustion <action> <failure> )   ;; assert action exhausts system resources
  ( assert_malformed <module> <failure> )    ;; assert module cannot be decoded with given failure string
  ( assert_invalid <module> <failure> )      ;; assert module is invalid with given failure string
  ( assert_unlinkable <module> <failure> )   ;; assert module fails to link
  ( assert_trap <module> <failure> )         ;; assert module traps on instantiation

result:
  ( <val_type>.const <numpat> )

numpat:
  <value>                                    ;; literal result
  nan:canonical                              ;; NaN in canonical form
  nan:arithmetic                             ;; NaN with 1 in MSB of payload

meta:
  ( script <name>? <script> )                ;; name a subscript
  ( input <name>? <string> )                 ;; read script or module from file
  ( output <name>? <string>? )               ;; output module to stout or file
*/

typedef enum {
    RETURN, TRAP, EXHAUSTION, MALFORMED, INVALID, UNLINKABLE
} AssertionType;
typedef enum {
    INVOKE, GET
} ActionType;
typedef enum {
    UI32, I32V, UI64, I64V, F32V, F64V
} ValueType;


typedef struct {
    ValueType type;
    union {
        uint32_t uint32;
        int32_t int32;
        uint64_t uint64;
        int64_t int64;
        float f32;
        double f64;
        char *str;
    };
} Value;

typedef enum {
    EMPTY, NAN_R, VAL,
} ResultType;

typedef struct {
    ResultType type;
    union {
        Value *value;
    };
} Result;

typedef struct {
    ActionType type;
    char *name;
    Value *expr;
} Action;


typedef struct {
    AssertionType type;
    union {
        Action *action;
        char *module;
    };
    union {
        Result *result;
        char *failure;
    };
} Assertion;

Action *makeInvokeAction(char *name, Value *expr);

Assertion *makeAssertionReturn(Action *action, Result *result);

Assertion *makeAssertionExhaustion(Action *action);

Result *makeEmptyResult();

Result *makeValueResult(Value *val);

Value *makeUI64(uint64_t value);

Value *makeI64(int64_t value);

#endif
