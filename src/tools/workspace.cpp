// src/tools/workspace.cpp
//
// Workspace::resolve: a purely lexical check on a model-supplied path. It
// never touches the filesystem, so it works for files that don't exist yet.
// Symbolic links are handled separately by check_symlinks (provided).


#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    // 1. nothing to resolve
    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }

    // 2. a NUL byte would make the OS cut the path short
    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "invalid path: contains null";
        return false;
    }

    // 3. only workspace-relative paths, even if an absolute one points inside root
    fs::path p {std::string(user_path)};
    if (p.is_absolute() || p.has_root_path()) {
        *reason = "path must be relative to the workspace";
        return false;
    }

    // 4. drop "." parts and doubled slashes, fold "x/.." pairs
    fs::path n = p.lexically_normal();

    // 5. a leading ".." means the path climbs out of the workspace, even if
    //    it comes back in later (notes/../../ws/notes/todo.txt)
    if (!n.empty() && *n.begin() == "..") {
        *reason = "path is outside the workspace";
        return false;
    }

    // 6. "a/.." normalizes to "." which is the workspace folder itself
    if (n.empty() || n == ".") {
        *out = root();
    } else {
        *out = root() / n;
    }
    return true;
}
