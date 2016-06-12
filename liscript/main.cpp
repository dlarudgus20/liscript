// Copyright (c) 2014, 임경현
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * liscript는 lisp의 문법을 따온 prototype-based 언어입니다.
 *
 * expression의 평가는 다음 규칙을 따릅니다.
 *  1) number와 string이라면 그대로 평가
 *  2) atom keyword라면 정의된 동작대로
 *  3) identifier라면 (getf this [identifier])
 *  4) list라면
 *   4a) empty list라면 undefined
 *   4b) 첫째 항목이 list keyword라면 정의된 동작대로
 *   4c) 둘째 항목이 function이라면 첫째 항목을 this로, 셋째 항목 이후를 argument로 하여 함수 호출
 *   4d) 전부 아니라면 ListEvaluateError 예외를 던짐
 *
 * this값은 초기에는 global입니다. 함수를 호출할 때 this값을 지정할 수 있습니다.
 * ex) (setf this asdf (object)) // global에 새 object를 asdf라는 이름으로 넣음
 *     (asdf (func ()
 *         (setf this qwer (object))) // asdf에 새 object를 qwer라는 이름으로 넣음
 *
 * atom keyword: global this undefined null prev
 * list keyword: func setf getf setl getl do if while
 *
 * conditional로 쓰이는 값이 true이거나 null이 아닌 object라면 참으로 취급됩니다.
 * conditional로 쓰이는 값이 false, null, undefined라면 거짓으로 취급됩니다.
 * conditional로 쓰이는 값이 위 두 경우가 아니라면 invalid_conditional 예외가 발생합니다.
 *
 * [ ]로 표시된 것은 생략할 수 없는 항목입니다.
 * / /로 표시된 것은 생략 가능한 항목입니다.
 *
 * atom keywords
 *
 * global
 *  전역 객체입니다.
 *
 * this
 *  this 값입니다.
 *
 * undefined
 *  undefined 값입니다.
 *
 * null
 *  null 값입니다.
 *
 * list keywords
 *
 * func: (func (/par1/, ...) [expr])
 *  함수를 만듭니다.
 *
 * setf: (setf /object/ [name] [expr])
 *  object에 값을 넣습니다. object 항목이 생략됬다면 this에 넣습니다.
 *
 * getf: (getf /object/ [name])
 *  object에서 값을 가져옵니다. object 항목이 생략됬다면 this에서 가져옵니다.
 *
 * setl: (setl [name] [expr])
 *  지역 변수에 값을 넣습니다.
 *
 * getl: (getl [name])
 *  지역 변수에 값을 넣습니다.
 *
 * do: (do [expr1], /expr2/, ...)
 *  expr들을 순차적으로 평가합니다. do 구문의 값은 맨 마지막 expr의 값이 됩니다.
 *
 * if: (if [condition] [expr1] [expr2])
 *  condition이 참이라면 expr1을 평가한 값이 됩니다.
 *  condition이 거짓이라면 expr2를 평가한 값이 됩니다.
 *
 * while: (while [condition] [expr])
 *  condition이 참일 동인 expr를 반복 실행합니다. while 구문의 값은 마지막으로 평가된 값이 됩니다.
 **/

// boehm-gc

#ifdef _MSC_VER
# include <gc.h>
# include <gc_allocator.h>
#else
# include <gc/gc.h>
# include <gc/gc_allocator.h>
#endif

#include <iostream>
#include <functional>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#include <list>
#include <stack>
#include <unordered_map>
#include <memory>
#include <utility>
#include <stdexcept>
#include <new>
#include <cstdlib>
#include <cstdint>
#include <cctype>
#include <cstring>

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>

namespace io = boost::iostreams;

////////////////////////////////////////////////////////////////////////////////

#define MAKE_EXCEPTION(name,msg) \
	class name : public std::runtime_error \
	{ \
	public: \
		name() : std::runtime_error(msg) { } \
	}

MAKE_EXCEPTION(unexpected_eof_error, "unexpected end-of-file");
MAKE_EXCEPTION(unexpected_newline_error, "unexpected newline");
MAKE_EXCEPTION(invalid_escape_error, "invalid escape character");
MAKE_EXCEPTION(invalid_atom_error, "invalid atom token");
MAKE_EXCEPTION(invalid_number_error, "invalid number token");
MAKE_EXCEPTION(unexpected_character_error, "unexpected character error");
MAKE_EXCEPTION(list_evaluate_error, "list evaluate error");
MAKE_EXCEPTION(atom_evaluate_error, "atom evaluate error");
MAKE_EXCEPTION(keyword_evaluate_error, "keyword evaluate error");
MAKE_EXCEPTION(name_collison_error, "name collison error");

MAKE_EXCEPTION(invalid_keyword_list, "invalid keyword list");
MAKE_EXCEPTION(invalid_conditional, "invalid conditional");
MAKE_EXCEPTION(invalid_func_call, "invalid function call");
MAKE_EXCEPTION(not_object_error, "variable is not a object");
MAKE_EXCEPTION(not_string_error, "variable is not a string");
MAKE_EXCEPTION(not_function_error, "variable is not a function");

MAKE_EXCEPTION(null_reference_error, "null reference error");
MAKE_EXCEPTION(undefined_error, "undefined error");

#undef MAKE_EXCEPTION

////////////////////////////////////////////////////////////////////////////////

// forward declaration & type alias

struct s_object;
struct s_string;

template <typename T>
using gc_vector = std::vector<T, traceable_allocator<T>>;

////////////////////////////////////////////////////////////////////////////////

/**
 * expression은 liscript 언어의 구문 하나를 나타냅니다.
 * expression은 list, string, number, atom 4개 중 하나입니다.
 * list는 다른 expression을 괄호 속에 담고 있습니다. ex: (1 2 "asdf" (+ 3 4))
 * string은 문자열입니다. ex: "asdf"
 * number는 64비트 부동 소수점 값입니다. ex: 3.14
 * atom은 identifier과 keyword입니다. ex: function
 **/

enum class expr_type { list, string, number, atom };

struct expression
{
	std::weak_ptr<expression> root;

	expr_type type;
	std::vector<expression> list;
	union
	{
		s_string* value;
		double number;
	};
};

////////////////////////////////////////////////////////////////////////////////

/**
 * variable은 변수 하나를 저장하는 구조체입니다.
 * 변수의 타입은 boolean, number, undefined, object 4개입니다.
 * boolean은 true/false 진위형입니다.
 * number는 64비트 부동 소수점입니다
 * undefined는 미정의 값을 나타냅니다.
 * object는 참조형 타입입니다.
 **/

enum class var_type { boolean, number, undefined, object };

struct variable
{
	var_type type;
	union
	{
		bool v_boolean;
		double v_number;
		s_object* v_object;
		std::uint64_t raw;
	};

	static variable boolean(bool b)
	{
		variable ret;
		ret.type = var_type::boolean;
		ret.v_boolean = b;
		return ret;
	}
	static variable number(double d)
	{
		variable ret;
		ret.type = var_type::number;
		ret.v_number = d;
		return ret;
	}
	static variable undefined()
	{
		variable ret;
		ret.type = var_type::undefined;
		ret.raw = 0;
		return ret;
	}
	static variable object(s_object* obj)
	{
		variable ret;
		ret.type = var_type::object;
		ret.raw = 0;
		ret.v_object = obj;
		return ret;
	}
};

////////////////////////////////////////////////////////////////////////////////

/**
 * object는 참조형 값입니다.
 * object는 proto와 [string, variable] hashmap을 가집니다.
 * object의 멤버는 hashmap에 저장됩니다. 이 값은 변경할 수 있습니다.
 * object는 proto의 값에도 접근할 수 있습니다. 이 값은 변경할 수 없습니다.
 * proto 또한 object 값이므로 proto을 가집니다.
 * proto의 proto가 null이 아니라면 object는 재귀적으로 이들 또한 접근할 수 있습니다.
 * string, function, array는 object 타입이지만 특별 취급됩니다.
 **/

// hash & equal functor
struct pstr_hash
{
	std::size_t operator()(const s_string* str) const;
};
struct pstr_equal
{
	bool operator()(const s_string* str1, const s_string* str2) const;
};

// GC의 작동을 위해서는 traceable_allocator로 할당한 메모리에 참조를 저장해야 합니다.
using object_map_allocator = traceable_allocator<std::pair<s_string*, variable>>;
using object_map = std::unordered_map<s_string*, variable,
	pstr_hash, pstr_equal, object_map_allocator>;

enum class object_type { object, string, function, array };

struct s_object
{
	object_type type;
	s_object* proto;
	object_map vars;
};

struct s_string
{
	s_object _obj;
	const char* ptr;
	size_t size;

	s_object* obj() { return &_obj; }
};
inline std::size_t pstr_hash::operator()(const s_string* str) const
{
	return boost::hash_range(str->ptr, str->ptr + str->size);
}
inline bool pstr_equal::operator()(const s_string* str1, const s_string* str2) const
{
	return strcmp(str1->ptr, str2->ptr) == 0;
}

struct s_function
{
	s_object _obj;
	gc_vector<s_string*> parameters;

	bool is_native;
	union
	{
		const expression* expr;
		variable (*native_fn)();
	};
	std::shared_ptr<expression> expr_root;

	s_object* obj() { return &_obj; }
};

struct s_array
{
	s_object _obj;
	gc_vector<variable> vector;
};

////////////////////////////////////////////////////////////////////////////////

// undefined를 나타내는 expression입니다.
expression empty_expr;

// predefined objects
s_object* global_object;
variable this_var;

// Object, Function, String, Array constructor
s_object* f_Object;
s_object* f_Function;
s_object* f_String;
s_object* f_Array;

////////////////////////////////////////////////////////////////////////////////

/**
 * 지역변수가 들어있는 stackframe입니다.
 * local_vars는 함수 단위의 frame입니다.
 * local_vars 안의 blocks는 블록 단위의 frame입니다.
 **/

template <typename T>
using list_stack = std::stack<T, std::list<T>>;

struct local_vars
{
	s_array* arguments;
	list_stack<object_map> blocks;
};

list_stack<local_vars> stackframe;

////////////////////////////////////////////////////////////////////////////////

/**
 * object, string, function의 할당/생성 함수입니다.
 * allocate 함수는 GC 할당과 초기화만 합니다.
 * create 함수는 allocate를 호출한 후 proto 값을 연결합니다.
 **/

s_object* allocate_object();
s_object* create_object();

s_string* allocate_string(const std::string& str);
s_string* create_string(const std::string& str);

s_function* allocate_function(const gc_vector<s_string*>& parameters, const expression& expr);
s_function* create_function(const gc_vector<s_string*>& parameters, const expression& expr);

s_array* allocate_array();
s_array* create_array();

////////////////////////////////////////////////////////////////////////////////

/**
 * 초기화, expr 구문분석, expr 평가
 **/

void init_scripting();

bool read_expr(std::istream& strm, expression& ret, const std::weak_ptr<expression>& root);

// 정의부는 eval_expr() 바로 위쪽에
struct eval_context;
variable eval_expr(const expression& expr);

bool to_conditional(variable var);

////////////////////////////////////////////////////////////////////////////////

/**
 * 출력 함수
 **/

void print_var(std::ostream& strm, variable var, int indent = 0);
void dump_expr(const expression& expr, int indent = 0);

////////////////////////////////////////////////////////////////////////////////

// Read-Eval-Print-Loop를 위한 boost.iostream source입니다.
class repl_source
{
public:
	typedef char char_type;
	typedef io::source_tag category;

	std::streamsize read(char* s, std::streamsize n)
	{
		int ch = getc();
		if (ch == -1)
			return -1;

		*s = (char)ch;
		return 1;
	}

	void reset_prompt()
	{
		first_newline_ = true;
	}

	void clear()
	{
		line_.clear();
		idx_ = 1;
	}

private:
	int getc()
	{
		if (idx_ > line_.size())
		{
			if (first_newline_)
			{
				std::cout << ">> ";
				first_newline_ = false;
			}
			else
			{
				std::cout << "-- ";
			}

			getline(std::cin, line_);
			idx_ = 0;

			if (std::cin.eof())
				return -1;
		}

		if (idx_ == line_.size())
		{
			++idx_;
			return '\n';
		}
		else
		{
			return line_[idx_++];
		}
	}

	std::string line_;
	std::string::size_type idx_ { 1 };
	bool first_newline_ { true };
};

////////////////////////////////////////////////////////////////////////////////

int main()
{
	io::stream<repl_source> strm;
	strm.open(repl_source { });

	init_scripting();

	while (true)
	{
		auto expr = std::make_shared<expression>();
		std::weak_ptr<expression> w_expr = expr;

		strm->reset_prompt();
		try
		{
			if (read_expr(strm, *expr, w_expr))
			{
				int ch = io::get(strm);
				if (ch != -1 && ch != '\n')
				{
					strm->clear();
					throw unexpected_character_error();
				}

				dump_expr(*expr);

				variable var = eval_expr(*expr);

				print_var(std::cout, var);
				std::cout << std::endl;
			}
			else if (strm.eof())
			{
				break;
			}
		}
		catch (std::runtime_error& ex)
		{
			std::cerr << ex.what() << std::endl;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

s_object* allocate_object()
{
	s_object* obj = (s_object*)GC_MALLOC(sizeof(s_object));
	new (obj) s_object();

	obj->type = object_type::object;

	GC_REGISTER_FINALIZER(obj, [](void* r_obj, void* cdata)
	{
		delete (s_object*)r_obj;
	}, nullptr, nullptr, nullptr);

	return obj;
}

s_object* create_object()
{
	s_object* obj = allocate_object();
	obj->proto = f_Object->proto;
	return obj;
}

s_string* allocate_string(const std::string& str)
{
	s_string* obj = (s_string*)GC_MALLOC(sizeof(s_string) + str.size() + 1);
	new (obj) s_string();

	obj->_obj.type = object_type::string;
	obj->ptr = (char*)obj + sizeof(s_string);
	obj->size = str.size();
	std::memcpy((char*)obj->ptr, str.c_str(), str.size() + 1);

	GC_REGISTER_FINALIZER(obj, [](void* r_obj, void* cdata) {
		delete (s_string*)r_obj;
	}, nullptr, nullptr, nullptr);

	return obj;
}

s_string* create_string(const std::string& str)
{
	s_string* obj = allocate_string(str);
	obj->_obj.proto = f_String->proto;
	return obj;
}

s_function* allocate_function(const gc_vector<s_string*>& parameters, const expression& expr)
{
	s_function* obj = (s_function*)GC_MALLOC(sizeof(s_function));
	new (obj) s_function();

	obj->_obj.type = object_type::function;
	obj->parameters = parameters;
	obj->expr = &expr;
	obj->expr_root = expr.root.lock();

	GC_REGISTER_FINALIZER(obj, [](void* r_obj, void* cdata) {
		delete (s_function*)r_obj;
	}, nullptr, nullptr, nullptr);

	return obj;
}

s_function* create_function(const gc_vector<s_string*>& parameters, const expression& expr)
{
	s_function* obj = allocate_function(parameters, expr);
	obj->_obj.proto = f_Function->proto;
	return obj;
}

s_array* allocate_array()
{
	s_array* obj = (s_array*)GC_MALLOC(sizeof(s_array));
	new (obj) s_array();

	obj->_obj.type = object_type::array;

	GC_REGISTER_FINALIZER(obj, [](void* r_obj, void* cdata) {
		delete (s_array*)r_obj;
	}, nullptr, nullptr, nullptr);

	return obj;
}

s_array* create_array()
{
	s_array* obj = allocate_array();
	obj->_obj.proto = f_Array->proto;
	return obj;
}

////////////////////////////////////////////////////////////////////////////////

void init_scripting()
{
	GC_INIT();
	GC_set_finalize_on_demand(0/*false*/);

	empty_expr.type = expr_type::list;

	f_Object = allocate_function({ }, empty_expr)->obj();
	f_Object->proto = allocate_object();
	f_Object->proto->proto = nullptr;

	f_Function = allocate_function({ }, empty_expr)->obj();
	f_Function->proto = allocate_object();
	f_Function->proto->proto = f_Object->proto;

	f_String = allocate_function({ }, empty_expr)->obj();
	f_String->proto = allocate_object();
	f_String->proto->proto = f_Object->proto;

	f_Array = allocate_function({ }, empty_expr)->obj();
	f_Array->proto = allocate_object();
	f_Array->proto->proto = f_Object->proto;

	s_string* tn = create_string("__typename__");
	f_Object->proto->vars[tn] = variable::object(create_string("object")->obj());
	f_Function->proto->vars[tn] = variable::object(create_string("function")->obj());
	f_String->proto->vars[tn] = variable::object(create_string("string")->obj());
	f_Array->proto->vars[tn] = variable::object(create_string("array")->obj());

	global_object = create_object();
	global_object->vars[create_string("Object")] = variable::object(f_Object);
	global_object->vars[create_string("Function")] = variable::object(f_Function);
	global_object->vars[create_string("String")] = variable::object(f_String);
	global_object->vars[create_string("Array")] = variable::object(f_Array);

	this_var = variable::object(global_object);
}

bool read_expr(std::istream& strm, expression& ret, const std::weak_ptr<expression>& root)
{
	auto getc = [&strm]
	{
		if (strm.eof())
			return -1;
		int ch = io::get(strm);
		return ch;
	};

	int ch;

	do
	{
		ch = getc();
		if (ch == '\n' || ch == -1)
			return false;
	}
	while (std::isspace(ch));

	std::string value;

	if (ch == '"')
	{
		ret.type = expr_type::string;

		while (true)
		{
			ch = getc();
			if (ch == -1)
				return false;

			if (ch == '"')
			{
				break;
			}
			else if (ch == '\n')
			{
				throw unexpected_newline_error();
			}
			else if (ch == '\\')
			{
				ch = getc();
				if (ch == -1)
					throw unexpected_eof_error();

				static char tbl[256] = { 0 };
				tbl['t'] = '\t';
				tbl['n'] = '\n';
				tbl['\\'] = '\\';

				if (tbl[ch] != 0)
					value.push_back(tbl[ch]);
				else
					throw invalid_escape_error();
			}
			else if (std::isspace(ch))
			{
				value.push_back(' ');
			}
			else
			{
				value.push_back(ch);
			}
		}
	}
	else if (ch == '(')
	{
		ret.type = expr_type::list;

		while (true)
		{
			ch = getc();
			if (ch == -1)
				throw unexpected_eof_error();

			if (std::isspace(ch))
			{
				continue;
			}
			else if (ch == ')')
			{
				break;
			}
			else
			{
				strm.unget();

				ret.list.emplace_back();
				read_expr(strm, ret.list.back(), root);
			}
		}
	}
	else
	{
		while (true)
		{
			if (ch == '(' || ch == ')')
			{
				strm.unget();
				break;
			}
			else if (std::isspace(ch))
			{
				strm.unget();
				break;
			}
			else if (std::isgraph(ch))
			{
				value.push_back(ch);
			}
			else
			{
				if (!value.empty() && std::isdigit(value[0]))
					throw invalid_number_error();
				else
					throw invalid_atom_error();
			}

			ch = getc();
			if (ch == -1)
				throw unexpected_eof_error();
		}

		if (!std::isdigit(value[0]))
		{
			ret.type = expr_type::atom;
		}
		else
		{
			char* endptr;
			double num = std::strtod(value.c_str(), &endptr);
			if (*endptr != '\0')
				throw invalid_number_error();

			ret.type = expr_type::number;
			ret.number = num;
		}
	}

	if (ret.type != expr_type::number)
	{
		ret.value = create_string(value);
	}
	ret.root = root;

	return true;
}

struct eval_context
{
};

// atom keywords
variable eval_expr_keyword_global(eval_context& context);
variable eval_expr_keyword_this(eval_context& context);
variable eval_expr_keyword_undefined(eval_context& context);
variable eval_expr_keyword_null(eval_context& context);

// list keywords
variable eval_expr_keyword_func(const expression& expr, eval_context& context);
variable eval_expr_keyword_setf(const expression& expr, eval_context& context);
variable eval_expr_keyword_getf(const expression& expr, eval_context& context);
variable eval_expr_keyword_setl(const expression& expr, eval_context& context);
variable eval_expr_keyword_getl(const expression& expr, eval_context& context);
variable eval_expr_keyword_do(const expression& expr, eval_context& context);
variable eval_expr_keyword_if(const expression& expr, eval_context& context);
variable eval_expr_keyword_while(const expression& expr, eval_context& context);

using atom_keyword_map_t = std::unordered_map<
	std::string, std::function<variable(eval_context& context)>>;
using list_keyword_map_t = std::unordered_map<
	std::string, std::function<variable(const expression& expr, eval_context& context)>>;
atom_keyword_map_t atom_keyword_map = {
	{ "global",		eval_expr_keyword_global },
	{ "this",		eval_expr_keyword_this },
	{ "undefined",	eval_expr_keyword_undefined },
	{ "null",		eval_expr_keyword_null },
};
list_keyword_map_t list_keyword_map = {
	{ "func",		eval_expr_keyword_func },
	{ "setf",		eval_expr_keyword_setf },
	{ "getf",		eval_expr_keyword_getf },
	{ "setl",		eval_expr_keyword_setl },
	{ "getl",		eval_expr_keyword_getl },
	{ "do",			eval_expr_keyword_do },
	{ "if",			eval_expr_keyword_if },
	{ "while",		eval_expr_keyword_while },
};

bool is_keyword(const std::string &str)
{
	return (atom_keyword_map.find(str) != atom_keyword_map.end()
		|| list_keyword_map.find(str) != list_keyword_map.end());
}

variable eval_expr(const expression& expr)
{
	eval_context context;

	if (expr.type == expr_type::string)
	{
		return variable::object(expr.value->obj());
	}
	else if (expr.type == expr_type::number)
	{
		return variable::number(expr.number);
	}
	else if (expr.type == expr_type::atom)
	{
		auto it = atom_keyword_map.find(expr.value->ptr);
		if (it != atom_keyword_map.end())
		{
			return it->second(context);
		}
		else
		{
			// (getf [expr])

			if (this_var.type != var_type::object)
				throw not_object_error();
			s_object* obj = this_var.v_object;

			if (obj == nullptr)
				throw null_reference_error();

			do
			{
				auto it = obj->vars.find(expr.value);
				if (it != obj->vars.end())
					return it->second;

				obj = obj->proto;
			} while (obj != nullptr);

			return variable::undefined();
		}
	}
	else //if (expr.type == expr_type::list)
	{
		if (expr.list.empty())
			return variable::undefined();

		auto& front = expr.list.front();

		if (front.type == expr_type::atom)
		{
			auto it = list_keyword_map.find(front.value->ptr);
			if (it != list_keyword_map.end())
			{
				return it->second(expr, context);
			}
		}

		// member function call

		if (expr.list.size() <= 1)
			throw invalid_func_call();

		variable var = eval_expr(expr.list[0]);

		if (var.type != var_type::object)
			throw not_object_error();
		if (var.v_object == nullptr)
			throw null_reference_error();
		s_object* obj = var.v_object;

		if (expr.list[1].type != expr_type::atom)
			throw invalid_func_call();

		variable fn = variable::undefined();
		do
		{
			auto it = obj->vars.find(expr.list[1].value);
			if (it != obj->vars.end())
			{
				fn = it->second;
				break;
			}
			obj = obj->proto;
		} while (obj != nullptr);

		if (fn.type == var_type::undefined)
			throw undefined_error();
		if (fn.type != var_type::object)
			throw not_function_error();
		if (fn.v_object->type != object_type::function)
			throw not_function_error();

		s_function* f_fn = (s_function*)fn.v_object;
		if (!f_fn->is_native)
		{
			std::abort(); // TODO
		}
		else
		{
			std::abort(); // TODO
		}
	}
}

// atom keyword handler

variable eval_expr_keyword_global(eval_context& context)
{
	return variable::object(global_object);
}

variable eval_expr_keyword_this(eval_context& context)
{
	return this_var;
}

variable eval_expr_keyword_undefined(eval_context& context)
{
	return variable::undefined();
}

variable eval_expr_keyword_null(eval_context& context)
{
	return variable::object(nullptr);
}

// list keyword handler

variable eval_expr_keyword_func(const expression& expr, eval_context& context)
{
	if (expr.list.size() != 3)
		throw invalid_keyword_list();
	if (expr.list[1].type != expr_type::list)
		throw invalid_keyword_list();

	gc_vector<s_string*> par;
	for (const auto& p : expr.list[1].list)
	{
		if (p.type != expr_type::atom)
			throw invalid_keyword_list();
		if (is_keyword(p.value->ptr))
			throw invalid_atom_error();

		par.emplace_back(p.value);
	}

	return variable::object(create_function(par, expr.list[2])->obj());
}

variable eval_expr_keyword_setf(const expression& expr, eval_context& context)
{
	s_object* obj;
	const expression* expr_name;
	const expression* expr_val;

	if (expr.list.size() == 4)
	{
		variable tmp = eval_expr(expr.list[1]);
		if (tmp.type != var_type::object)
			throw not_object_error();
		obj = tmp.v_object;

		expr_name = &expr.list[2];
		expr_val = &expr.list[3];
	}
	else if (expr.list.size() == 3)
	{
		if (this_var.type != var_type::object)
			throw not_object_error();
		obj = this_var.v_object;

		expr_name = &expr.list[1];
		expr_val = &expr.list[2];
	}
	else
	{
		throw invalid_keyword_list();
	}

	s_string* name;

	if (expr_name->type == expr_type::atom)
	{
		name = expr_name->value;
	}
	else
	{
		variable tmp = eval_expr(*expr_name);
		if (tmp.type != var_type::object)
			throw not_object_error();
		if (tmp.v_object->type != object_type::string)
			throw not_string_error();
		name = (s_string*)tmp.v_object;
	}

	if (obj == nullptr)
		throw null_reference_error();

	variable val = eval_expr(*expr_val);
	obj->vars[name] = val;

	return val;
}

variable eval_expr_keyword_getf(const expression& expr, eval_context& context)
{
	s_object* obj;
	const expression* expr_name;

	if (expr.list.size() == 3)
	{
		variable tmp = eval_expr(expr.list[1]);
		if (tmp.type != var_type::object)
			throw not_object_error();
		obj = tmp.v_object;

		expr_name = &expr.list[2];
	}
	else if (expr.list.size() == 2)
	{
		if (this_var.type != var_type::object)
			throw not_object_error();
		obj = this_var.v_object;

		expr_name = &expr.list[1];
	}
	else
	{
		throw invalid_keyword_list();
	}

	s_string* name;

	if (expr_name->type == expr_type::atom)
	{
		name = expr_name->value;
	}
	else
	{
		variable tmp = eval_expr(*expr_name);
		if (tmp.type != var_type::object)
			throw not_object_error();
		if (tmp.v_object->type != object_type::string)
			throw not_string_error();
		name = (s_string*)tmp.v_object;
	}

	if (obj == nullptr)
		throw null_reference_error();

	do
	{
		auto it = obj->vars.find(name);
		if (it != obj->vars.end())
			return it->second;

		obj = obj->proto;
	} while (obj != nullptr);

	return variable::undefined();
}

variable eval_expr_keyword_setl(const expression& expr, eval_context& context)
{
	if (expr.list.size() != 3)
		throw invalid_keyword_list();

	object_map *mp;

	if (stackframe.empty())
		mp = &global_object->vars;
	else
		mp = &stackframe.top().blocks.top();

	s_string* name;

	if (expr.list[1].type == expr_type::atom)
	{
		name = expr.list[1].value;
	}
	else
	{
		variable tmp = eval_expr(expr.list[1]);
		if (tmp.type != var_type::object)
			throw not_object_error();
		if (tmp.v_object->type != object_type::string)
			throw not_string_error();
		name = (s_string*)tmp.v_object;
	}

	variable val = eval_expr(expr.list[2]);
	(*mp)[name] = val;

	return val;
}

variable eval_expr_keyword_getl(const expression& expr, eval_context& context)
{
	if (expr.list.size() != 2)
		throw invalid_keyword_list();

	object_map *mp;

	if (stackframe.empty())
		mp = &global_object->vars;
	else
		mp = &stackframe.top().blocks.top();

	s_string* name;

	if (expr.list[1].type == expr_type::atom)
	{
		name = expr.list[1].value;
	}
	else
	{
		variable tmp = eval_expr(expr.list[1]);
		if (tmp.type != var_type::object)
			throw not_object_error();
		if (tmp.v_object->type != object_type::string)
			throw not_string_error();
		name = (s_string*)tmp.v_object;
	}

	auto it = mp->find(name);
	if (it != mp->end())
		return it->second;
	else
		return variable::undefined();
}

variable eval_expr_keyword_do(const expression& expr, eval_context& context)
{
	if (expr.list.size() <= 1)
		throw invalid_keyword_list();

	variable ret;

	for (auto it = std::next(expr.list.cbegin()); it != expr.list.cend(); ++it)
	{
		ret = eval_expr(*it);
	}

	return ret;
}

variable eval_expr_keyword_if(const expression& expr, eval_context& context)
{
	if (expr.list.size() != 4)
		throw invalid_keyword_list();

	bool cond = to_conditional(eval_expr(expr.list[1]));
	if (cond)
	{
		return eval_expr(expr.list[2]);
	}
	else
	{
		return eval_expr(expr.list[3]);
	}
}

variable eval_expr_keyword_while(const expression& expr, eval_context& context)
{
	if (expr.list.size() != 3)
		throw invalid_keyword_list();

	variable ret;

	while (to_conditional(eval_expr(expr.list[1])))
	{
		ret = eval_expr(expr.list[2]);
	}

	return ret;
}

bool to_conditional(variable var)
{
	if ((var.type == var_type::boolean && var.v_boolean)
		|| (var.type == var_type::object && var.v_object != nullptr))
	{
		return true;
	}
	else if ((var.type == var_type::boolean && !var.v_boolean)
		|| (var.type == var_type::object && var.v_object == nullptr)
		|| var.type == var_type::undefined)
	{
		return false;
	}
	else
	{
		throw invalid_conditional();
	}
}

////////////////////////////////////////////////////////////////////////////////

void print_var(std::ostream& strm, variable var, int indent /* = 0 */)
{
	if (var.type == var_type::boolean)
	{
		strm << (var.v_boolean ? "true" : "false");
	}
	else if (var.type == var_type::number)
	{
		strm << var.v_number;
	}
	else if (var.type == var_type::undefined)
	{
		strm << "(undefined)";
	}
	else //if (var.type == var_type::object)
	{
		if (var.v_object == nullptr)
		{
			strm << "(null)";
		}
		else if (var.v_object->type == object_type::string)
		{
			strm << '"' << ((s_string*)var.v_object)->ptr << '"';
		}
		else if (var.v_object->type == object_type::function)
		{
			strm << "(func (";

			bool first = true;
			for (const auto& p : ((s_function*)var.v_object)->parameters)
			{
				if (!first)
					strm << ", ";
				first = false;

				strm << p->ptr;
			}

			strm << " ) (...))";
		}
		else //if (var.v_object->type == object_type::object)
		{
			if (var.v_object->vars.empty())
			{
				strm << "{ }";
			}
			else
			{
				std::string str_indent((indent + 1) * 2, ' ');
				bool first = true;

				for (const auto& pr : var.v_object->vars)
				{
					if (first)
						strm << "{\n" << str_indent;
					else
						strm << ",\n" << str_indent;
					first = false;

					strm << pr.first;
					print_var(strm, pr.second, indent + 1);
				}
				strm << '\n' << str_indent << '}';
			}
		}
	}
}

void dump_expr(const expression& expr, int indent /* = 0 */)
{
	std::string str_indent(indent * 2, ' ');

	std::cout << str_indent;

	if (expr.type == expr_type::atom)
	{
		std::cout << "[atom] " << expr.value->ptr << "\n";
	}
	else if (expr.type == expr_type::string)
	{
		std::cout << "[string] " << expr.value->ptr << "\n";
	}
	else if (expr.type == expr_type::number)
	{
		std::cout << "[number] " << expr.number << "\n";
	}
	else
	{
		if (expr.list.empty())
		{
			std::cout << "( )\n";
		}
		else
		{
			std::cout << "(\n";
			for (const auto& sub : expr.list)
			{
				dump_expr(sub, indent + 1);
			}
			std::cout << str_indent << ")\n";
		}
	}
}
