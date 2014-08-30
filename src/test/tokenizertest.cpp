#include "tests_common.hpp"

#include <tokenizer.hpp>
#include <vector>

using namespace rbb;

TESTS_INIT()
    {
        tokenizer tok {"{ !10 }"};
        std::vector<token> expected {
            token{token::type_e::curly_open},
            token{token::type_e::exclamation},
            token::number(10),
            token{token::type_e::curly_close}
        };
        auto tok_all = tok.look_all();

        TEST_CONDITION(
            tok_all == expected,
            printf(
                "Unexpected tokenization (expected length: %d, result length: %d)\n",
                expected.size(), tok_all.size()))

        TEST_CONDITION(
            tok.look_next() == expected[0],
            puts("First token by look_next() isn't {"))

        TEST_CONDITION(
            tok.next() == expected[0],
            puts("First token by next() isn't {"))

        TEST_CONDITION(
            tok.look_next() == expected[1],
            puts("Next token by look_next() isn't !"))

        TEST_CONDITION(
            tok.next() == expected[1],
            puts("Next token by next() isn't !"))

        TEST_CONDITION(
            tok.look_next() == expected[2],
            puts("Next token by look_next() isn't 10"))

        TEST_CONDITION(
            tok.next() == expected[2],
            puts("Next token by next() isn't 10"))

        TEST_CONDITION(
            tok.look_next() == expected[3],
            puts("Next token by next() isn't }"))
    }

    {
        tokenizer tok{
            R"(
                ~:i -> 0
                ~:v -> |20|[]
                ~while~ {! ~i < 20 } {
                    ~v|~i, ~i * (~i)
                }
            )"
        };
        std::vector<token> expected {
            token{token::type_e::tilde},
            token{token::type_e::colon},
            token{token::type_e::bracket_open},
            token::symbol("i"),
            token{token::type_e::arrow},
            token::number(0),
            token{token::type_e::bracket_close},
            token{token::type_e::stm_sep},
            token{token::type_e::tilde},
            token{token::type_e::colon},
            token{token::type_e::bracket_open},
            token::symbol("v"),
            token{token::type_e::arrow},
            token{token::type_e::bar},
            token::number(20),
            token{token::type_e::bar},
            token{token::type_e::bracket_open},
            token{token::type_e::bracket_close},
            token{token::type_e::bracket_close},
            token{token::type_e::stm_sep},
            token{token::type_e::tilde},
            token::symbol("while"),
            token{token::type_e::tilde},
            token{token::type_e::curly_open},
            token{token::type_e::exclamation},
            token{token::type_e::tilde},
            token::symbol("i"),
            token::symbol("<"),
            token::number(20),
            token{token::type_e::curly_close},
            token{token::type_e::curly_open},
            token{token::type_e::tilde},
            token::symbol("v"),
            token{token::type_e::bar},
            token{token::type_e::bracket_open},
            token{token::type_e::tilde},
            token::symbol("i"),
            token{token::type_e::comma},
            token{token::type_e::tilde},
            token::symbol("i"),
            token::symbol("*"),
            token{token::type_e::parenthesis_open},
            token{token::type_e::tilde},
            token::symbol("i"),
            token{token::type_e::parenthesis_close},
            token{token::type_e::bracket_close},
            token{token::type_e::curly_close}
        };
        auto tok_all = tok.look_all();

        TEST_CONDITION(
            tok_all == expected,
            printf(
                "Unexpected tokenization (expected length: %d, result length: %d)\n",
                expected.size(), tok_all.size()))
    }
TESTS_END()
