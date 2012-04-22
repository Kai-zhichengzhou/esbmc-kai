/*******************************************************************\

Module: Pointer Logic

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <assert.h>

#include <config.h>
#include <i2string.h>
#include <arith_tools.h>
#include <pointer_offset_size.h>
#include <std_expr.h>
#include <prefix.h>

#include "pointer_logic.h"

/*******************************************************************\

Function: pointer_logict::add_object

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

unsigned pointer_logict::add_object(const expr2tc &expr)
{
  // remove any index/member
  
  if (expr->expr_id == expr2t::index_id)
  {
    const index2t &index = static_cast<const index2t &>(*expr.get());
    return add_object(index.source_data);
  }
  else if (expr->expr_id == expr2t::member_id)
  {
    const member2t &memb = static_cast<const member2t &>(*expr.get());
    return add_object(memb.source_data);
  }
  std::pair<objectst::iterator, bool> ret = objects.insert(
                                     std::pair<expr2tc,unsigned int>(expr, 0));
  if (!ret.second)
    return ret.first->second;

  // Initialize object number.
  ret.first->second = objects.size() - 1;
  lookup.push_back(expr);
  assert(lookup.size() == objects.size());
  return objects.size() - 1;
}

/*******************************************************************\

Function: pointer_logict::pointer_expr

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

expr2tc pointer_logict::pointer_expr(
  unsigned object,
  const type2tc &type) const
{
  pointert pointer(object, 0);
  return pointer_expr(pointer, type);
}

/*******************************************************************\

Function: pointer_logict::pointer_expr

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

expr2tc pointer_logict::pointer_expr(
  const pointert &pointer,
  const type2tc &type) const
{
  type2tc pointer_type(new pointer_type2t(type2tc(new empty_type2t())));

  if(pointer.object==null_object) // NULL?
  {
    return expr2tc(new symbol2t(type, irep_idt("NULL")));
  }
  else if(pointer.object==invalid_object) // INVALID?
  {
    return expr2tc(new symbol2t(type, irep_idt("INVALID")));
  }
  
  if(pointer.object>=objects.size() || pointer.object<0)
  {
    return expr2tc(new symbol2t(type,
                                irep_idt("INVALID" + i2string(pointer.object))));
  }

  const expr2tc &object_expr = lookup[pointer.object];

  expr2tc deep_object = object_rec(pointer.offset, type, object_expr);
  
  exprt result;
  
  assert(type->type_id == type2t::pointer_id);
  return expr2tc(new address_of2t(type, deep_object));
}

/*******************************************************************\

Function: pointer_logict::object_rec

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

expr2tc pointer_logict::object_rec(
  const mp_integer &offset,
  const type2tc &pointer_type,
  const expr2tc &src) const
{
#warning jmorse - I am covered in bees.
//  assert(offset>=0);

  if(src->type->type_id == type2t::array_id)
  {
    const array_type2t &arrtype = static_cast<const array_type2t&>
                                             (*src->type.get());
    mp_integer size=pointer_offset_size(*arrtype.subtype.get());

    if (size == 0)
      return src;
    
    mp_integer index=offset/size;
    mp_integer rest=offset%size;

    type2tc inttype(new unsignedbv_type2t(config.ansi_c.int_width));
    expr2tc newindex(new index2t(arrtype.subtype, src,
                                 expr2tc(new constant_int2t(inttype, index))));
    
    return object_rec(rest, pointer_type, newindex);
  }
  else if(src->type->type_id == type2t::struct_id ||
          src->type->type_id == type2t::union_id)
  {
    const struct_union_type2t &type2 = static_cast<const struct_union_type2t&>
                                                  (*pointer_type.get());
    assert(offset>=0);
  
    if(offset==0) // the struct itself
      return src;

    mp_integer current_offset=1;

    assert(offset>=current_offset);

    unsigned int idx = 0;
    forall_types(it, type2.members) {
      assert(offset>=current_offset);

      mp_integer sub_size=pointer_offset_size(**it);
      
      if(sub_size==0)
        return src;
      
      mp_integer new_offset=current_offset+sub_size;

      if(new_offset>offset)
      {
        // found it
        expr2tc tmp(new member2t(*it, src,
                                 constant_string2t(type2tc(new string_type2t()),
                                                   type2.member_names[idx])));
        
        return object_rec(offset-current_offset, pointer_type, tmp);
      }
      
      assert(new_offset<=offset);
      current_offset=new_offset;
      assert(current_offset<=offset);
      idx++;
    }
  }
  
  return src;
}

/*******************************************************************\

Function: pointer_logict::pointer_logict

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

pointer_logict::pointer_logict()
{

  type2tc type(new pointer_type2t(type2tc(new empty_type2t())));
  expr2tc sym(new symbol2t(type, "NULL"));

  // add NULL
  null_object = add_object(sym);

  // add INVALID
  expr2tc invalid(new symbol2t(type, "INVALID"));
  invalid_object = add_object(invalid);
}

/*******************************************************************\

Function: pointer_logict::~pointer_logict

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

pointer_logict::~pointer_logict()
{
}
