
struct Span {
    long begin;
    long end;
};

enum class TokenType {
    // literals and names (values)
    Name,
    LiteralInt,
    LiteralFloat,

    // keywords
    KeywordFunction,
    KeywordRuleset,
    KeywordLet,
    KeywordReturn,

    // punctuation
    LeftBracket,
    RightBracket,
    LeftBrace,
    RightBrace,
    Comma,
    Colon,
    SemiColon,
    Arrow,

    // operators
    Plus,
    Minus,
    Star,
    Slash,
    Equal,
    EqualTo,
    NotEqualTo,
    LessThan,
    GreaterThan,
    LessThanEqualTo,
    GreaterThanEqualTo,
    Eof
};

struct Token {
    TokenType type;
    Span span;
};