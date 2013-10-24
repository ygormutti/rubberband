// Rubberband language
// Copyright (C) 2013  Luiz Romário Santana Rios <luizromario at gmail dot com>
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

#include "splay_tree.hpp"
#include "symbol.hpp"

#include <cmath>

#define SEND_MSG(typename)\
static object typename##_send_msg(object *thisptr, const object &msg)

#define DESTROY(typename)\
static void typename##_destroy(object *thisptr)

#define REF(typename)\
static void typename##_ref(object *thisptr)

#define DEREF(typename)\
static int typename##_deref(object *thisptr)

#define OBJECT_METHODS(typename)\
static object_methods *typename##_object_methods_ptr = 0; \
static object_methods *typename##_object_methods() \
{ \
    if (!typename##_object_methods_ptr) {\
        typename##_object_methods_ptr = new object_methods;\
        typename##_object_methods_ptr->send_msg = typename##_send_msg;\
        typename##_object_methods_ptr->destroy = typename##_destroy;\
        typename##_object_methods_ptr->ref = typename##_ref;\
        typename##_object_methods_ptr->deref = typename##_deref;\
    }\
    return typename##_object_methods_ptr;\
}

#define OBJECT_METHODS_NO_DATA(typename)\
DESTROY(typename) {}\
REF(typename) {}\
DEREF(typename) { return 0; }\
OBJECT_METHODS(typename)

using namespace rbb;

struct rbb::object_methods
{
    object (*send_msg)(object *, const object &);
    void (*destroy)(object *);
    void (*ref)(object *);
    int (*deref)(object *);
};

// object: The base for everything
SEND_MSG(noop)
{
    return empty();
}
OBJECT_METHODS_NO_DATA(noop)

object::object() :
    __m(noop_object_methods())
{}

object::object(const object &other) :
    __value(other.__value),
    __m(other.__m)
{
    ref();
}

static bool is_numeric(const value_t &val)
{
    return val.type == value_t::integer_t ||
           val.type == value_t::floating_t;
}

static bool is_atom(const value_t &val)
{
    return val.type == value_t::empty_t   ||
           is_numeric(val)                ||
           val.type == value_t::boolean_t ||
           val.type == value_t::symbol_t   ;
}

object::~object()
{
    if (!is_atom(__value) && deref() == 0)
        destroy();
}

bool object::operator==(const object& other) const
{
    if (!other.__value.type == __value.type)
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

object object::send_msg(const object &msg)
{
    return __m->send_msg(this, msg);
}

void object::destroy()
{
    __m->destroy(this);
}

void object::ref()
{
    __m->ref(this);
}

int object::deref()
{
    return __m->deref(this);
}

// empty: Empty object
SEND_MSG(empty_cmp_eq) { return boolean(msg.__value.type == value_t::empty_t); }
OBJECT_METHODS_NO_DATA(empty_cmp_eq)

SEND_MSG(empty_cmp_ne) { return boolean(msg.__value.type != value_t::empty_t); }
OBJECT_METHODS_NO_DATA(empty_cmp_ne)

SEND_MSG(empty)
{
    if (msg.__value.type != value_t::symbol_t)
        return empty();
    
    object cmp;
    
    if (msg == symbol("=="))
        cmp.__m = empty_cmp_eq_object_methods();
    else if (msg == symbol("!="))
        cmp.__m = empty_cmp_ne_object_methods();
    else
        return empty();

    return cmp;
}
OBJECT_METHODS_NO_DATA(empty)

object rbb::empty()
{
    object emp;
    emp.__value.type = value_t::empty_t;
    emp.__m = empty_object_methods();
    
    return emp;
}

// number: Numeric object
// TODO make comparison and arithmetic objects store their answerer value in data instead of
// integer or floating
double rbb::number_to_double(const object &num)
{
    if (!is_numeric(num.__value))
        return NAN;
    
    return num.__value.type == value_t::floating_t?
        num.__value.floating : (double) num.__value.integer;
}

static object num_operation(const object &thisobj, const object &msg,
                            object (*int_operation)(long, long),
                            object (*float_operation)(double, double))
{
    if (!thisobj.__value.type & value_t::number_t)
        return empty();

    if (thisobj.__value.type == value_t::integer_t &&
        msg.__value.type == value_t::integer_t
    ) {
        return int_operation(thisobj.__value.integer, msg.__value.integer);
    }

    double this_val = number_to_double(thisobj);
    double msg_val = number_to_double(msg);

    return float_operation(this_val, msg_val);
}

#define NUM_OPERATION(operation, symbol, ret_obj)\
object num_int_operation_##operation(long a, long b)\
{ return ret_obj(a symbol b); }\
object num_float_operation_##operation(double a, double b)\
{ return ret_obj(a symbol b); }\
\
SEND_MSG(num_op_##operation)\
{\
    return num_operation(*thisptr, msg,\
                         num_int_operation_##operation, num_float_operation_##operation);\
}\
OBJECT_METHODS_NO_DATA(num_op_##operation)

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

SEND_MSG(number)
{
    if (msg.__value.type != value_t::symbol_t)
        return empty();
    
    object comp;
    
    if (msg == symbol("=="))
        comp.__m = num_op_eq_object_methods();
    else if (msg == symbol("!="))
        comp.__m = num_op_ne_object_methods();
    else if (msg == symbol("<"))
        comp.__m = num_op_lt_object_methods();
    else if (msg == symbol(">"))
        comp.__m = num_op_gt_object_methods();
    else if (msg == symbol("<="))
        comp.__m = num_op_le_object_methods();
    else if (msg == symbol(">="))
        comp.__m = num_op_ge_object_methods();
    else if (msg == symbol("+"))
        comp.__m = num_op_add_object_methods();
    else if (msg == symbol("-"))
        comp.__m = num_op_sub_object_methods();
    else if (msg == symbol("*"))
        comp.__m = num_op_mul_object_methods();
    else if (msg == symbol("/"))
        comp.__m = num_op_div_object_methods();
    else
        return empty();
    
    comp.__value.type = thisptr->__value.type;
    switch (thisptr->__value.type) {
    case value_t::integer_t:
        comp.__value.integer = thisptr->__value.integer;
        break;
    case value_t::floating_t:
        comp.__value.floating = thisptr->__value.floating;
        break;
    default:
        return empty();
    }
    
    return comp;
}
OBJECT_METHODS_NO_DATA(number)

object rbb::number(double val)
{
    object num;
    num.__m = number_object_methods();
    
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
SEND_MSG(symbol_comp_eq)
{
    if (msg.__value.type != value_t::symbol_t)
        return empty();
    
    return thisptr->__value.symbol == msg.__value.symbol? boolean(true) : boolean(false);
}
OBJECT_METHODS_NO_DATA(symbol_comp_eq)

SEND_MSG(symbol_comp_ne)
{
    if (msg.__value.type != value_t::symbol_t)
        return empty();
    
    return thisptr->__value.symbol != msg.__value.symbol? boolean(true) : boolean(false);
}
OBJECT_METHODS_NO_DATA(symbol_comp_ne)

SEND_MSG(symbol)
{
    if (msg.__value.type != value_t::symbol_t)
        return empty();
    
    object cmp_op;
    cmp_op.__value.symbol = thisptr->__value.symbol;
    
    if (msg.__value.symbol == symbol_node::retrieve("=="))
        cmp_op.__m = symbol_comp_eq_object_methods();
    
    if (msg.__value.symbol == symbol_node::retrieve("!="))
        cmp_op.__m = symbol_comp_ne_object_methods();
    
    return cmp_op;
}
OBJECT_METHODS_NO_DATA(symbol)

object rbb::symbol(char* val)
{
    object symb;
    symb.__m = symbol_object_methods();
    symb.__value.type = value_t::symbol_t;
    symb.__value.symbol = symbol_node::retrieve(val);
    
    return symb;
}

object rbb::symbol(const char* val)
{
    object symb;
    symb.__m = symbol_object_methods();
    symb.__value.type = value_t::symbol_t;
    symb.__value.symbol = symbol_node::retrieve(val);
    
    return symb;
}

// boolean: Boolean object
SEND_MSG(boolean_comp)
{
    if (msg.__value.type != value_t::boolean_t)
        return boolean(false);
    
    if (msg.__value.boolean != thisptr->__value.boolean)
        return boolean(false);
    
    return boolean(true);
}
OBJECT_METHODS_NO_DATA(boolean_comp)

SEND_MSG(boolean)
{
    if (msg.__value.type != value_t::symbol_t)
        return empty();
    
    object comp_block;
    comp_block.__m = boolean_comp_object_methods();
    comp_block.__value.type = value_t::external_obj_t;
    
    if (msg.__value.symbol == symbol("==").__value.symbol) {
        comp_block.__value.boolean = thisptr->__value.boolean;
        return comp_block;
    }
    
    if (msg.__value.symbol == symbol("!=").__value.symbol) {
        comp_block.__value.boolean = !thisptr->__value.boolean;
        return comp_block;
    }
    
    return empty();
}
OBJECT_METHODS_NO_DATA(boolean)

object rbb::boolean(bool val)
{
    object b;
    b.__m = boolean_object_methods();
    b.__value.type = value_t::boolean_t;
    b.__value.boolean = val;
    
    return b;
}

// Comparison for any other objects
SEND_MSG(data_comparison_eq) { return rbb::boolean(thisptr->__value.data == msg.__value.data); }
OBJECT_METHODS_NO_DATA(data_comparison_eq) // thisptr's data is handled in the answerer

SEND_MSG(data_comparison_ne) { return rbb::boolean(thisptr->__value.data != msg.__value.data); }
OBJECT_METHODS_NO_DATA(data_comparison_ne) // thisptr's data is handled in the answerer

// list: Array of objects
struct list_data
{
    object *arr;
    int size;
    int refc;
};

static object_methods *list_object_methods();

static object create_list_object(list_data *d)
{
    object l;
    l.__value.type = value_t::list_t;
    l.__value.data = (void *) d;
    l.__m = list_object_methods();
    
    return l;
}

SEND_MSG(list_concatenation)
{
    if (msg.__value.type != value_t::list_t)
        return empty();
    
    list_data *d = reinterpret_cast<list_data *>(thisptr->__value.data);
    list_data *msg_d = reinterpret_cast<list_data *>(msg.__value.data);
    
    list_data *new_d = new list_data;
    new_d->refc = 1;
    new_d->size = d->size + msg_d->size;
    new_d->arr = new object[new_d->size];
    
    for (int i = 0; i < d->size; ++i)
        new_d->arr[i] = d->arr[i];
    
    for (int i = d->size, j = 0; j < msg_d->size; ++i, ++j)
        new_d->arr[i] = msg_d->arr[j];
    
    return create_list_object(new_d);
}
OBJECT_METHODS_NO_DATA(list_concatenation)

static int get_index_from_obj(const object &obj);

SEND_MSG(list_slicing)
{
    if (msg.__value.type != value_t::list_t)
        return empty();
    
    list_data *d = reinterpret_cast<list_data *>(thisptr->__value.data);
    list_data *msg_d = reinterpret_cast<list_data *>(msg.__value.data);
    
    if (msg_d->size < 2)
        return empty();
    
    int pos = get_index_from_obj(msg_d->arr[0]);
    int size = get_index_from_obj(msg_d->arr[1]);
    
    if (pos < 0 || size < 0)
        return empty();
    
    int this_len = d->size;
    
    if (pos + size > this_len)
        return empty();
    
    list_data *new_d = new list_data;
    new_d->refc = 1;
    new_d->size = size;
    new_d->arr = new object[size];
    
    for (int i = 0, j = pos; i < size; ++i, ++j)
        new_d->arr[i] = d->arr[j];
    
    return create_list_object(new_d);
}
OBJECT_METHODS_NO_DATA(list_slicing)

static int get_index_from_obj(const object &obj)
{
    if (!is_numeric(obj.__value))
        return -1;
    
    int ind;
    if (obj.__value.type == value_t::floating_t)
        return obj.__value.floating;
    
    return obj.__value.integer;
}

static bool in_bounds(list_data *d, int i)
{
    return i >= 0 && i < d->size;
}

SEND_MSG(list)
{
    list_data *d = reinterpret_cast<list_data *>(thisptr->__value.data);
    
    if (msg.__value.type == value_t::symbol_t) {
        if (msg == symbol("?|"))
            return rbb::number(d->size);
        
        object symb_ret;
        symb_ret.__value.type = value_t::block_t;
        symb_ret.__value.data = d;
        
        if (msg == symbol("=="))
            symb_ret.__m = data_comparison_eq_object_methods();
        if (msg == symbol("!="))
            symb_ret.__m = data_comparison_ne_object_methods();
        if (msg == symbol("+"))
            symb_ret.__m = list_concatenation_object_methods();
        if (msg == symbol("/"))
            symb_ret.__m = list_slicing_object_methods();
        
        return symb_ret;
    } else if (msg.__value.type == value_t::list_t) {
        list_data *msg_d = reinterpret_cast<list_data *>(msg.__value.data);
        if (msg_d->size < 2)
            return empty();
        
        int msg_ind = get_index_from_obj(msg_d->arr[0]);
        
        if (!in_bounds(d, msg_ind))
            return empty();
        
        d->arr[msg_ind] = msg_d->arr[1];
        
        return empty();
    }
    
    int ind = get_index_from_obj(msg);
    if (ind < 0)
        return empty();
    
    if (!in_bounds(d, ind))
        return empty();
    
    return d->arr[ind];
}

DESTROY(list)
{
    list_data *d = reinterpret_cast<list_data *>(thisptr->__value.data);
    
    delete[] d->arr;
    delete d;
    
}
REF(list)
{
    list_data *d = reinterpret_cast<list_data *>(thisptr->__value.data);
    
    ++d->refc;
}

DEREF(list)
{
    list_data *d = reinterpret_cast<list_data *>(thisptr->__value.data);
    
    return --d->refc;
}

OBJECT_METHODS(list)

object rbb::list(const object obj_array[], int size)
{
    list_data *d = new list_data;
    
    d->arr = new object[size];
    d->size = size;
    d->refc = 1;
    
    for (int i = 0; i < size; ++i)
        d->arr[i] = obj_array[i];
    
    return create_list_object(d);
}

// TODO block

// Generic object: Basically, a map from symbols to objects
struct symbol_object_pair
{
    symbol_node *sym;
    object obj;
};

class symbol_object_tree : public splay_tree<symbol_node *, symbol_object_pair>
{
public:
    symbol_object_tree() {}
    
protected:
    symbol_node *key_from_node(node *n) const { return n->item.sym; }
};

SEND_MSG(generic_object)
{
    
}

object rbb::generic_object(const object symbol_array[], const object obj_array[], int size)
{
    
}
