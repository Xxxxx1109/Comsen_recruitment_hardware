/*
 * AI-assisted replacement for the laboratory calculator engine.
 * The public calculator_engine.h interface is retained for the lab UI library.
 * This file is new source, not the source of libcalculator_engine.a.
 *
 * 双栈表达式计算器：values 保存数值，operators 保存尚未计算的符号。
 * 固定大小数组，不使用 malloc，也不使用递归，便于控制 STM32 的栈空间。
 */
#include "calculator_engine.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define INPUT_LIMIT 63u
#define VALUE_LIMIT 32u
#define OP_LIMIT 64u
#define PI_F 3.14159265358979323846f

typedef struct {
    calc_complex_t values[VALUE_LIMIT];
    char operators[OP_LIMIT];
    unsigned value_count;
    unsigned op_count;
    calc_angle_unit_t angle;
    uint8_t complex_mode;
    calc_status_t status;
} evaluator_t;

static calc_complex_t number(float real, float imag)
{
    calc_complex_t value = {real, imag};
    return value;
}

static void fail(evaluator_t *e, calc_status_t status)
{
    if (e->status == CALC_OK) e->status = status;
}

static calc_complex_t checked(evaluator_t *e, calc_complex_t value)
{
    if (!isfinite(value.real) || !isfinite(value.imag) ||
        (!e->complex_mode && value.imag != 0.0f)) {
        fail(e, CALC_DOMAIN);
        return number(0.0f, 0.0f);
    }
    return value;
}

static int digit(char c) { return c >= '0' && c <= '9'; }
static int space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static int function(char c)
{
    return c == 's' || c == 'c' || c == 't' || c == 'l' || c == 'n' || c == 'q';
}

/* 幂高于负号：-2^2 = -(2^2)。幂从右结合：2^3^2 = 2^(3^2)。 */
static int precedence(char c)
{
    switch (c) {
    case '+': case '-': return 10;
    case '*': case '/': return 20;
    case 'd': return 30;  /* 实验室界面用 d 表示极坐标的角度分隔符 */
    case '~': case '#': return 40; /* 一元负号、一元正号 */
    case '^': return 50;
    default: return 0;
    }
}

static void push_value(evaluator_t *e, calc_complex_t value)
{
    if (e->value_count == VALUE_LIMIT) fail(e, CALC_SYNTAX);
    else e->values[e->value_count++] = checked(e, value);
}

static void push_operator(evaluator_t *e, char op)
{
    if (e->op_count == OP_LIMIT) fail(e, CALC_SYNTAX);
    else e->operators[e->op_count++] = op;
}

/* 支持小数和大写 E 指数；只保留 float 所需的有效数字，避免长数字中间溢出。 */
static calc_complex_t read_number(evaluator_t *e, const char **cursor)
{
    const char *p = *cursor;
    float mantissa = 0.0f;
    int decimal_shift = 0;
    unsigned digits = 0;
    int fractional = 0;
    for (;;) {
        if (digit(*p)) {
            if (mantissa < 100000000.0f) mantissa = mantissa * 10.0f + (*p - '0');
            else ++decimal_shift;
            if (fractional) --decimal_shift;
            ++digits;
            ++p;
        } else if (*p == '.' && !fractional) {
            fractional = 1;
            ++p;
        } else break;
    }
    if (digits == 0) fail(e, CALC_SYNTAX);
    if (*p == 'E') {
        int sign = 1;
        unsigned exponent = 0;
        ++p;
        if (*p == '+' || *p == '-') {
            if (*p == '-') sign = -1;
            ++p;
        }
        if (!digit(*p)) fail(e, CALC_SYNTAX);
        while (digit(*p)) {
            if (exponent < 1000u) exponent = exponent * 10u + (unsigned)(*p - '0');
            ++p;
        }
        if (exponent > 99u) fail(e, CALC_DOMAIN);
        decimal_shift += sign * (int)exponent;
    }
    *cursor = p;
    if (mantissa == 0.0f || e->status != CALC_OK) return number(0.0f, 0.0f);
    while (mantissa >= 10.0f) {
        mantissa /= 10.0f;
        ++decimal_shift;
    }
    if (decimal_shift > 38) {
        fail(e, CALC_DOMAIN);
        return number(0.0f, 0.0f);
    }
    /* 先保留一位数量级，避免 1.5E-45 的比例因子提前下溢。 */
    float value;
    if (decimal_shift < -46) value = 0.0f;
    else if (decimal_shift < -37) value = (mantissa * powf(10.0f, (float)(decimal_shift + 10))) * 1.0e-10f;
    else value = mantissa * powf(10.0f, (float)decimal_shift);
    return checked(e, number(value, 0.0f));
}

static float radians(const evaluator_t *e, float value)
{
    if (e->angle == CALC_ANGLE_DEG) return fmodf(value, 360.0f) * (PI_F / 180.0f);
    return value;
}

static calc_complex_t apply_function(evaluator_t *e, char op, calc_complex_t a)
{
    if (op == '~') return number(-a.real, -a.imag);
    if (op == '#') return a;
    /* 与保留的界面约定一致：三角/对数只接受实数；负数开方需要 CMPLX。 */
    if (a.imag != 0.0f) {
        fail(e, CALC_DOMAIN);
        return number(0.0f, 0.0f);
    }
    float x = a.real;
    switch (op) {
    case 's': return number(sinf(radians(e, x)), 0.0f);
    case 'c': return number(cosf(radians(e, x)), 0.0f);
    case 't': {
        float angle = radians(e, x);
        if (fabsf(cosf(angle)) < 1.0e-6f) break;
        return number(tanf(angle), 0.0f);
    }
    case 'l': if (x > 0.0f) return number(log10f(x), 0.0f); break;
    case 'n': if (x > 0.0f) return number(logf(x), 0.0f); break;
    case 'q':
        if (x >= 0.0f) return number(sqrtf(x), 0.0f);
        if (e->complex_mode) return number(0.0f, sqrtf(-x));
        break;
    default: fail(e, CALC_SYNTAX); return number(0.0f, 0.0f);
    }
    fail(e, CALC_DOMAIN);
    return number(0.0f, 0.0f);
}

static calc_complex_t power(evaluator_t *e, calc_complex_t a, calc_complex_t b)
{
    if (a.real == 0.0f && a.imag == 0.0f) {
        if (b.imag != 0.0f) fail(e, CALC_DOMAIN);
        else if (b.real < 0.0f) fail(e, CALC_DIV_ZERO);
        return number(b.real == 0.0f ? 1.0f : 0.0f, 0.0f);
    }
    if (a.imag == 0.0f && b.imag == 0.0f &&
        (a.real >= 0.0f || truncf(b.real) == b.real)) {
        return number(powf(a.real, b.real), 0.0f);
    }
    if (!e->complex_mode) {
        fail(e, CALC_DOMAIN);
        return number(0.0f, 0.0f);
    }
    /* a^b = exp(b * (ln|a| + i arg(a)))，取复数主值。 */
    float scale = fmaxf(fabsf(a.real), fabsf(a.imag));
    float normalized_real = a.real / scale, normalized_imag = a.imag / scale;
    float log_radius = logf(scale) + 0.5f * logf(normalized_real * normalized_real + normalized_imag * normalized_imag);
    /* 一元负号可能产生 -0 虚部；负实数统一从 +pi 分支取主值。 */
    float angle = a.imag == 0.0f && a.real < 0.0f ? PI_F : atan2f(a.imag, a.real);
    float radius = expf(b.real * log_radius - b.imag * angle);
    float phase = b.real * angle + b.imag * log_radius;
    return number(radius * cosf(phase), radius * sinf(phase));
}

static calc_complex_t binary(evaluator_t *e, char op, calc_complex_t a, calc_complex_t b)
{
    switch (op) {
    case '+': return number(a.real + b.real, a.imag + b.imag);
    case '-': return number(a.real - b.real, a.imag - b.imag);
    case '*': return number(a.real * b.real - a.imag * b.imag, a.real * b.imag + a.imag * b.real);
    case '/': {
        if (b.real == 0.0f && b.imag == 0.0f) {
            fail(e, CALC_DIV_ZERO);
            return number(0.0f, 0.0f);
        }
        /* 输入/结果仍为 float；中间用 double，避免平方提前上溢/下溢。 */
        double denominator = (double)b.real * b.real + (double)b.imag * b.imag;
        double real = ((double)a.real * b.real + (double)a.imag * b.imag) / denominator;
        double imag = ((double)a.imag * b.real - (double)a.real * b.imag) / denominator;
        return number((float)real, (float)imag);
    }
    case '^': return power(e, a, b);
    case 'd':
        if (e->complex_mode && a.imag == 0.0f && b.imag == 0.0f) {
            float angle = radians(e, b.real);
            return number(a.real * cosf(angle), a.real * sinf(angle));
        }
        fail(e, CALC_DOMAIN);
        return number(0.0f, 0.0f);
    default: fail(e, CALC_SYNTAX); return number(0.0f, 0.0f);
    }
}

static void reduce(evaluator_t *e)
{
    if (e->op_count == 0) { fail(e, CALC_SYNTAX); return; }
    char op = e->operators[--e->op_count];
    unsigned count = (function(op) || op == '~' || op == '#') ? 1u : 2u;
    if (op == '(' || e->value_count < count) { fail(e, CALC_SYNTAX); return; }
    calc_complex_t b = e->values[--e->value_count];
    calc_complex_t result;
    if (count == 1u) result = apply_function(e, op, b);
    else {
        calc_complex_t a = e->values[--e->value_count];
        result = binary(e, op, a, b);
    }
    push_value(e, result);
}

/* 既接受界面的单字母函数，也接受用于测试/阅读的完整函数名。 */
static char read_name(const char **cursor)
{
    static const struct { const char *name; char token; } names[] = {
        {"sqrt", 'q'}, {"sin", 's'}, {"cos", 'c'}, {"tan", 't'},
        {"log", 'l'}, {"ln", 'n'}, {"pi", 'p'}, {"ans", 'A'}
    };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        size_t length = strlen(names[i].name);
        if (strncmp(*cursor, names[i].name, length) == 0) {
            *cursor += length;
            return names[i].token;
        }
    }
    return *(*cursor)++;
}

calc_status_t calculator_evaluate(const char *expression, calc_angle_unit_t angle_unit,
                                  uint8_t allow_complex, calc_complex_t answer,
                                  calc_complex_t *result)
{
    if (result == NULL) return CALC_SYNTAX;
    /* answer/result 可以指向同一个对象，answer 已按值复制。 */
    *result = number(0.0f, 0.0f);
    if (expression == NULL) return CALC_SYNTAX;
    unsigned length = 0;
    while (length <= INPUT_LIMIT && expression[length] != '\0') ++length;
    if (length == 0 || length > INPUT_LIMIT) return CALC_SYNTAX;
    if (angle_unit != CALC_ANGLE_DEG && angle_unit != CALC_ANGLE_RAD) return CALC_DOMAIN;

    evaluator_t e = { .angle = angle_unit, .complex_mode = allow_complex != 0, .status = CALC_OK };
    const char *p = expression;
    int expect_value = 1;
    while (*p != '\0' && e.status == CALC_OK) {
        if (space(*p)) { ++p; continue; }
        char token = *p;
        if (expect_value) {
            if (token == '+' || token == '-') {
                push_operator(&e, token == '-' ? '~' : '#');
                ++p;
            } else if (token == '(') {
                push_operator(&e, '(');
                ++p;
            } else if (digit(token) || token == '.') {
                push_value(&e, read_number(&e, &p));
                expect_value = 0;
            } else {
                token = read_name(&p);
                if (function(token)) {
                    while (space(*p)) ++p;
                    if (*p != '(') { fail(&e, CALC_SYNTAX); break; }
                    push_operator(&e, token);
                    push_operator(&e, '(');
                    ++p;
                } else if (token == 'p' || token == 'e' || token == 'A' || token == 'i') {
                    calc_complex_t value = answer;
                    if (token == 'p') value = number(PI_F, 0.0f);
                    if (token == 'e') value = number(2.718281828459045f, 0.0f);
                    if (token == 'i') value = number(0.0f, 1.0f);
                    push_value(&e, value);
                    expect_value = 0;
                } else fail(&e, CALC_SYNTAX);
            }
        } else if (token == ')') {
            while (e.op_count && e.operators[e.op_count - 1] != '(' && e.status == CALC_OK) reduce(&e);
            if (e.op_count == 0) { fail(&e, CALC_SYNTAX); break; }
            --e.op_count;
            if (e.op_count && function(e.operators[e.op_count - 1])) reduce(&e);
            ++p;
        } else if (token == '+' || token == '-' || token == '*' || token == '/' || token == '^' || token == 'd') {
            while (e.op_count && precedence(e.operators[e.op_count - 1]) != 0 && e.status == CALC_OK) {
                int top = precedence(e.operators[e.op_count - 1]);
                if (top < precedence(token) || (top == precedence(token) && token == '^')) break;
                reduce(&e);
            }
            push_operator(&e, token);
            ++p;
            expect_value = 1;
        } else fail(&e, CALC_SYNTAX); /* 禁止省略乘号：2(3)、2i 都应明确写 *。 */
    }
    if (expect_value) fail(&e, CALC_SYNTAX);
    while (e.op_count && e.status == CALC_OK) reduce(&e);
    if (e.value_count != 1u) fail(&e, CALC_SYNTAX);
    if (e.status == CALC_OK) *result = e.values[0];
    return e.status;
}

calc_status_t calculator_solve_linear(float a, float b, float *x)
{
    if (x == NULL) return CALC_SYNTAX;
    *x = 0.0f;
    if (!isfinite(a) || !isfinite(b)) return CALC_DOMAIN;
    if (a == 0.0f) return CALC_DIV_ZERO;
    float value = -b / a;
    if (!isfinite(value)) return CALC_DOMAIN;
    *x = value;
    return CALC_OK;
}
