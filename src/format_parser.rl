#include <stdio.h>

#include <string>
#include <vector>

%%{
  machine FormatCompiler;

  action MarkStart { text_start_ = fpc; }

  action AddTextToken {
    int text_len = fpc - text_start_;
    if (text_len > 0) {
      tokens.emplace_back(Token::Type::PLAINTEXT,
                          std::string(text_start_, text_len));
    }
  }

  action AddFieldToken {
    int text_len = fpc - text_start_;

    tokens.emplace_back(Token::Type::FIELD, std::string(text_start_, text_len));
  }

  tag = '{' ( lower+ ( '|' lower+ )? ) >MarkStart '}' @AddFieldToken;

  main := (
    ( any - '{' )* >MarkStart %AddTextToken
    tag
  )* @eof(AddTextToken);

  write data;
}%%

struct Token {
  enum class Type : int {
    PLAINTEXT,
    FIELD,
  };

  Token(Type type, std::string value) : type(type), value(std::move(value)) {}

  Type type;
  std::string value;
};

class Compiler {
 public:
  Compiler(const std::string& input) : buffer_(input) {
    %%write init;

    p = buffer_.c_str();
    pe = p + buffer_.size();
    eof = pe;
  }
  ~Compiler() = default;

  std::vector<Token> Compile() {
    std::vector<Token> tokens;

    %%write exec;

    return tokens;
  }

 private:
  // buffer state
  const char* p;
  const char* pe;
  const char* eof;

  // current token
  const char* ts;
  const char* te;

  // machine state
  int act, cs, top, stack[1];

  std::string buffer_;
  const char* text_start_;
};

// int main(int argc, char** argv) {
//   Compiler compiler(argv[1]);
//   const std::vector<Token> tokens = compiler.Compile();
// 
//   for (const auto& token : tokens) {
//     printf("%d: '%s'\n", token.type, token.value.c_str());
//   }
// 
//   return 0;
// }
