// Rubberband language
// Copyright (C) 2014  Luiz Romário Santana Rios <luizromario at gmail dot com>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#include "tokenizer.hpp"

using namespace rbb;

token tokenizer::next()
{
    int length;
    auto ret = _previous_token = _look_token(length);

    _remaining.erase(_remaining.begin(), _remaining.begin() + length);
    return ret;
}

token tokenizer::look_next() const
{
    int l; // unused
    return _look_token(l);
}

std::vector<token> tokenizer::all()
{
    std::vector<token> result;

    for (auto tok = next(); tok.type != token::type_e::end_of_input; tok = next())
        result.push_back(tok);

    return result;
}

std::vector<token> tokenizer::look_all() const
{
    tokenizer tok{_remaining};

    return tok.all();
}

token tokenizer::_look_token(int& length) const
{
    length = 1;
    int ignore_offset = 0;
    auto cur_state = _state::start;

    for (; length <= _remaining.size(); ++length) {
        auto &ch = _remaining[length - 1];

        // Start state machine
        switch (cur_state) {
        case _state::start:
            // Ignore whitespace
            switch (ch) {
            case ' ':
            case '\t':
                ++ignore_offset;
                continue;
            case '\n':
                switch (_previous_token.type) {
                case token::type_e::start_of_input:
                case token::type_e::curly_open:
                case token::type_e::exclamation:
                case token::type_e::stm_sep:
                    ++ignore_offset;
                    continue;
                default:
                {
                    // TODO maybe this is a bit overkill?
                    tokenizer tok{_remaining.substr(length)};
                    const auto next_tok = tok.look_next();

                    switch (next_tok.type) {
                    case token::type_e::exclamation:
                    case token::type_e::curly_close:
                    case token::type_e::end_of_input:
                        ++ignore_offset;
                        continue;
                    default:
                        return token{token::type_e::stm_sep};
                    }
                }
            }
            // Single-char tokens
            case '[':
                return token{token::type_e::bracket_open};
            case '{':
                return token{token::type_e::curly_open};
            case '(':
                return token{token::type_e::parenthesis_open};
            case ']':
                return token{token::type_e::bracket_close};
            case '}':
                return token{token::type_e::curly_close};
            case ')':
                return token{token::type_e::parenthesis_close};
            case ',':
                return token{token::type_e::comma};
            case '!':
                return token{token::type_e::exclamation};
            case ';':
                if (_previous_token.type != token::type_e::stm_sep)
                    return token{token::type_e::stm_sep};
                
                ++ignore_offset;
                continue;
            case '~':
                return token{token::type_e::tilde};
            case '|':
                return token{token::type_e::bar};
            case ':':
                return token{token::type_e::colon};
            // Special symbols
            case '?':
            case '+':
            case '*':
                return token::symbol(std::string{ch});
            case '>':
                cur_state = _state::gt_char;
                continue;
            case '<':
                cur_state = _state::lt_char;
                continue;
            case '=':
                cur_state = _state::eq_symbol;
                continue;
            case '/':
                cur_state = _state::slash_char;
                continue;
            case '\\':
                cur_state = _state::inverted_slash_char;
                continue;
            // Arrow or number
            case '-':
                if (
                    _previous_token.type == token::type_e::number ||
                    _previous_token.type == token::type_e::number_f
                )
                    return token::symbol("-");
                
                cur_state = _state::arrow_or_negative_number;
                continue;
            default:
                // Number
                if (ch >= '0' && ch <= '9')
                    cur_state = _state::number_integer_part;
                // Symbol
                else if (
                    (ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    ch == '_'
                )
                    cur_state = _state::alphanumeric_symbol;
                else {
                    length = _remaining.size();
                    return token{token::type_e::invalid};
                }
                
                continue;
            }
        case _state::arrow_or_negative_number:
            if (ch == '>')
                return token{token::type_e::arrow};
            if (ch >= '0' && ch <= '9') {
                cur_state = _state::number_integer_part;
                continue;
            }
            
            --length;
            return token::symbol("-");
        case _state::number_integer_part:
            switch (ch) {
            case '.':
                cur_state = _state::number_fractional_part;
                continue;
            default:
                if (!(ch >= '0' && ch <= '9')) {
                    --length;

                    return token::number(
                        std::stol(
                            _remaining.substr(0, length + 1)));
                }
                continue;
            }
        case _state::number_fractional_part:
            if (!(ch >= '0' && ch <= '9')) {
                --length;

                return token::number_f(
                    std::stod(
                        _remaining.substr(0, length + 1)));
            }
            continue;
        case _state::gt_char:
            switch (ch) {
            case '<':
                return token::symbol("><");
            case '=':
                return token::symbol(">=");
            }
            
            --length;
            return token::symbol(">");
        case _state::lt_char:
            if (ch == '=')
                return token::symbol("<=");
            
            --length;
            return token::symbol("<");
        case _state::eq_symbol:
            if (ch == '=')
                return token::symbol("==");
            
            --length;
            return token::symbol("=");
        case _state::slash_char:
            switch (ch) {
            case '=':
                return token::symbol("/=");
            case '\\':
                return token::symbol("/\\");
            }
            
            --length;
            return token::symbol("/");
        case _state::inverted_slash_char:
            if (ch == '/')
                return token::symbol("\\/");
            
            --length;
            return token::symbol("\\");
        case _state::alphanumeric_symbol:
            if (!(
                (ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '_'
            )) {
                --length;
                
                return token::symbol(
                    _remaining.substr(ignore_offset, length));
            }
            continue;
        }
    }

    --length;
    return token{token::type_e::end_of_input};
}
