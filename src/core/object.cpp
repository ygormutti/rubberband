// Rubberband language
// Copyright (C) 2013--2015  Luiz Romário Santana Rios <luizromario at gmail dot com>
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

#include "object.hpp"

#include "block.hpp"
#include "error.hpp"
#include "object_private.hpp"
#include "symbol.hpp"

#include <cmath>
#include <vector>

#define SEND_MSG(typename)\
static object typename##_send_msg(object *thisptr, const object &msg)

using namespace rbb;

// Helper create_object functions
static object create_object(value_t::type_t type, send_msg_function send_msg = 0)
{
    object ret;
    ret.__value.type = type;
    ret.__send_msg = send_msg;

    return ret;
}

static object create_data_object(shared_data_t *data, send_msg_function send_msg = 0)
{
    object ret = create_object(value_t::data_t, send_msg);
    ret.__value.data = data;

    return ret;
}

// object: The base for everything
SEND_MSG(empty);

object::object() :
    __send_msg(empty_send_msg)
{}

object::object(const object &other)
{
    *this = other;
}

object::object(object&& other) :
    __value{other.__value},
    __send_msg{other.__send_msg}
{
    other.__value = value_t{};
}

object& object::operator=(const object& other)
{
    this->~object();

    __value = other.__value;
    __send_msg = other.__send_msg;
    ref();

    return *this;
}

static bool is_numeric(const value_t &val)
{
    return val.type == value_t::integer_t ||
           val.type == value_t::floating_t;
}

object::~object()
{
    if (deref() == 0)
        destroy();
}

bool object::operator==(const object& other) const
{
    if (other.__value.type != __value.type)
        return false;

    switch (__value.type) {
    case value_t::empty_t:
        return true;
    case value_t::integer_t:
        return __value.integer == other.__value.integer;
    case value_t::floating_t:
        return __value.floating == other.__value.floating;
    case value_t::boolean_t:
        return __value.boolean == other.__value.boolean;
    case value_t::symbol_t:
        return __value.symbol == other.__value.symbol;
    default:
        return __value.data == other.__value.data;
    }

    return false;
}

class object_data : public shared_data_t
{
public:
    object_data(const object &obj) :
        obj(obj)
    {}

    object obj;
};

// static symbols
namespace s_symb
{
    static auto &&double_lt = symbol("<<");
    static auto &&double_lt_question = symbol("<<?");
}

static bool in_bounds(array_data *, int);

SEND_MSG(responds_to)
{
    auto &object = static_cast<object_data *>(thisptr->__value.data)->obj;
    
    if (msg == symbol("==") || msg == symbol("!="))
        switch (object.__value.type) {
        case value_t::empty_t:
        case value_t::integer_t:
        case value_t::floating_t:
        case value_t::symbol_t:
        case value_t::boolean_t:
        case value_t::data_t:
            return boolean(true);
        default:
            return boolean(false);
        }
        
    
    switch (object.__value.type)
    {
    case value_t::integer_t:
    case value_t::floating_t:
        return boolean(
            msg == symbol("<")  ||
            msg == symbol(">")  ||
            msg == symbol("<=") ||
            msg == symbol(">=") ||
            msg == symbol("+")  ||
            msg == symbol("-")  ||
            msg == symbol("*")  ||
            msg == symbol("/")  ||
            (
                msg.__value.type == value_t::data_t &&
                dynamic_cast<array_data *>(msg.__value.data)
            ));
    case value_t::boolean_t:
        return boolean(
            msg == symbol("?")   ||
            msg == symbol("><")  ||
            msg == symbol("\\/") ||
            msg == symbol("/\\"));
    case value_t::data_t:
    {
        auto d = object.__value.data;
        if (dynamic_cast<array_data *>(d)) {
            auto arr_d = dynamic_cast<array_data *>(d);
            
            switch (msg.__value.type) {
            case value_t::symbol_t:
                return boolean(
                    msg == symbol("*") ||
                    msg == symbol("+") ||
                    msg == symbol("/"));
            case value_t::integer_t:
                return boolean(in_bounds(arr_d, msg.__value.integer));
            case value_t::floating_t:
                return boolean(in_bounds(arr_d, msg.__value.floating));
            case value_t::data_t:
            {
                auto msg_d = dynamic_cast<array_data *>(msg.__value.data);
                return boolean(
                    msg_d &&
                    msg_d->size == 2 &&
                    (
                        (
                            msg_d->arr[0].__value.type == value_t::integer_t &&
                            in_bounds(arr_d, msg_d->arr[0].__value.integer)
                        ) || (
                            msg_d->arr[0].__value.type == value_t::floating_t &&
                            in_bounds(arr_d, msg_d->arr[0].__value.floating)
                        )
                    ));
            }
            default:
                return boolean(false);
            }
        }
        
        if (dynamic_cast<table_data *>(d)) {
            auto table_d = dynamic_cast<table_data *>(d);
            
            switch (msg.__value.type) {
            case value_t::symbol_t:
                return boolean(
                    msg == symbol("+") ||
                    msg == symbol("-") ||
                    msg == symbol("*") ||
                    table_d->objtree.find(msg.__value.symbol) != table_d->objtree.end());
            case value_t::data_t:
                return boolean(dynamic_cast<table_data *>(msg.__value.data));
            default:
                return boolean(false);
            }
        }
        
        if (dynamic_cast<block_data *>(d)) {
            auto block_d = dynamic_cast<block_data *>(d);
            if (!block_d->block_l->_context_set)
                return boolean(true);
        }
        
        return {};
    }
    default:
        return boolean(false);
    }
}

object ask_if_obj_follows_interface(const object &obj, const object &msg)
{
    auto &&follows_interface_obj = obj.__send_msg(const_cast<object *>(&obj), symbol("<<?"));
    return boolean(
        follows_interface_obj.__send_msg(
            &follows_interface_obj,
            msg) ==
        boolean(true));
}

object follows_interface(const object &obj, const object &msg)
{
    switch (obj.__value.type) {
    case value_t::integer_t:
    case value_t::floating_t:
        return boolean(msg == symbol("<-0"));
    case value_t::empty_t:
        return boolean(msg == symbol("<-()"));
    case value_t::symbol_t:
        return boolean(msg == symbol("<-a"));
    case value_t::boolean_t:
        return boolean(msg == symbol("<-?"));
    case value_t::data_t:
    {
        auto d = obj.__value.data;

        if (dynamic_cast<array_data *>(d))
            return boolean(msg == symbol("<-|"));

        if (dynamic_cast<table_data *>(d))
            return boolean(msg == symbol("<-:"));

        auto block_d = dynamic_cast<block_data *>(d);
        if (block_d && !block_d->block_l->_context_set)
            return boolean(msg == symbol("<-{}"));

        // otherwise, delegate the question to the object itself
        try {
            return ask_if_obj_follows_interface(obj, msg);
        // FIXME maybe catching *all* exceptions isn't a good idea
        } catch (...) {
            return boolean(false);
        }
    }
    default:
        // do same thing here
        return ask_if_obj_follows_interface(obj, msg);
    }
}

SEND_MSG(follows_interface)
{
    auto &obj = static_cast<object_data *>(thisptr->__value.data)->obj;
    
    return follows_interface(obj, msg);
}

object object::operator<<(const object &msg)
{
    if (msg == s_symb::double_lt) {
        // In this case, this object is the responds_to method already
        if (__send_msg == responds_to_send_msg)
            return boolean(true);
        
        object responds_to_obj;
        responds_to_obj.__value.type = value_t::data_t;
        responds_to_obj.__value.data = new object_data{*this};
        responds_to_obj.__send_msg = responds_to_send_msg;
        
        return responds_to_obj;
    }
    
    if (msg == s_symb::double_lt_question) {
        if (__send_msg == responds_to_send_msg)
            return boolean(true);
        
        object follows_interface_obj;
        follows_interface_obj.__value.type = value_t::data_t;
        follows_interface_obj.__value.data = new object_data{*this};
        follows_interface_obj.__send_msg = follows_interface_send_msg;
        
        return follows_interface_obj;
    }
    
    return __send_msg(this, msg);
}

void object::destroy()
{
    if (__value.type == value_t::data_t)
        delete __value.data;
}

void object::ref()
{
    if (__value.type == value_t::data_t)
        ++__value.data->refc;
}

int object::deref()
{
    if (__value.type == value_t::data_t)
        return --__value.data->refc;

    return 1;
}

// empty: Empty object
SEND_MSG(empty_cmp_eq) { return follows_interface(msg, symbol("<-()")); }
SEND_MSG(empty_cmp_ne) { return boolean(follows_interface(msg, symbol("<-()")) != boolean(true)); }

SEND_MSG(empty)
{
    if (follows_interface(msg, symbol("<-a")) != boolean(true))
        return empty();

    object cmp;
    cmp.__value.type = value_t::no_data_t;

    if (msg == symbol("=="))
        cmp.__send_msg = empty_cmp_eq_send_msg;
    else if (msg == symbol("!="))
        cmp.__send_msg = empty_cmp_ne_send_msg;
    else
        throw message_not_recognized_error{*thisptr, msg};

    return cmp;
}

object rbb::empty()
{
    return object{};
}

// number: Numeric object
double rbb::number_to_double(const object &num)
{
    if (!is_numeric(num.__value))
        return NAN;

    return num.__value.type == value_t::floating_t?
        num.__value.floating : (double) num.__value.integer;
}

static object num_operation(const object &thisobj, const object &msg,
                            object (*int_operation)(long long, long long),
                            object (*float_operation)(double, double))
{
    object obj = static_cast<object_data *>(thisobj.__value.data)->obj;

    if (!is_numeric(obj.__value))
        return empty();

    if (obj.__value.type == value_t::integer_t &&
        msg.__value.type == value_t::integer_t
    ) {
        return int_operation(obj.__value.integer, msg.__value.integer);
    }

    double this_val = number_to_double(obj);
    double msg_val = number_to_double(msg);

    return float_operation(this_val, msg_val);
}

#define NUM_OPERATION(operation, symbol, ret_obj)\
object num_int_operation_##operation(long long a, long long b)\
{ return ret_obj(a symbol b); }\
object num_float_operation_##operation(double a, double b)\
{ return ret_obj(a symbol b); }\
\
SEND_MSG(num_op_##operation)\
{\
    return num_operation(*thisptr, msg,\
                         num_int_operation_##operation, num_float_operation_##operation);\
}

NUM_OPERATION(eq, ==, boolean)
NUM_OPERATION(ne, !=, boolean)
NUM_OPERATION(lt, <, boolean)
NUM_OPERATION(gt, >, boolean)
NUM_OPERATION(le, <=, boolean)
NUM_OPERATION(ge, >=, boolean)

NUM_OPERATION(add, +, number)
NUM_OPERATION(sub, -, number)
NUM_OPERATION(mul, *, number)
NUM_OPERATION(div, /, number)

SEND_MSG(array);

static object create_array_object(array_data *d)
{
    return create_data_object(d, array_send_msg);
}

static int get_index_from_obj(const object &);

SEND_MSG(number)
{
    if (follows_interface(msg, symbol("<-|")) == boolean(true)) {
        auto d = static_cast<array_data *>(msg.__value.data);
        auto new_d = new array_data{get_index_from_obj(*thisptr)};

        for (auto i = 0; i < d->size; ++i)
            new_d->arr[i] = d->arr[i];

        return create_array_object(new_d);
    }

    if (follows_interface(msg, symbol("<-a")) != boolean(true))
        throw message_not_recognized_error{*thisptr, msg};

    object comp = create_data_object(new object_data(*thisptr));

    if (msg == symbol("=="))
        comp.__send_msg = num_op_eq_send_msg;
    else if (msg == symbol("/="))
        comp.__send_msg = num_op_ne_send_msg;
    else if (msg == symbol("<"))
        comp.__send_msg = num_op_lt_send_msg;
    else if (msg == symbol(">"))
        comp.__send_msg = num_op_gt_send_msg;
    else if (msg == symbol("<="))
        comp.__send_msg = num_op_le_send_msg;
    else if (msg == symbol(">="))
        comp.__send_msg = num_op_ge_send_msg;
    else if (msg == symbol("+"))
        comp.__send_msg = num_op_add_send_msg;
    else if (msg == symbol("-"))
        comp.__send_msg = num_op_sub_send_msg;
    else if (msg == symbol("*"))
        comp.__send_msg = num_op_mul_send_msg;
    else if (msg == symbol("/"))
        comp.__send_msg = num_op_div_send_msg;
    else
        throw message_not_recognized_error{*thisptr, msg};

    return comp;
}

object rbb::number(double val)
{
    object num;
    num.__send_msg = number_send_msg;

    if (val - trunc(val) != 0) {
        num.__value.type = value_t::floating_t;
        num.__value.floating = val;
    } else {
        num.__value.type = value_t::integer_t;
        num.__value.integer = val;
    }

    return num;
}

// symbol: Symbol object
static object symbol_comp_send_msg(object *thisptr, const object &msg, bool eq)
{
    return static_cast<object_data *>(thisptr->__value.data)->obj == msg?
        boolean(eq) : boolean(!eq);
}

SEND_MSG(symbol_comp_eq) { return symbol_comp_send_msg(thisptr, msg, true); }

SEND_MSG(symbol_comp_ne) { return symbol_comp_send_msg(thisptr, msg, false); }

SEND_MSG(symbol)
{
    if (follows_interface(msg, symbol("<-a")) != boolean(true))
        throw message_not_recognized_error{*thisptr, msg};

    object cmp_op = create_data_object(new object_data(*thisptr));

    if (msg == symbol("=="))
        cmp_op.__send_msg = symbol_comp_eq_send_msg;
    else if (msg == symbol("!="))
        cmp_op.__send_msg = symbol_comp_ne_send_msg;
    else
        throw message_not_recognized_error{*thisptr, msg};

    return cmp_op;
}

object rbb::symbol(const std::string &val)
{
    object symb;
    symb.__send_msg = symbol_send_msg;
    symb.__value.type = value_t::symbol_t;
    symb.__value.symbol = symbol_node::retrieve(val);

    return symb;
}

// boolean: Boolean object
SEND_MSG(boolean_comp)
{
    if (follows_interface(msg, symbol("<-?")) != boolean(true))
        return boolean(false);

    if (msg.__value.boolean != thisptr->__value.boolean)
        return boolean(false);

    return boolean(true);
}

class boolean_decision_data : public shared_data_t
{
public:
    boolean_decision_data(const object &symt, const object &bool_obj) :
        context(symt),
        boolean_obj(bool_obj)
    {}

    object context;
    object boolean_obj;
    object true_result;
};

static constexpr const char block_name[] = "Block";

SEND_MSG(boolean_get_iffalse_block)
{
    boolean_decision_data *d = static_cast<boolean_decision_data *>(thisptr->__value.data);
    if (!d)
        return empty();

    object block = msg;
    if (follows_interface(msg, symbol("<-{}")) != boolean(true))
        throw wrong_type_error<block_name>{*thisptr, msg};

    return d->boolean_obj.__value.boolean?
        d->true_result :
        block << d->context << empty();
}

SEND_MSG(boolean_get_iftrue_block)
{
    boolean_decision_data *d = static_cast<boolean_decision_data *>(thisptr->__value.data);
    if (!d)
        return empty();

    boolean_decision_data *d_ret = new boolean_decision_data(d->context, d->boolean_obj);

    object block = msg;
    if (follows_interface(msg, symbol("<-{}")) != boolean(true))
        throw wrong_type_error<block_name>{*thisptr, msg};

    if (d->boolean_obj.__value.boolean)
        d_ret->true_result = block << d->context << empty();

    return create_data_object(d_ret, boolean_get_iffalse_block_send_msg);
}

SEND_MSG(boolean_get_context)
{
    object_data *d = static_cast<object_data *>(thisptr->__value.data);
    if (!d)
        return empty();

    return create_data_object(
        new boolean_decision_data(msg, d->obj),
        boolean_get_iftrue_block_send_msg);
}

#define BOOLEAN_DO(op, sym)\
SEND_MSG(boolean_do_##op)\
{\
    if (msg.__value.type != value_t::boolean_t)\
        return empty();\
\
    object_data *d = static_cast<object_data *>(thisptr->__value.data);\
    if (!d)\
        return empty();\
\
    return rbb::boolean(d->obj.__value.boolean sym msg.__value.boolean);\
}

BOOLEAN_DO(AND, &&)
BOOLEAN_DO(OR, ||)

SEND_MSG(boolean_raise)
{
    auto boolean_obj = static_cast<object_data *>(thisptr->__value.data)->obj;

    if (boolean_obj == boolean(true))
        throw rbb::runtime_error{msg};

    return {};
}

SEND_MSG(boolean)
{
    if (follows_interface(msg, symbol("<-a")) != boolean(true))
        throw message_not_recognized_error{*thisptr, msg};

    object comp_block = create_object(value_t::no_data_t, boolean_comp_send_msg);

    if (msg == symbol("==")) {
        comp_block.__value.boolean = thisptr->__value.boolean;
        return comp_block;
    } else if (msg == symbol("!=")) {
        comp_block.__value.boolean = !thisptr->__value.boolean;
        return comp_block;
    } else if (msg == symbol("/\\")) {
        return create_data_object(new object_data(*thisptr), boolean_do_AND_send_msg);
    } else if (msg == symbol("\\/")) {
        return create_data_object(new object_data(*thisptr), boolean_do_OR_send_msg);
    } else if (msg == symbol("><")) {
        return boolean(!thisptr->__value.boolean);
    } else if (msg == symbol("?")) {
        return create_data_object(new object_data(*thisptr), boolean_get_context_send_msg);
    } else if (msg == symbol("^")) {
        return create_data_object(new object_data{*thisptr}, boolean_raise_send_msg);
    }

    throw message_not_recognized_error{*thisptr, msg};
}

object rbb::boolean(bool val)
{
    object b;
    b.__send_msg = boolean_send_msg;
    b.__value.type = value_t::boolean_t;
    b.__value.boolean = val;

    return b;
}

// Comparison for any other objects
static object data_comparison_send_msg(object *thisptr, const object &msg, bool eq)
{
    object_data *d = static_cast<object_data *>(thisptr->__value.data);

    if (!d)
        return empty();

    return rbb::boolean((d->obj == msg) == eq);
}

SEND_MSG(data_comparison_eq) { return data_comparison_send_msg(thisptr, msg, true); }

SEND_MSG(data_comparison_ne) { return data_comparison_send_msg(thisptr, msg, false); }

// array: Array of objects
SEND_MSG(array);

static constexpr const char array_name[] = "Array";

SEND_MSG(array_concatenation)
{
    object d_obj = static_cast<object_data *>(thisptr->__value.data)->obj;
    array_data *d = static_cast<array_data *>(d_obj.__value.data);

    if (follows_interface(msg, symbol("<-|")) != boolean(true))
        throw wrong_type_error<array_name>{*thisptr, msg};

    array_data *msg_d = static_cast<array_data *>(msg.__value.data);
    array_data *new_d = new array_data(d->size + msg_d->size);

    for (int i = 0; i < d->size; ++i)
        new_d->arr[i] = d->arr[i];

    for (int i = d->size, j = 0; j < msg_d->size; ++i, ++j)
        new_d->arr[i] = msg_d->arr[j];

    return create_array_object(new_d);
}

static constexpr const char number_name[] = "Number";

SEND_MSG(array_slicing)
{
    if (msg.__value.type != value_t::data_t)
        throw wrong_type_error<array_name>{*thisptr, msg};

    object d_obj = static_cast<object_data *>(thisptr->__value.data)->obj;
    array_data *d = static_cast<array_data *>(d_obj.__value.data);

    if (follows_interface(msg, symbol("<-|")) != boolean(true))
        throw wrong_type_error<array_name>{*thisptr, msg};

    array_data *msg_d = static_cast<array_data *>(msg.__value.data);

    if (msg_d->size < 2)
        throw semantic_error{"Wrong number of arguments for array slicing", *thisptr, msg};

    if (
        !is_numeric(msg_d->arr[0].__value) ||
        !is_numeric(msg_d->arr[1].__value)
    )
        throw wrong_type_error<number_name>{*thisptr, msg};

    auto pos = get_index_from_obj(msg_d->arr[0]);
    auto size = get_index_from_obj(msg_d->arr[1]);
    auto this_len = d->size;

    if (pos + size > this_len)
        return empty();

    array_data *new_d = new array_data(size);

    for (int i = 0, j = pos; i < size; ++i, ++j)
        new_d->arr[i] = d->arr[j];

    return create_array_object(new_d);
}

static int get_index_from_obj(const object &obj)
{
    if (!is_numeric(obj.__value))
        return -1;

    if (obj.__value.type == value_t::floating_t)
        return obj.__value.floating;

    return obj.__value.integer;
}

static bool in_bounds(array_data *d, int i)
{
    return i >= 0 && i < d->size;
}

SEND_MSG(array)
{
    array_data *d = static_cast<array_data *>(thisptr->__value.data);

    if (follows_interface(msg, symbol("<-a")) == boolean(true)) {
        if (msg == symbol("*"))
            return rbb::number(d->size);

        object symb_ret = create_data_object(new object_data(*thisptr));

        if (msg == symbol("=="))
            symb_ret.__send_msg = data_comparison_eq_send_msg;
        else if (msg == symbol("!="))
            symb_ret.__send_msg = data_comparison_ne_send_msg;
        else if (msg == symbol("+"))
            symb_ret.__send_msg = array_concatenation_send_msg;
        else if (msg == symbol("/"))
            symb_ret.__send_msg = array_slicing_send_msg;
        else
            throw message_not_recognized_error{*thisptr, msg};

        return symb_ret;
    } else if (follows_interface(msg, symbol("<-|")) == boolean(true)) {
        array_data *msg_d = static_cast<array_data *>(msg.__value.data);

        if (msg_d->size < 2)
            throw semantic_error{"Wrong number of arguments for array assignment", *thisptr, msg};

        auto index = msg_d->arr[0];
        if (!is_numeric(index.__value))
            throw wrong_type_error<number_name>{*thisptr, msg};

        auto msg_ind = get_index_from_obj(index);
        if (!in_bounds(d, msg_ind))
            return empty();

        d->arr[msg_ind] = msg_d->arr[1];

        return empty();
    } else if (follows_interface(msg, symbol("<-0")) == boolean(true)) {
        auto ind = get_index_from_obj(msg);
        if (!in_bounds(d, ind))
            return empty();

        return d->arr[ind];
    }

    throw message_not_recognized_error{*thisptr, msg};
}

object rbb::array(const std::vector<object> &objects)
{
    array_data *d = new array_data(objects.size());
    int i = 0;

    for (auto obj : objects)
        d->arr[i++] = obj;

    return create_array_object(d);
}

// Table: Basically, a map from symbols to objects
SEND_MSG(table_merge)
{
    if (follows_interface(msg, symbol("<-:")) != boolean(true))
        throw wrong_type_error<block_name>{*thisptr, msg};

    object table = rbb::table();
    table << static_cast<object_data *>(thisptr->__value.data)->obj;
    table << msg;

    return table;
}

static constexpr const char symbol_name[] = "Symbol";

SEND_MSG(del_element)
{
    if (follows_interface(msg, symbol("<-a")) != boolean(true))
        throw wrong_type_error<symbol_name>{*thisptr, msg};

    auto &objtree =
        static_cast<table_data *>(
            static_cast<object_data *>(
                thisptr->__value.data)
            ->obj.__value.data)
        ->objtree;

    objtree.erase(msg.__value.symbol);

    return {};
}

SEND_MSG(table)
{
    table_data *d = static_cast<table_data *>(thisptr->__value.data);

    if (follows_interface(msg, symbol("<-a")) == boolean(true)) {
        object answer = create_data_object(new object_data(*thisptr));

        if (msg == rbb::symbol("=="))
            answer.__send_msg = data_comparison_eq_send_msg;
        else if (msg == rbb::symbol("!="))
            answer.__send_msg = data_comparison_ne_send_msg;
        else if (msg == rbb::symbol("+"))
            answer.__send_msg = table_merge_send_msg;
        else if (msg == rbb::symbol("-"))
            answer.__send_msg = del_element_send_msg;
        else if (msg == rbb::symbol("*")) {
            std::vector<object> l_el;

            for (auto pair : d->objtree) {
                object sym;
                sym.__value.type = value_t::symbol_t;
                sym.__value.symbol = pair.first;

                l_el.push_back(sym);
            }

            return rbb::array(l_el);
        } else {
            auto result = d->objtree.find(msg.__value.symbol);
            return result != d->objtree.end()? result->second : empty();
        }

        return answer;

    // Attribution (merging)
    } else if (follows_interface(msg, symbol("<-:")) == boolean(true)) {
        table_data *msg_table_d = static_cast<table_data *>(msg.__value.data);

        for (auto msg_pair : msg_table_d->objtree)
            d->objtree[msg_pair.first] = msg_pair.second;

        return *thisptr;
    } else if (follows_interface(msg, symbol("<-|")) == boolean(true)) {
        auto msg_array_d = static_cast<array_data *>(msg.__value.data);
        auto new_table = table();
        auto new_table_d = static_cast<table_data *>(new_table.__value.data);

        for (int i = 0; i < msg_array_d->size; ++i) {
            auto &&obj = msg_array_d->arr[i];
            if (obj.__value.type != value_t::symbol_t)
                throw message_not_recognized_error{*thisptr, msg};

            auto sym = obj.__value.symbol;
            if (d->objtree.find(sym) == d->objtree.end())
                continue;

            new_table_d->objtree[sym] = d->objtree[sym];
        }

        return new_table;
    }

    throw message_not_recognized_error{*thisptr, msg};
}

object rbb::table(
    const std::vector<object> &symbols,
    const std::vector<object> &objects)
{
    auto d = new table_data;
    const auto size = std::min(symbols.size(), objects.size());

    for (int i = 0; i < size; ++i) {
        if (symbols[i].__value.type != value_t::symbol_t)
            continue;

        auto sym = symbols[i].__value.symbol;
        d->objtree[sym] = objects[i];
    }

    return create_data_object(d, table_send_msg);
}

// Block: A sequence of instructions ready to be executed
SEND_MSG(block)
{
    block_data *d = static_cast<block_data *>(thisptr->__value.data);
    d->block_l->set_message(msg);

    return d->block_l->run();
}

SEND_MSG(block_body)
{
    block_data *d = static_cast<block_data *>(thisptr->__value.data);
    auto block_l = new literal::block(*d->block_l);
    block_l->set_context(msg);

    return create_data_object(new block_data(block_l), block_send_msg);
}

object rbb::literal::block::eval(literal::block *parent)
{
    auto block_l = new block{*this};
    if (parent)
        block_l->set_master(parent->_master);

    return create_data_object(new block_data(block_l), block_body_send_msg);
}
