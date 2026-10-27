// src/tools/calculator_eval.cpp
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"

#include <stack>
#include <string>
#include <vector>

namespace {

int precedence(CalcToken::Kind kind) {
    if (kind == CalcToken::Kind::Times || kind == CalcToken::Kind::Divide) return 2;
    if (kind == CalcToken::Kind::Plus || kind == CalcToken::Kind::Minus) return 1;
    return 0;
}

// left op right -> *result. Fails on divide by zero.
bool do_math(CalcToken::Kind op, double left, double right, double* result, std::string* error) {
    if (op == CalcToken::Kind::Plus) {
        *result = left + right;
    } else if (op == CalcToken::Kind::Minus) {
        *result = left - right;
    } else if (op == CalcToken::Kind::Times) {
        *result = left * right;
    } else if (op == CalcToken::Kind::Divide) {
        if (right == 0.0) {
            *error = "division by zero";
            return false;
        }
        *result = left / right;
    } else {
        *error = "invalid expression";
        return false;
    }
    return true;
}

// Takes the top operator and the two numbers under it off their stacks and
// pushes the answer back on the number stack.
bool apply_top(std::stack<double>& values, std::stack<CalcToken::Kind>& ops, std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "invalid expression";
        return false;
    }
    CalcToken::Kind op {ops.top()};
    ops.pop();
    double right {values.top()};
    values.pop();
    double left {values.top()};
    values.pop();

    double result {0.0};
    if (!do_math(op, left, right, &result, error)) return false;
    values.push(result);
    return true;
}
}

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    std::stack<double> values;
    std::stack<CalcToken::Kind> ops;

    for (const CalcToken& token : tokens) {
        if (token.kind == CalcToken::Kind::Number) {
            values.push(token.value);
        } else if (token.kind == CalcToken::Kind::LParen) {
            ops.push(token.kind);
        } else if (token.kind == CalcToken::Kind::RParen) {
            while (!ops.empty() && ops.top() != CalcToken::Kind::LParen) {
                if (!apply_top(values, ops, error)) return false;
            }
            if (ops.empty()) {
                *error = "invalid expression";
                return false;
            }
            ops.pop();
        } else {
            while (!ops.empty() && ops.top() != CalcToken::Kind::LParen &&
                   precedence(ops.top()) >= precedence(token.kind)) {
                if (!apply_top(values, ops, error)) return false;
            }
            ops.push(token.kind);
        }
    }

    while (!ops.empty()) {
        if (ops.top() == CalcToken::Kind::LParen) {
            *error = "invalid expression";
            return false;
        }
        if (!apply_top(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        *error = "invalid expression";
        return false;
    }
    *out = values.top();
    return true;
}
