#include <clang-cpp-frontend/clang_cpp_adjust.h>
#include <util/c_sizeof.h>
#include <util/destructor.h>
#include <util/expr_util.h>

clang_cpp_adjust::clang_cpp_adjust(contextt &_context)
  : clang_c_adjust(_context)
{
}

void clang_cpp_adjust::adjust_symbol(symbolt &symbol)
{
  clang_c_adjust::adjust_symbol(symbol);

  /*
   * implicit code generation for vptr initializations:
   * The idea is to get the constructor method symbol and
   * add implicit code to set each virtual pointer of this
   * class to point to the corresponding virtual table.
   */
  gen_vptr_initializations(symbol);
}

void clang_cpp_adjust::adjust_side_effect(side_effect_exprt &expr)
{
  const irep_idt &statement = expr.statement();

  if(statement == "cpp_new" || statement == "cpp_new[]")
  {
    adjust_new(expr);
  }
  else if(statement == "cpp_delete" || statement == "cpp_delete[]")
  {
    // adjust side effect node to explicitly call class destructor
    // e.g. the adjustment here will add the following instruction in GOTO:
    // FUNCTION_CALL:  ~t2(&(*p))
    code_function_callt destructor = get_destructor(ns, expr.type());
    if(destructor.is_not_nil())
    {
      exprt new_object("new_object", expr.type());
      new_object.cmt_lvalue(true);

      destructor.arguments().push_back(address_of_exprt(new_object));
      expr.set("destructor", destructor);
    }
  }
  else if(statement == "temporary_object")
  {
    exprt &initializer = static_cast<exprt &>(expr.add("initializer"));

    side_effect_expr_function_callt &constructor_call =
      to_side_effect_expr_function_call(initializer.op0());

    adjust_function_call_arguments(constructor_call);
  }
  else if(statement == "assign")
  {
    adjust_side_effect_assign(expr);
  }
  else
    clang_c_adjust::adjust_side_effect(expr);
}

void clang_cpp_adjust::adjust_new(exprt &expr)
{
  if(expr.initializer().is_not_nil())
  {
    exprt &initializer = static_cast<exprt &>(expr.add("initializer"));
    adjust_expr(initializer);
  }

  // Set sizeof and cmt_sizeof_type
  exprt size_of = c_sizeof(expr.type().subtype(), ns);
  size_of.set("#c_sizeof_type", expr.type().subtype());

  expr.set("sizeof", size_of);
}

void clang_cpp_adjust::adjust_member(member_exprt &expr)
{
  clang_c_adjust::adjust_member(expr);

  /*
   * Additional adjustment is required for C++ class member access:
   * e.g. when we got a class/struct member function call via:
   * dot operator, e.g. OBJECT.setX();
   * or arrow operator, e.g.OBJECT->setX();
   */
  if(expr.type().is_code() && !expr.get_string("component_name").empty())
  {
    adjust_cpp_member(expr);
  }
}

void clang_cpp_adjust::adjust_cpp_member(member_exprt &expr)
{
  /*
   * For class member function call:
   * Replace OBJECT.setX() OR OBJECT->setX() with setX(), where
   * OBJECT.setX() is represented by:
   *    member:
   *      * type: ...
   *      * operands:
   *      * symbol: <object_symbol>
   *      * component_name: <setX_clang_ID>
   *  OBJECT->setX() is represented by:
   *    member:
   *      * type: ...
   *      * operands:
   *        * dereference: <object_ptr>
   *      * component_name: <setX_clang_ID>
   *  and setX() is represented by the symbol expr:
   *    symbol:
   *      * type: ...
   *      * id: <setX_clang_ID>
   */
  const symbolt *comp_symb = namespacet(context).lookup(expr.component_name());
  assert(comp_symb);
  // compoment's type shall be the same as member_exprt's type
  // and both are of the type `code`
  assert(comp_symb->type.is_code());
  exprt method_call = symbol_expr(*comp_symb);
  expr.swap(method_call);
}

void clang_cpp_adjust::adjust_side_effect_assign(side_effect_exprt &expr)
{
  // sideeffect assign got be representing a binary operator
  assert(expr.operands().size() == 2);

  exprt &lhs = expr.op0();
  exprt &rhs = expr.op1();

  if(
    rhs.id() == "sideeffect" && rhs.statement() == "function_call" &&
    rhs.get_bool("constructor"))
  {
    // turn assign expression bleh = BLAH() into one instruction:
    // BLAH(&bleh);
    // where bleh has been declared and there exists a corresponding symbol
    side_effect_expr_function_callt &rhs_func_call =
      to_side_effect_expr_function_call(rhs);
    exprt &f_op = rhs_func_call.function();

    // callee must be a constructor
    assert(f_op.type().return_type().id() == "constructor");

    // just populate rhs' argument and replace the entire expression
    exprt &lhs = expr.op0();
    exprt arg = address_of_exprt(lhs);
    exprt base_symbol = arg.op0();
    assert(base_symbol.op0().id() == "symbol");
    // TODO: wrap base symbol into dereference if it's a member
    rhs_func_call.arguments().push_back(arg);

    expr.swap(rhs);

    // let's go through C's side_effect_function_call to make sure
    // there's no missing adjustment we need to carry out
    clang_c_adjust::adjust_side_effect_function_call(
      to_side_effect_expr_function_call(expr));
  }
  else if(lhs.is_symbol() && lhs.type().get_bool("#reference"))
  {
    // since we modelled lvalue reference as pointers
    // turn assign expression r = 1, where r is an lvalue reference
    // into *r = 1
    convert_lvalue_ref_to_deref(lhs);
  }
  else
    clang_c_adjust::adjust_side_effect(expr);
}

void clang_cpp_adjust::adjust_expr_rel(exprt &expr)
{
  clang_c_adjust::adjust_expr_rel(expr);

  exprt &op0 = expr.op0();
  if(op0.is_typecast())
  {
    // special treatment for lvalue reference
    // if lhs is a typecast of lvalue reference, e.g. (int)r == 1
    // where r is a reference
    // we turn it into (int)*r == 1
    exprt &tp_op0 = op0.op0();
    if(tp_op0.is_symbol() && tp_op0.type().get_bool("#reference"))
      convert_lvalue_ref_to_deref(tp_op0);
  }
}

void clang_cpp_adjust::convert_lvalue_ref_to_deref(exprt &expr)
{
  assert(expr.is_symbol() && expr.type().get_bool("#reference"));

  dereference_exprt tmp(expr, expr.type());
  tmp.location() = expr.location();
  expr.swap(tmp);
}