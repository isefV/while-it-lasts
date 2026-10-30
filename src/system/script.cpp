#include <while_it_lasts/system/script.hpp>
#include <while_it_lasts/system/story_state.hpp>
#include <while_it_lasts/utility/text.hpp>

#include <cctype>
#include <charconv>

namespace while_it_lasts
{
    // -------------------------------------------------------------------------
    // Conditions: a recursive-descent parser over a short token list
    // -------------------------------------------------------------------------

    struct ConditionOperand
    {
        bool isName = false;
        std::string name;
        int value = 0;

        [[nodiscard]] int evaluate(const StoryState& state) const { return isName ? state.get(name) : value; }
    };

    struct Condition::Node
    {
        enum class Kind
        {
            Or,
            And,
            Not,
            Compare,
            Value,
        };

        Kind kind = Kind::Value;
        std::shared_ptr<const Node> left;
        std::shared_ptr<const Node> right;
        ConditionOperand a;
        ConditionOperand b;
        std::string op;
    };

    namespace
    {
        using Node = Condition::Node;
        using NodePtr = std::shared_ptr<const Node>;

        struct Token
        {
            enum class Kind
            {
                Name,
                Number,
                Op,
                End,
            };

            Kind kind = Kind::End;
            std::string text;
            int number = 0;
        };

        bool isNameStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
        bool isNameChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'; }

        bool tokenize(std::string_view text, std::vector<Token>& tokens, std::string& error)
        {
            std::size_t i = 0;
            while (i < text.size())
            {
                const char c = text[i];
                if (c == ' ' || c == '\t')
                {
                    ++i;
                    continue;
                }
                if (isNameStart(c))
                {
                    const std::size_t start = i;
                    while (i < text.size() && isNameChar(text[i])) ++i;
                    std::string word(text.substr(start, i - start));

                    if (word == "and") tokens.push_back({Token::Kind::Op, "&&"});
                    else if (word == "or") tokens.push_back({Token::Kind::Op, "||"});
                    else if (word == "not") tokens.push_back({Token::Kind::Op, "!"});
                    else if (word == "true") tokens.push_back({Token::Kind::Number, word, 1});
                    else if (word == "false") tokens.push_back({Token::Kind::Number, word, 0});
                    else tokens.push_back({Token::Kind::Name, std::move(word)});
                    continue;
                }
                if (std::isdigit(static_cast<unsigned char>(c)) ||
                    (c == '-' && i + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[i + 1]))))
                {
                    int value = 0;
                    const auto [end, ec] = std::from_chars(text.data() + i, text.data() + text.size(), value);
                    if (ec != std::errc())
                    {
                        error = "number too large near \"" + std::string(text.substr(i)) + "\"";
                        return false;
                    }
                    const auto length = static_cast<std::size_t>(end - (text.data() + i));
                    tokens.push_back({Token::Kind::Number, std::string(text.substr(i, length)), value});
                    i += length;
                    continue;
                }

                static constexpr std::string_view twoChar[] = {"&&", "||", "==", "!=", ">=", "<="};
                bool matched = false;
                for (const auto op : twoChar)
                {
                    if (text.substr(i, 2) == op)
                    {
                        tokens.push_back({Token::Kind::Op, std::string(op)});
                        i += 2;
                        matched = true;
                        break;
                    }
                }
                if (matched) continue;

                if (c == '!' || c == '<' || c == '>' || c == '(' || c == ')')
                {
                    tokens.push_back({Token::Kind::Op, std::string(1, c)});
                    ++i;
                    continue;
                }
                if (c == '=')
                {
                    error = "use == to compare (a single = sets a value, which only works in \"do\")";
                    return false;
                }

                error = "unexpected character '" + std::string(1, c) + "'";
                return false;
            }
            tokens.push_back({Token::Kind::End, ""});
            return true;
        }

        class Parser
        {
        public:
            explicit Parser(const std::vector<Token>& tokens)
                : tokens_(tokens)
            {
            }

            NodePtr parse(std::string& error)
            {
                NodePtr node = parseOr();
                if (node && peek().kind != Token::Kind::End)
                    fail("unexpected \"" + peek().text + "\"");
                if (!error_.empty())
                {
                    error = error_;
                    return nullptr;
                }
                return node;
            }

        private:
            const Token& peek() const { return tokens_[pos_]; }
            const Token& next() { return tokens_[pos_ < tokens_.size() - 1 ? pos_++ : pos_]; }
            bool accept(std::string_view op)
            {
                if (peek().kind == Token::Kind::Op && peek().text == op)
                {
                    ++pos_;
                    return true;
                }
                return false;
            }
            void fail(std::string message)
            {
                if (error_.empty()) error_ = std::move(message);
            }

            NodePtr binary(Node::Kind kind, NodePtr left, NodePtr right)
            {
                auto node = std::make_shared<Node>();
                node->kind = kind;
                node->left = std::move(left);
                node->right = std::move(right);
                return node;
            }

            NodePtr parseOr()
            {
                NodePtr left = parseAnd();
                while (left && accept("||"))
                    left = binary(Node::Kind::Or, left, parseAnd());
                return left;
            }

            NodePtr parseAnd()
            {
                NodePtr left = parseUnary();
                while (left && accept("&&"))
                    left = binary(Node::Kind::And, left, parseUnary());
                return left;
            }

            NodePtr parseUnary()
            {
                if (accept("!"))
                {
                    auto node = std::make_shared<Node>();
                    node->kind = Node::Kind::Not;
                    node->left = parseUnary();
                    return node->left ? node : nullptr;
                }
                if (accept("("))
                {
                    NodePtr inner = parseOr();
                    if (!accept(")")) fail("missing )");
                    return inner;
                }
                return parseComparison();
            }

            bool parseOperand(ConditionOperand& out)
            {
                const Token& token = next();
                if (token.kind == Token::Kind::Name)
                {
                    out.isName = true;
                    out.name = token.text;
                    return true;
                }
                if (token.kind == Token::Kind::Number)
                {
                    out.value = token.number;
                    return true;
                }
                fail(token.kind == Token::Kind::End ? std::string("the condition ends too early")
                                                    : "expected a name or a number, found \"" + token.text + "\"");
                return false;
            }

            NodePtr parseComparison()
            {
                auto node = std::make_shared<Node>();
                if (!parseOperand(node->a)) return nullptr;

                static constexpr std::string_view ops[] = {"==", "!=", ">=", "<=", ">", "<"};
                for (const auto op : ops)
                {
                    if (accept(op))
                    {
                        node->kind = Node::Kind::Compare;
                        node->op = std::string(op);
                        if (!parseOperand(node->b)) return nullptr;
                        return node;
                    }
                }
                node->kind = Node::Kind::Value;
                return node;
            }

            const std::vector<Token>& tokens_;
            std::size_t pos_ = 0;
            std::string error_;
        };

        bool evaluateNode(const Node& node, const StoryState& state)
        {
            switch (node.kind)
            {
            case Node::Kind::Or: return evaluateNode(*node.left, state) || evaluateNode(*node.right, state);
            case Node::Kind::And: return evaluateNode(*node.left, state) && evaluateNode(*node.right, state);
            case Node::Kind::Not: return !evaluateNode(*node.left, state);
            case Node::Kind::Value: return node.a.evaluate(state) != 0;
            case Node::Kind::Compare:
            {
                const int a = node.a.evaluate(state);
                const int b = node.b.evaluate(state);
                if (node.op == "==") return a == b;
                if (node.op == "!=") return a != b;
                if (node.op == ">=") return a >= b;
                if (node.op == "<=") return a <= b;
                if (node.op == ">") return a > b;
                return a < b;
            }
            }
            return false;
        }

        void collectNames(const Node& node, std::vector<std::string>& out)
        {
            if (node.left) collectNames(*node.left, out);
            if (node.right) collectNames(*node.right, out);
            if (node.a.isName) out.push_back(node.a.name);
            if (node.b.isName) out.push_back(node.b.name);
        }
    } // namespace

    std::optional<Condition> Condition::parse(std::string_view text, std::string& error)
    {
        Condition condition;
        condition.source_ = std::string(trim(text));
        if (condition.source_.empty()) return condition;

        std::vector<Token> tokens;
        if (!tokenize(condition.source_, tokens, error)) return std::nullopt;

        Parser parser(tokens);
        condition.root_ = parser.parse(error);
        if (!condition.root_) return std::nullopt;
        return condition;
    }

    bool Condition::evaluate(const StoryState& state) const
    {
        return !root_ || evaluateNode(*root_, state);
    }

    std::vector<std::string> Condition::names() const
    {
        std::vector<std::string> out;
        if (root_) collectNames(*root_, out);
        return out;
    }

    // -------------------------------------------------------------------------
    // Effects
    // -------------------------------------------------------------------------

    namespace
    {
        bool parseValue(std::string_view text, int& value)
        {
            if (text == "true")
            {
                value = 1;
                return true;
            }
            if (text == "false")
            {
                value = 0;
                return true;
            }
            const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
            return ec == std::errc() && end == text.data() + text.size();
        }

        bool isName(std::string_view text)
        {
            if (text.empty() || !isNameStart(text[0])) return false;
            for (const char c : text)
                if (!isNameChar(c)) return false;
            return true;
        }
    } // namespace

    std::optional<Effect> Effect::parse(std::string_view text, std::string& error)
    {
        Effect effect;
        effect.source = std::string(trim(text));
        if (effect.source.empty())
        {
            error = "empty effect";
            return std::nullopt;
        }

        // name += n / name -= n / name = v
        for (const std::string_view op : {"+=", "-=", "="})
        {
            const auto at = effect.source.find(op);
            if (at == std::string::npos) continue;

            const auto name = trim(std::string_view(effect.source).substr(0, at));
            const auto value = trim(std::string_view(effect.source).substr(at + op.size()));
            if (!isName(name))
            {
                error = "\"" + std::string(name) + "\" is not a variable name";
                return std::nullopt;
            }
            if (!parseValue(value, effect.value))
            {
                error = "\"" + std::string(value) + "\" is not a whole number or true/false";
                return std::nullopt;
            }
            effect.name = std::string(name);
            if (op == "=")
            {
                effect.kind = Kind::Set;
            }
            else
            {
                effect.kind = Kind::Add;
                if (op == "-=") effect.value = -effect.value;
            }
            return effect;
        }

        auto words = splitWords(effect.source);
        if ((words[0] == "give" || words[0] == "take"))
        {
            if (words.size() != 2)
            {
                error = "\"" + words[0] + "\" needs exactly one item id";
                return std::nullopt;
            }
            effect.kind = words[0] == "give" ? Kind::Give : Kind::Take;
            effect.name = words[1];
            return effect;
        }

        effect.kind = Kind::Command;
        effect.name = words[0];
        effect.args.assign(words.begin() + 1, words.end());
        return effect;
    }

    bool applyStateEffect(const Effect& effect, StoryState& state)
    {
        switch (effect.kind)
        {
        case Effect::Kind::Set: state.set(effect.name, effect.value); return true;
        case Effect::Kind::Add: state.add(effect.name, effect.value); return true;
        case Effect::Kind::Give: state.give(effect.name); return true;
        case Effect::Kind::Take: state.take(effect.name); return true;
        case Effect::Kind::Command: return false;
        }
        return false;
    }
} // namespace while_it_lasts
