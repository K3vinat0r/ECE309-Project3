# Project 3 Design Log
**Author:** Kevin Aroche Rios · **P2 code used:** my own | instructor reference
**Tools used:** Claude (AI). I've used it to explain more about hash tables and the two-stack algorithm used in `calc_evaluate`'s operator loop, and to also have it draft the first version of `calc_evaluate` and `ToolRegistry`, as well as help draft register tests. I have it review my builds for errors. I wrote this log from my own understanding, with Claude helping with the draft.

## 1. Hash table
 
I used separate chaining: each bucket is a `std::vector` of tools. It is simpler than open addressing, which needs probing and tombstones. The hash is 64-bit FNV-1a, which uses every character of the name. The bucket count is always a power of two, so the index is `hash & (size - 1)` instead of `%`. The load-factor limit is 1.0. Before an `add` that would go over it, the table doubles its buckets and rehashes everything.
 
The weak hash `s[0] + s.size()` does not work here. Every name from `tool_0` to `tool_9999` starts with `t` and has length 6 to 9, so it gives only four different values. I measured both with 10,000 tools:
 
| | buckets | load factor | longest chain |
|---|---|---|---|
| FNV-1a (mine) | 16384 | 0.610 | 6 |
| `s[0] + s.size()` | 16384 | n/a | 9000 |
 
FNV-1a also gave 10,000 different hash values. With the weak hash, one bucket holds 9000 tools, so `find` would scan thousands of entries and stop being O(1).
 
`add` is amortized O(1). A resize costs O(n), but it only happens when the count doubles. Over n adds the moves add up to about 1 + 2 + 4 + ..., which is at most 2n, so the average cost per add is constant.
 
## 2. Pointer stability
 
The buckets hold `std::unique_ptr<Tool>`, not `Tool` objects. A resize only moves the pointers into the new buckets, so the tools stay at the same addresses. A `Tool*` from `find()` is still valid after any number of resizes. My `reg_10000_tools` test checks that the pointer for `tool_0` is the same after all 10,000 adds.
 
## 3. Calculator trace
 
`calc_evaluate` loops over the tokens with two stacks: numbers and operators. For `2 * (3 + 4) - 5`:
 
1. `2`: numbers [2].
2. `*`: operators are empty, so push. Operators [*].
3. `(`: push. Operators [*, (].
4. `3`: numbers [2, 3].
5. `+`: the top is `(`, so push. Operators [*, (, +].
6. `4`: numbers [2, 3, 4].
7. `)`: apply `+`: 3 + 4 = 7. Numbers [2, 7]. Pop the `(`. Operators [*].
8. `-`: `*` on top has higher precedence, so apply it: 2 * 7 = 14. Numbers [14]. Push `-`.
9. `5`: numbers [14, 5].
10. End: apply `-`: 14 - 5 = 9.
An operator on top is applied when it has the same or higher precedence than the new one, which gives left-to-right order. I used loops instead of recursion, so 5,000 nested parentheses and 20,000 terms do not overflow the stack.
 
## 4. Errors as values
 
In an agent loop, a tool failing is normal: a missing file, a forbidden path, a division by zero. The model reads the error and tries something else. So `resolve`, `calc_evaluate`, and the tools return an error as a value (`false` plus a message, or `ToolResult::failure`) that the loop can pass straight to the model. Exceptions are for bugs in the C++ itself, like calling `as_string()` on a JSON number. The dispatcher also catches anything a tool throws, as a last line of defense.
 
## 5. What I would design differently
 
`add` computes the hash twice, once in `find` and once to pick the bucket. I would compute it once and reuse it. I would also store each tool's hash so a resize does not recompute them.
