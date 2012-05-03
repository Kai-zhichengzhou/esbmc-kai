#include "irep2.h"

expr2t *
expr2t::do_simplify(bool shallow) const
{

  if (shallow)
    return NULL;

  // Base simplify - the obj being simplified has no function that simplifies
  // itself. So, fetch a vector of operands and apply simplification to anything
  // further down. If something gets changed, then we need to clone this expr
  // and return a new one, as we've been modified.
  bool changed = false;
  std::vector<const expr2tc*> operands;
  std::vector<expr2t*> newoperands;

  list_operands(operands);
  for (std::vector<const expr2tc *>::iterator it = operands.begin();
       it != operands.end(); it++) {
    expr2t *tmp = (**it).get()->do_simplify(false);
    newoperands.push_back(tmp);
    if (tmp != NULL)
      changed = true;
  }

  if (changed == false)
    return NULL;

  // An operand has been changed; clone ourselves and update.
  expr2t *new_us = clone_raw();
  std::vector<expr2tc*> clonedoperands;
  new_us->list_operands(clonedoperands);
  assert(clonedoperands.size() == newoperands.size());

  std::vector<expr2t *>::iterator it2 = newoperands.begin();
  for (std::vector<expr2tc *>::iterator it = clonedoperands.begin();
       it != clonedoperands.end(); it++, it2++) {
    if ((*it2) == NULL)
      continue; // No change in operand;
    else
      **it = expr2tc(*it2); // Operand changed; overwrite with new one.
  }

  return new_us;
}

expr2t *
add2t::do_simplify(bool shallow)
{
  if (is_constant_expr(side_1) && is_constant_expr(side_2)) {
    // Try simplification
    // unimplemented.
    return NULL;
  } else if (shallow) {
    return NULL;
  } else {
    // Attempt to simplify operands
    expr2t *cloned = expr2t::do_simplify(false);

    if (cloned == NULL)
      return NULL; // Sub-operands were not simplified further.

    return cloned->do_simplify(true); // Attempt to simplify simplified copy.
  }
}
