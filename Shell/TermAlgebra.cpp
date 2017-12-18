
/*
 * File TermAlgebra.cpp.
 *
 * This file is part of the source code of the software program
 * Vampire. It is protected by applicable
 * copyright laws.
 *
 * This source code is distributed under the licence found here
 * https://vprover.github.io/license.html
 * and in the source directory
 *
 * In summary, you are allowed to use Vampire for non-commercial
 * purposes but not allowed to distribute, modify, copy, create derivatives,
 * or use in competitions. 
 * For other uses of Vampire please contact developers for a different
 * licence, which we will make an effort to provide. 
 */
#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Inference.hpp"

using namespace Kernel;
using namespace Lib;

namespace Shell {

vstring TermAlgebraConstructor::name()
{
  return Lib::env.signature->functionName(_functor);
}

TermAlgebraConstructor::TermAlgebraConstructor(unsigned functor, Lib::Array<unsigned> destructors)
  : _functor(functor),
    _hasDiscriminator(false),
    _destructors(destructors)
{
  _type = env.signature->getFunction(_functor)->fnType();
  ASS_REP(env.signature->getFunction(_functor)->termAlgebraCons(),
          env.signature->functionName(_functor));
  ASS_EQ(_type->arity(), destructors.size());
}

TermAlgebraConstructor::TermAlgebraConstructor(unsigned functor, unsigned discriminator, Lib::Array<unsigned> destructors)
  : _functor(functor),
    _hasDiscriminator(true),
    _discriminator(discriminator),
    _destructors(destructors)
{
  _type = env.signature->getFunction(_functor)->fnType();
  ASS_REP(env.signature->getFunction(_functor)->termAlgebraCons(),
          env.signature->functionName(_functor));
  ASS_EQ(_type->arity(), destructors.size());
}

bool TermAlgebraConstructor::recursive()
{
  CALL("TermAlgebraConstructor::recursive");

  for (unsigned i=0; i < arity(); i++) {
    if (_type->arg(i) == _type->result()) {
      // this constructor has a recursive argument
      return true;
    }
  }
  return false;
}

Lib::vstring TermAlgebraConstructor::discriminatorName()
{
  CALL("TermAlgebraConstructor::discriminatorName");

  return "$is_" + name();
}

Lib::vstring TermAlgebraConstructor::getCtxFunctionName() {
  return "$ctx_" + name();
}

unsigned TermAlgebraConstructor::getCtxFunction() {
  CALL("TermAlgebra::getCtxFunction");

  bool added;
  unsigned s = env.signature->addFunction(getCtxFunctionName(), arity(), added);

  //TODO
  /*if (added) {
    env.signature->getFunction(s)->setType(OperatorType::getFunctionType({contextSort(), _sort}, _sort));
    }*/

  return s;
}

TermAlgebra::TermAlgebra(unsigned sort,
                         unsigned n,
                         TermAlgebraConstructor** constrs,
                         bool allowsCyclicTerms) :
  _sort(sort),
  _n(n),
  _allowsCyclicTerms(allowsCyclicTerms),
  _constrs(n)
{
  for (unsigned i = 0; i < n; i++) {
    ASS(constrs[i]->rangeSort() == _sort);
    _constrs[i] = constrs[i];
  }
}

bool TermAlgebra::emptyDomain()
{
  CALL("TermAlgebra::emptyDomain");

  if (_n == 0) {
    return true;
  }

  if (_allowsCyclicTerms) {
    return false;
  }

  for (unsigned i = 0; i < _n; i++) {
    if (!(_constrs[i]->recursive())) {
      return false;
    }
  }
  return true;
}

bool TermAlgebra::finiteDomain()
{
  CALL("TermAlgebra::finiteDomain");

  for (unsigned i = 0; i < _n; i++) {
    if (_constrs[i]->arity() > 0) {
      return false;
    }
  }

  return true;
}

bool TermAlgebra::infiniteDomain()
{
  CALL("TermAlgebra::infiniteDomain");

  for (unsigned i = 0; i < _n; i++) {
    if (_constrs[i]->recursive()) {
      return true;
    }
  }

  return false;
}

bool TermAlgebra::subtermReachable(TermAlgebra *ta)
{
  CALL("TermAlgebra::subtermReachable");

  for (unsigned i = 0; i < _n; i++) {
    TermAlgebraConstructor* c = _constrs[i];
    for (unsigned j = 0; j < c->arity(); j++) {
      unsigned s = c->argSort(j);
      if (s == ta->sort() ||
          (env.signature->isTermAlgebraSort(s)
           && env.signature->getTermAlgebraOfSort(s)->subtermReachable(ta))) {
        return true;
      }
    }
  }
  return false;
}
  
Lib::vstring TermAlgebra::getSubtermPredicateName() {
  return "$subterm_" + env.sorts->sortName(_sort);
}

unsigned TermAlgebra::getSubtermPredicate() {
  CALL("TermAlgebra::getSubtermPredicate");

  bool added;
  unsigned s = env.signature->addPredicate(getSubtermPredicateName(), 2, added);

  if (added) {
    // declare a binary predicate subterm
    env.signature->getPredicate(s)->setType(OperatorType::getPredicateType({_sort, _sort}));
  }

  return s;
}

unsigned TermAlgebra::contextSort(TermAlgebra* ta) {
  if (_contextSorts.find(ta)) {
    return _contextSorts.get(ta);
  } else {
    unsigned s = env.sorts->addSort("ctx_" + name() + "_" + ta->name(), false);
    _contextSorts.insert(ta, s);
    return s;
  }
}

Lib::vstring TermAlgebra::getCstFunctionName() {
  return "$cst_" + name();
}

unsigned TermAlgebra::getCstFunction() {
  CALL("TermAlgebra::getCstFunction");

  bool added;
  unsigned s = env.signature->addFunction(getSubstFunctionName(), 1, added);

  if (added) {
    env.signature->getFunction(s)->setType(OperatorType::getFunctionType({_sort}, contextSort(this)));
  }

  return s;
}

Lib::vstring TermAlgebra::getCycleFunctionName() {
  return "$cycle_" + name();
}

unsigned TermAlgebra::getCycleFunction() {
  CALL("TermAlgebra::getCycleFunction");

  bool added;
  unsigned s = env.signature->addFunction(getCycleFunctionName(), 1, added);

  if (added) {
    env.signature->getFunction(s)->setType(OperatorType::getFunctionType({contextSort(this)}, _sort));
  }

  return s;
}

Lib::vstring TermAlgebra::getAppFunctionName(TermAlgebra* ta) {
  return "$app_" + name() + "_" + ta->name();
}

unsigned TermAlgebra::getAppFunction(TermAlgebra* ta) {
  CALL("TermAlgebra::getAppFunction");

  bool added;
  unsigned s = env.signature->addFunction(getCycleFunctionName(), 2, added);

  if (added) {
    env.signature->getFunction(s)->setType(OperatorType::getFunctionType({contextSort(ta), ta->sort()}, _sort));
  }

  return s;
}

}
