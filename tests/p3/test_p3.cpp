// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...
 
#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"
 
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <unordered_set>
 
namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

// ------------------------------------------------------------- my register tests
 
TEST(reg_empty_state) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.find("anything") == nullptr);
    CHECK(!r.add(nullptr));              // a null tool is rejected
    CHECK(r.size() == 0);
}
 
TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    std::vector<std::string> names = {"read_file", "write_file", "list_dir", "calculator", "extra"};
    for (const std::string& n : names) REQUIRE(r.add(std::make_unique<NamedTool>(n)));
 
    std::map<std::string, int> seen;
    int calls = 0;
    r.for_each([&](const Tool& t) {
        ++seen[t.name()];
        ++calls;
    });
 
    CHECK(calls == 5);
    for (const std::string& n : names) CHECK(seen[n] == 1);
}
 
TEST(reg_10000_tools) {
    ToolRegistry r;
    Tool* first = nullptr;
    bool load_ok = true;
 
    for (int i = 0; i < 10000; ++i) {
        auto t = std::make_unique<NamedTool>("tool_" + std::to_string(i));
        Tool* raw = t.get();
        REQUIRE(r.add(std::move(t)));
        if (i == 0) first = raw;
        if (r.load_factor() > 1.0) load_ok = false;
    }
 
    CHECK(r.size() == 10000);
    CHECK(load_ok);
    CHECK(r.find("tool_0") == first);   // still the same pointer after all the resizes
 
    bool all_found = true;
    for (int i = 0; i < 10000; ++i)
        if (r.find("tool_" + std::to_string(i)) == nullptr) all_found = false;
    CHECK(all_found);
 
    std::unordered_set<std::size_t> distinct;
    for (int i = 0; i < 10000; ++i)
        distinct.insert(ToolRegistry::hash("tool_" + std::to_string(i)));
    CHECK(distinct.size() >= 9990);
 
    CHECK(r.max_probe_length() <= 64);
    std::printf("    10000 tools: buckets=%zu load=%.3f longest chain=%zu distinct hashes=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length(), distinct.size());
}

// same 10,000 names, but pretend the hash was first char + length
TEST(reg_alternative_hash) {
    ToolRegistry r;
    std::vector<std::string> names;
    for (int i = 0; i < 10000; ++i) {
        names.push_back("tool_" + std::to_string(i));
        REQUIRE(r.add(std::make_unique<NamedTool>(names.back())));
    }

    std::size_t buckets = r.bucket_count();
    std::vector<int> weak_counts(buckets, 0);
    for (const std::string& s : names) {
        std::size_t weak = static_cast<unsigned char>(s[0]) + s.size();
        ++weak_counts[weak % buckets];
    }
    int weak_longest = *std::max_element(weak_counts.begin(), weak_counts.end());

    std::printf("    weak hash longest chain=%d, FNV-1a longest chain=%zu (buckets=%zu)\n",
                weak_longest, r.max_probe_length(), buckets);
    CHECK(weak_longest > 64);
    CHECK(r.max_probe_length() <= 64);
}
 
// ------------------------------------------------------------- my workspace tests
 
TEST(ws_accepts_and_normalizes) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
 
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));   // doesn't have to exist yet
    CHECK(out == ws.root() / "new" / "dir" / "file.txt");
    REQUIRE(ws.resolve("my..notes.txt", &out, &reason));      // ".." inside a name is fine
    CHECK(out == ws.root() / "my..notes.txt");
 
    REQUIRE(ws.resolve("./notes//todo.txt", &out, &reason));  // clean up . and //
    CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(ws.resolve(".", &out, &reason));
    CHECK(out == ws.root());
    REQUIRE(ws.resolve("a/../b", &out, &reason));
    CHECK(out == ws.root() / "b");
    REQUIRE(ws.resolve("a/..", &out, &reason));
    CHECK(out == ws.root());
}
 
TEST(ws_rejects_empty_nul_absolute) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
 
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(reason == "empty path");
 
    reason.clear();
    CHECK(!ws.resolve(std::string("notes") + '\0' + "/x", &out, &reason));   // a plain "notes\0/x" literal would stop at the NUL
    CHECK(!reason.empty());
 
    reason.clear();
    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}
 
TEST(ws_rejects_dotdot_escapes) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
 
    // the last one climbs out and then walks back in to the workspace folder
    std::vector<std::string> bad = {
        "..",
        "../etc/passwd",
        "a/../../b",
        "notes/../../" + ws.root().filename().string() + "/notes/todo.txt",
    };
    for (const std::string& path : bad) {
        reason.clear();
        CHECK(!ws.resolve(path, &out, &reason));
        CHECK(!reason.empty());
    }
}
 
// ------------------------------------------------------------ my calculator tests
 
TEST(calc_left_to_right) {
    double v = 0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
    REQUIRE(eval("125 / 5 / 5", &v, &err));
    CHECK(v == 5);
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
}
 
TEST(calc_parentheses) {
    double v = 0;
    std::string err;
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("10 - (5 - 3)", &v, &err));
    CHECK(v == 8);
    REQUIRE(eval("((((10))))", &v, &err));
    CHECK(v == 10);
}

TEST(calc_negatives_and_decimals) {
    double v = 0;
    std::string err;
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("(-1) * 4", &v, &err));
    CHECK(v == -4);
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
    REQUIRE(eval("1.5 + 2.25", &v, &err));
    CHECK(v == 3.75);
}
 
TEST(calc_very_long_expressions) {
    double v = 0;
    std::string err;
 
    std::string many_terms = "0";
    for (int i = 0; i < 371887; ++i) many_terms += " + 1";
    REQUIRE(eval(many_terms, &v, &err));
    CHECK(v == 371887);
 
    std::string nested = std::string(5000, '(') + "1" + std::string(5000, ')');
    REQUIRE(eval(nested, &v, &err));
    CHECK(v == 1);
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
