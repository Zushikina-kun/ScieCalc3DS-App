#ifndef INC_EXPR_PARSER_H
#define INC_EXPR_PARSER_H

#include <string>
#include <string_view>
#include <memory>
#include <optional>

// Small recursive-descent parser + evaluator for real-valued single-variable
// functions, used only by graph mode. Supports:
//   + - * / ^  unary minus  ( )
//   sin cos tan asin acos atan sinh cosh tanh
//   ln log sqrt abs exp
//   pi, e, x
//
// Design goals (in priority order, matching what was asked for):
//   1. Cannot crash on malformed input. Parsing failures return an error
//      instead of throwing/asserting; evaluation failures (domain errors,
//      div by zero) produce NAN, never UB.
//   2. Bounded recursion. Input length is capped (kMaxInputLength) so a
//      pathological string can't blow the 3DS's small stack.
//   3. Cheap to evaluate many times per frame (needed for sampling ~400
//      x-values per redraw) - no heap allocation happens during Eval(),
//      only during Parse().
class Expr {
public:
    static constexpr size_t kMaxInputLength = 128;
    static constexpr int kMaxDepth = 32; // matches kMaxInputLength headroom

    // Parses `text`. On success, ok() is true and Eval() is usable.
    // On failure, ok() is false and error_message()/error_position() are set
    // so the UI can point at the mistake instead of just saying "error".
    explicit Expr(std::string_view text);
    ~Expr();
    Expr(Expr&&) noexcept;
    Expr& operator=(Expr&&) noexcept;
    Expr(const Expr&) = delete;
    Expr& operator=(const Expr&) = delete;

    bool ok() const { return valid; }
    const std::string& error_message() const { return err_msg; }
    int error_position() const { return err_pos; }

    // Evaluates the parsed expression at the given x. Returns NAN for any
    // domain error (log of negative, div by zero, etc) rather than crashing
    // or throwing - callers (graph sampler) should treat non-finite results
    // as "don't draw this point".
    double eval(double x) const;

private:
    struct Node;
    std::unique_ptr<Node> root;
    bool valid = false;
    std::string err_msg;
    int err_pos = 0;

    // internal recursive-descent state, only used during construction
    struct Parser;
};

#endif
