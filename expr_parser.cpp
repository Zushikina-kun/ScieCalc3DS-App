#include "expr_parser.h"
#include <cmath>
#include <cctype>
#include <array>
#include <utility>

struct Expr::Node {
    enum class Kind { Num, Var, Neg, Add, Sub, Mul, Div, Pow, Call };
    Kind kind;
    double num = 0.0;
    std::unique_ptr<Node> a, b;
    std::string func; // only used when kind == Call
};

namespace {

double call_func(const std::string& name, double v)
{
    if(name == "sin")   return std::sin(v);
    if(name == "cos")   return std::cos(v);
    if(name == "tan")   return std::tan(v);
    if(name == "asin")  return std::asin(v);   // std::asin already returns NaN outside [-1,1]
    if(name == "acos")  return std::acos(v);   // same
    if(name == "atan")  return std::atan(v);
    if(name == "sinh")  return std::sinh(v);
    if(name == "cosh")  return std::cosh(v);
    if(name == "tanh")  return std::tanh(v);
    if(name == "exp")   return std::exp(v);
    if(name == "abs")   return std::fabs(v);
    if(name == "sqrt")  return v < 0.0 ? NAN : std::sqrt(v);
    if(name == "ln")    return v <= 0.0 ? NAN : std::log(v);   // matches equation.cpp: ln -> log
    if(name == "log")   return v <= 0.0 ? NAN : std::log10(v); // matches equation.cpp: log -> log10
    return NAN; // unknown function name - never reached if Parser::parse_atom validated it
}

}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------
struct Expr::Parser {
    std::string_view text;
    size_t pos = 0;
    int depth = 0;
    bool failed = false;
    std::string err_msg;
    int err_pos = 0;

    // Bounds total AST node count. This is a SEPARATE guard from DepthGuard
    // below: DepthGuard bounds grammar-nesting recursion (e.g. parentheses),
    // but a long flat chain like "1+1+1+...+1" doesn't recurse deeply while
    // *parsing* (parse_expr's loop is iterative) - it still produces a tree
    // whose depth equals the number of terms, which eval() then walks
    // recursively. node_budget bounds that tree size directly, so eval()'s
    // recursion depth stays bounded even if kMaxInputLength is ever raised.
    int node_budget = 256;

    explicit Parser(std::string_view t) : text(t) {}

    std::unique_ptr<Node> new_node(Node::Kind kind)
    {
        if(node_budget <= 0) { fail("expression too long/complex"); return nullptr; }
        --node_budget;
        auto n = std::make_unique<Node>();
        n->kind = kind;
        return n;
    }

    void skip_ws()
    {
        while(pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    }

    char peek()
    {
        skip_ws();
        return pos < text.size() ? text[pos] : '\0';
    }

    bool eat(char c)
    {
        skip_ws();
        if(pos < text.size() && text[pos] == c) { ++pos; return true; }
        return false;
    }

    void fail(const char* msg)
    {
        if(!failed) // keep the first error, it's usually the most useful one
        {
            failed = true;
            err_msg = msg;
            err_pos = static_cast<int>(pos);
        }
    }

    // Depth guard: every recursive rule bumps this on entry and pops on
    // exit. If user input is pathological (deeply nested parens), this
    // trips instead of overflowing the stack.
    struct DepthGuard {
        Parser& p;
        DepthGuard(Parser& p_) : p(p_) { ++p.depth; if(p.depth > Expr::kMaxDepth) p.fail("expression too complex"); }
        ~DepthGuard() { --p.depth; }
    };

    std::unique_ptr<Node> make_num(double v)
    {
        auto n = new_node(Node::Kind::Num);
        if(!n) return nullptr;
        n->num = v;
        return n;
    }

    std::unique_ptr<Node> parse_number()
    {
        skip_ws();
        const size_t start = pos;
        bool any_digit = false;
        while(pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) { ++pos; any_digit = true; }
        if(pos < text.size() && text[pos] == '.')
        {
            ++pos;
            while(pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) { ++pos; any_digit = true; }
        }
        if(!any_digit) { fail("expected a number"); return nullptr; }
        const std::string sub(text.substr(start, pos - start));
        try { return make_num(std::stod(sub)); }
        catch(...) { fail("bad number"); return nullptr; }
    }

    // identifier = letters only (function names / 'x' / 'pi')
    std::string_view parse_ident()
    {
        skip_ws();
        const size_t start = pos;
        while(pos < text.size() && std::isalpha(static_cast<unsigned char>(text[pos]))) ++pos;
        return text.substr(start, pos - start);
    }

    static bool is_known_func(std::string_view s)
    {
        static constexpr std::array<const char*, 14> names = {
            "sin","cos","tan","asin","acos","atan",
            "sinh","cosh","tanh","exp","abs","sqrt","ln","log"
        };
        for(const auto* n : names) if(s == n) return true;
        return false;
    }

    // atom := number | 'x' | 'pi' | func '(' expr ')' | '(' expr ')'
    std::unique_ptr<Node> parse_atom()
    {
        DepthGuard g(*this);
        if(failed) return nullptr;

        skip_ws();
        if(pos >= text.size()) { fail("unexpected end of input"); return nullptr; }

        if(text[pos] == '(')
        {
            ++pos;
            auto inner = parse_expr();
            if(!eat(')')) { fail("missing ')'"); return nullptr; }
            return inner;
        }

        if(std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '.')
        {
            return parse_number();
        }

        if(std::isalpha(static_cast<unsigned char>(text[pos])))
        {
            const size_t save = pos;
            std::string_view ident = parse_ident();

            if(ident == "x")
            {
                return new_node(Node::Kind::Var);
            }
            if(ident == "pi")
            {
                return make_num(M_PI);
            }
            if(ident == "e")
            {
                return make_num(M_E);
            }
            if(is_known_func(ident))
            {
                if(!eat('('))
                {
                    fail("expected '(' after function name");
                    return nullptr;
                }
                auto arg = parse_expr();
                if(!eat(')')) { fail("missing ')'"); return nullptr; }
                auto n = new_node(Node::Kind::Call);
                if(!n) return nullptr;
                n->func = std::string(ident);
                n->a = std::move(arg);
                return n;
            }

            pos = save;
            fail("unknown identifier (try: sin cos tan asin acos atan sinh cosh tanh ln log sqrt abs exp x pi)");
            return nullptr;
        }

        fail("unexpected character");
        return nullptr;
    }

    // Handles implicit multiplication so "2x", "2sin(x)", "(x+1)(x-1)" work,
    // which is what most calculator users expect and type without thinking.
    bool starts_atom()
    {
        const char c = peek();
        return c == '(' || std::isdigit(static_cast<unsigned char>(c)) || c == '.' || std::isalpha(static_cast<unsigned char>(c));
    }

    // power := atom ('^' unary)?   (right-associative)
    std::unique_ptr<Node> parse_power()
    {
        DepthGuard g(*this);
        if(failed) return nullptr;
        auto base = parse_atom();
        if(failed) return nullptr;
        if(eat('^'))
        {
            auto exponent = parse_unary();
            if(failed) return nullptr;
            auto n = new_node(Node::Kind::Pow);
            if(!n) return nullptr;
            n->a = std::move(base);
            n->b = std::move(exponent);
            return n;
        }
        return base;
    }

    // unary := '-' unary | power
    std::unique_ptr<Node> parse_unary()
    {
        DepthGuard g(*this);
        if(failed) return nullptr;
        if(eat('-'))
        {
            auto inner = parse_unary();
            if(failed) return nullptr;
            auto n = new_node(Node::Kind::Neg);
            if(!n) return nullptr;
            n->a = std::move(inner);
            return n;
        }
        if(eat('+')) return parse_unary(); // unary plus is a no-op
        return parse_power();
    }

    // term := unary (('*' | '/' | implicit-mul) unary)*
    std::unique_ptr<Node> parse_term()
    {
        DepthGuard g(*this);
        if(failed) return nullptr;
        auto lhs = parse_unary();
        for(;;)
        {
            if(failed) return nullptr;
            if(eat('*'))
            {
                auto rhs = parse_unary();
                if(failed) return nullptr;
                auto n = new_node(Node::Kind::Mul);
                if(!n) return nullptr;
                n->a = std::move(lhs); n->b = std::move(rhs);
                lhs = std::move(n);
            }
            else if(eat('/'))
            {
                auto rhs = parse_unary();
                if(failed) return nullptr;
                auto n = new_node(Node::Kind::Div);
                if(!n) return nullptr;
                n->a = std::move(lhs); n->b = std::move(rhs);
                lhs = std::move(n);
            }
            else if(starts_atom())
            {
                auto rhs = parse_unary();
                if(failed) return nullptr;
                auto n = new_node(Node::Kind::Mul);
                if(!n) return nullptr;
                n->a = std::move(lhs); n->b = std::move(rhs);
                lhs = std::move(n);
            }
            else break;
        }
        return lhs;
    }

    // expr := term (('+' | '-') term)*
    std::unique_ptr<Node> parse_expr()
    {
        DepthGuard g(*this);
        if(failed) return nullptr;
        auto lhs = parse_term();
        for(;;)
        {
            if(failed) return nullptr;
            if(eat('+'))
            {
                auto rhs = parse_term();
                if(failed) return nullptr;
                auto n = new_node(Node::Kind::Add);
                if(!n) return nullptr;
                n->a = std::move(lhs); n->b = std::move(rhs);
                lhs = std::move(n);
            }
            else if(eat('-'))
            {
                auto rhs = parse_term();
                if(failed) return nullptr;
                auto n = new_node(Node::Kind::Sub);
                if(!n) return nullptr;
                n->a = std::move(lhs); n->b = std::move(rhs);
                lhs = std::move(n);
            }
            else break;
        }
        return lhs;
    }
};

// ---------------------------------------------------------------------------
// Expr
// ---------------------------------------------------------------------------
// Defined here (not in the header) because Node is only forward-declared in
// expr_parser.h - the destructor/move ops need the complete type to know how
// to delete a Node, so they must live where Node's definition is visible.
Expr::~Expr() = default;
Expr::Expr(Expr&&) noexcept = default;
Expr& Expr::operator=(Expr&&) noexcept = default;

Expr::Expr(std::string_view text)
{
    if(text.empty())
    {
        err_msg = "empty expression";
        err_pos = 0;
        return;
    }
    if(text.size() > kMaxInputLength)
    {
        err_msg = "expression too long";
        err_pos = static_cast<int>(kMaxInputLength);
        return;
    }

    Parser p(text);
    auto result = p.parse_expr();

    if(p.failed)
    {
        err_msg = p.err_msg;
        err_pos = p.err_pos;
        return;
    }
    p.skip_ws();
    if(p.pos != text.size())
    {
        err_msg = "unexpected trailing characters";
        err_pos = static_cast<int>(p.pos);
        return;
    }

    root = std::move(result);
    valid = true;
}

double Expr::eval(double x) const
{
    if(!valid || !root) return NAN;

    // small explicit stack-based-by-recursion eval; tree depth is bounded
    // by kMaxDepth at parse time, so this cannot overflow the stack.
    struct Impl {
        double x;
        double operator()(const Node* n) const
        {
            if(!n) return NAN;
            switch(n->kind)
            {
                case Node::Kind::Num: return n->num;
                case Node::Kind::Var: return x;
                case Node::Kind::Neg: return -(*this)(n->a.get());
                case Node::Kind::Add: return (*this)(n->a.get()) + (*this)(n->b.get());
                case Node::Kind::Sub: return (*this)(n->a.get()) - (*this)(n->b.get());
                case Node::Kind::Mul: return (*this)(n->a.get()) * (*this)(n->b.get());
                case Node::Kind::Div:
                {
                    const double bv = (*this)(n->b.get());
                    if(bv == 0.0) return NAN;
                    return (*this)(n->a.get()) / bv;
                }
                case Node::Kind::Pow: return std::pow((*this)(n->a.get()), (*this)(n->b.get()));
                case Node::Kind::Call: return call_func(n->func, (*this)(n->a.get()));
            }
            return NAN;
        }
    };
    Impl impl{x};
    const double result = impl(root.get());
    return result;
}
