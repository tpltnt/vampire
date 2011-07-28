/**
 * @file InterpretedNormalizer.cpp
 * Implements class InterpretedNormalizer.
 */

#include "Lib/Environment.hpp"
#include "Lib/ScopedPtr.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaTransformer.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Signature.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/TermTransformer.hpp"

#include "Property.hpp"

#include "InterpretedNormalizer.hpp"

namespace Shell
{

/**
 * Base term transforming class
 */
class InterpretedNormalizer::FunctionTranslator
{
public:
  virtual ~FunctionTranslator() {}

  virtual TermList translate(Term* trm) = 0;
};

/**
 * Class for transforming terms (t-u) into (t+(-u))
 */
class InterpretedNormalizer::SuccessorTranslator : public FunctionTranslator
{
public:
  SuccessorTranslator()
  {
    CALL("InterpretedNormalizer::BinaryMinusTranslator::BinaryMinusTranslator");

    _succFun = env.signature->getInterpretingSymbol(Theory::INT_SUCCESSOR);
    _plusFun = env.signature->getInterpretingSymbol(Theory::INT_PLUS);
    _one = TermList(theory->representConstant(IntegerConstantType(1)));
  }

  virtual TermList translate(Term* trm)
  {
    CALL("InterpretedNormalizer::BinaryMinusTranslator::translate");
    ASS_EQ(trm->functor(), _succFun);

    TermList arg = *trm->nthArgument(0);
    TermList res(Term::create2(_plusFun, arg, _one));
    return res;
  }

  /** Function that is being rewritten by this object */
  unsigned srcFunc() const { return _succFun; }
private:

  unsigned _succFun;
  unsigned _plusFun;
  TermList _one;
};

/**
 * Class for transforming terms (t-u) into (t+(-u))
 */
class InterpretedNormalizer::BinaryMinusTranslator : public FunctionTranslator
{
public:
  BinaryMinusTranslator(Interpretation bMinus, Interpretation plus, Interpretation uMinus)
  {
    CALL("InterpretedNormalizer::BinaryMinusTranslator::BinaryMinusTranslator");

    _bMinusFun = env.signature->getInterpretingSymbol(bMinus);
    _plusFun = env.signature->getInterpretingSymbol(plus);
    _uMinusFun = env.signature->getInterpretingSymbol(uMinus);
  }

  virtual TermList translate(Term* trm)
  {
    CALL("InterpretedNormalizer::BinaryMinusTranslator::translate");
    ASS_EQ(trm->functor(), _bMinusFun);

    TermList arg1 = *trm->nthArgument(0);
    TermList arg2 = *trm->nthArgument(1);
    TermList negArg2(Term::create1(_uMinusFun, arg2));
    TermList res(Term::create2(_plusFun, arg1, negArg2));
    return res;
  }

  /** Function that is being rewritten by this object */
  unsigned srcFunc() const { return _bMinusFun; }
private:

  unsigned _bMinusFun;
  unsigned _plusFun;
  unsigned _uMinusFun;
};

/**
 * Class whose instances are to be used for translating one type of inequality to enother
 */
class InterpretedNormalizer::IneqTranslator
{
public:
  IneqTranslator(Interpretation src, Interpretation tgt, bool swapArguments, bool reversePolarity)
   : _swapArguments(swapArguments), _reversePolarity(reversePolarity)
  {
    CALL("InterpretedNormalizer::IneqTranslator::IneqTranslator");
    _srcPred = env.signature->getInterpretingSymbol(src);
    _tgtPred = env.signature->getInterpretingSymbol(tgt);
    ASS_EQ(env.signature->predicateArity(_srcPred), 2);
    ASS_EQ(env.signature->predicateArity(_tgtPred), 2);

  }

  /** Predicate that is being rewritten by this object */
  unsigned srcPred() const { return _srcPred; }

  Literal* apply(Literal* lit)
  {
    CALL("InterpretedNormalizer::IneqTranslator::apply");
    ASS_EQ(lit->functor(), _srcPred);

    TermList args[2] = { *lit->nthArgument(0), *lit->nthArgument(1) };
    if(_swapArguments) { swap(args[0], args[1]); }
    bool polarity = lit->isPositive() ^ _reversePolarity;

    return Literal::create(_tgtPred, 2, polarity, false, args);
  }

private:
  unsigned _srcPred;
  unsigned _tgtPred;
  bool _swapArguments;
  bool _reversePolarity;
};

/**
 * Class that performs literal transformations
 */
class InterpretedNormalizer::NLiteralTransformer : private TermTransformer
{
public:
  NLiteralTransformer(Property* prop)
  : _ineqTransls(env.signature->predicates()),
    _fnTransfs(env.signature->functions()),
    _prop(prop)
  {
    CALL("InterpretedNormalizer::NLiteralTransformer::NLiteralTransformer");

    addIneqTransformer(Theory::INT_LESS, 	  Theory::INT_LESS_EQUAL, true, true);
    addIneqTransformer(Theory::INT_GREATER, 	  Theory::INT_LESS_EQUAL, false, true);
    addIneqTransformer(Theory::INT_GREATER_EQUAL, Theory::INT_LESS_EQUAL, true, false);

    addIneqTransformer(Theory::RAT_LESS, 	  Theory::RAT_LESS_EQUAL, true, true);
    addIneqTransformer(Theory::RAT_GREATER,	  Theory::RAT_LESS_EQUAL, false, true);
    addIneqTransformer(Theory::RAT_GREATER_EQUAL, Theory::RAT_LESS_EQUAL, true, false);

    addIneqTransformer(Theory::REAL_LESS,	  Theory::REAL_LESS_EQUAL, true, true);
    addIneqTransformer(Theory::REAL_GREATER,	  Theory::REAL_LESS_EQUAL, false, true);
    addIneqTransformer(Theory::REAL_GREATER_EQUAL,Theory::REAL_LESS_EQUAL, true, false);

    addMinusTransformer(Theory::INT_MINUS, Theory::INT_PLUS, Theory::INT_UNARY_MINUS);
    addMinusTransformer(Theory::RAT_MINUS, Theory::RAT_PLUS, Theory::RAT_UNARY_MINUS);
    addMinusTransformer(Theory::REAL_MINUS, Theory::REAL_PLUS, Theory::REAL_UNARY_MINUS);

    addSuccessorTransformer();
  }

  void apply(Literal* lit, bool& constantRes, Literal*& litRes, bool& boolRes)
  {
    CALL("InterpretedNormalizer::NLiteralTransformer::apply");

    if(theory->isInterpretedPredicate(lit)) {
      Interpretation itp = theory->interpretPredicate(lit);
      if(isTrivialInterpretation(itp)) {
	constantRes = true;
	boolRes = lit->isPositive();
	return;
      }
    }
    constantRes = false;
    litRes = transform(lit);

    unsigned pred = litRes->functor();
    IneqTranslator* transl = getIneqTranslator(pred);
    if(transl) {
      litRes = transl->apply(litRes);
    }
    if(_prop) {
      _prop->scanForInterpreted(litRes);
    }
  }
protected:
  using TermTransformer::transform;

  virtual TermList transform(TermList trm)
  {
    CALL("InterpretedNormalizer::NLiteralTransformer::transform");

  start:
    if(theory->isInterpretedFunction(trm)) {
      Interpretation itp = theory->interpretFunction(trm);
      if(isTrivialInterpretation(itp)) {
	Term* t = trm.term();
	ASS_EQ(t->arity(),1);
	return *t->nthArgument(0);
      }
    }
    if(trm.isTerm()) {
      Term* t = trm.term();
      unsigned func = t->functor();
      FunctionTranslator* transl = getFnTranslator(func);
      if(transl) {
	trm = transl->translate(t);
	goto start;
      }
    }
    return trm;
  }

private:

  /**
   * Ensure that binary minus @c bMinus will be replaced by combination of
   * plus operation @c plus and unary minus @c uMinus
   */
  void addMinusTransformer(Interpretation bMinus, Interpretation plus, Interpretation uMinus)
  {
    CALL("InterpretedNormalizer::NLiteralTransformer::addMinusTransformer");

    if(!env.signature->haveInterpretingSymbol(bMinus)) {
      return; //the symbol to be transformed doesn't exist, so we don't need to worry
    }
    BinaryMinusTranslator* transl = new BinaryMinusTranslator(bMinus, plus, uMinus);
    unsigned func = transl->srcFunc();
    ASS(!_fnTransfs[func])
    _fnTransfs[func] = transl;
  }

  /**
   * Ensure the INT_SUCCESSOR operation is rewritten to X+1
   */
  void addSuccessorTransformer()
  {
    CALL("InterpretedNormalizer::NLiteralTransformer::addMinusTransformer");

    if(!env.signature->haveInterpretingSymbol(Theory::INT_SUCCESSOR)) {
      return; //the symbol to be transformed doesn't exist, so we don't need to worry
    }
    SuccessorTranslator* transl = new SuccessorTranslator();
    unsigned func = transl->srcFunc();
    ASS(!_fnTransfs[func])
    _fnTransfs[func] = transl;
  }

  /**
   * Ensure that inequality @c from will be replaced by inequality @c to.
   * The arguments @c swapArguments and @c reversePolarity specify how the
   * replacement should be done.
   */
  void addIneqTransformer(Interpretation from, Interpretation to, bool swapArguments, bool reversePolarity)
  {
    CALL("InterpretedNormalizer::NLiteralTransformer::addIneqTransformer");

    if(!env.signature->haveInterpretingSymbol(from)) {
      return; //the symbol to be transformed doesn't exist, so we don't need to worry
    }
    IneqTranslator* transl = new IneqTranslator(from, to, swapArguments, reversePolarity);
    unsigned pred = transl->srcPred();
    ASS(!_ineqTransls[pred])
    _ineqTransls[pred] = transl;
  }

  /**
   * Return object that translates occurrences of function @c func, or zero
   * if there isn't any.
   */
  FunctionTranslator* getFnTranslator(unsigned func)
  {
    CALL("InterpretedNormalizer::NLiteralTransformer::addIneqTransformer");
    if(_fnTransfs.size()<=func) { return 0; }
    return _fnTransfs[func].ptr();
  }

  /**
   * Return object that translates occurrences of inequalities with predicate @c pred,
   * or zero if there isn't any.
   */
  IneqTranslator* getIneqTranslator(unsigned ineq)
  {
    CALL("InterpretedNormalizer::NLiteralTransformer::getIneqTranslator");
    if(_ineqTransls.size()<=ineq) { return 0; }
    return _ineqTransls[ineq].ptr();
  }

  /** inequality translators at positions of their predicate numbers */
  DArray<ScopedPtr<IneqTranslator> > _ineqTransls;
  /** inequality translators at positions of their predicate numbers */
  DArray<ScopedPtr<FunctionTranslator> > _fnTransfs;

  /** Problem property to be updated with newly added interpreted symbols
   * Can be zero, in that case nothing is updated*/
  Property* _prop;
};

/**
 * Class that uses @c NLiteralTransformer to perform transformations on formulas
 */
class InterpretedNormalizer::NFormulaTransformer : public FormulaTransformer
{
public:
  NFormulaTransformer(NLiteralTransformer* litTransf) : _litTransf(litTransf) {}

protected:
  /**
   * Transfor atomic formula
   *
   * The rest of transformations is done by the @c FormulaTransformer ancestor.
   */
  virtual Formula* applyLiteral(Formula* f)
  {
    CALL("applyLiteral");

    Literal* lit = f->literal();
    bool isConst;
    Literal* newLit;
    bool newConst;
    _litTransf->apply(lit, isConst, newLit, newConst);
    if(isConst) {
      return new Formula(newConst);
    }
    if(newLit==lit) { return f; }
    return new AtomicFormula(newLit);
  }

private:
  NLiteralTransformer* _litTransf;
};

//////////////////////////
// InterpretedNormalizer
//

InterpretedNormalizer::InterpretedNormalizer(Property* prop)
{
  CALL("InterpretedNormalizer::InterpretedNormalizer");

  _litTransf = new NLiteralTransformer(prop);
}

InterpretedNormalizer::~InterpretedNormalizer()
{
  CALL("InterpretedNormalizer::~InterpretedNormalizer");

  delete _litTransf;
}


void InterpretedNormalizer::apply(UnitList*& units)
{
  CALL("InterpretedNormalizer::apply");

  NFormulaTransformer ftransf(_litTransf);
  FTFormulaUnitTransformer<NFormulaTransformer> futransf(Inference::EVALUATION, ftransf);

  UnitList::DelIterator uit(units);
  while(uit.hasNext()) {
    Unit* u = uit.next();
    if(u->isClause()) {
      Clause* cl = static_cast<Clause*>(u);
      Clause* cl1 = apply(cl);
      if(cl!=cl1) {
	if(cl1) {
	  uit.replace(cl1);
	}
	else {
	  uit.del();
	}
      }
    }
    else {
      FormulaUnit* fu = static_cast<FormulaUnit*>(u);
      FormulaUnit* fu1 = futransf.transform(fu);
      if(fu!=fu1) {
	uit.replace(fu1);
      }
    }
  }
}

Clause* InterpretedNormalizer::apply(Clause* cl)
{
  CALL("InterpretedNormalizer::isTrivialInterpretation");

  static LiteralStack lits;
  lits.reset();
  unsigned clen = cl->length();
  bool modified = false;

  for(unsigned i=0; i<clen; i++) {
    Literal* lit = (*cl)[i];

    bool isConst;
    Literal* newLit;
    bool newConst;
    _litTransf->apply(lit, isConst, newLit, newConst);

    if(isConst) {
      modified = true;
      if(newConst) {
	return 0;
      }
      continue;
    }
    if(newLit!=lit) {
      modified = true;
    }
    lits.push(newLit);
  }
  if(!modified) {
    return cl;
  }

  Clause* res = Clause::fromStack(lits, cl->inputType(),
      new Inference1(Inference::EVALUATION, cl));
  return res;
}

/**
 * Return true if interpretatioin @c itp is trivial and shold just be
 * removed as an identity (in case of functions), or replaced by $true
 * (in case of predicates)
 */
bool InterpretedNormalizer::isTrivialInterpretation(Interpretation itp)
{
  CALL("InterpretedNormalizer::isTrivialInterpretation");

  switch(itp) {
  case Theory::INT_IS_INT:
  case Theory::INT_IS_RAT:
  case Theory::INT_IS_REAL:
  case Theory::RAT_IS_RAT:
  case Theory::RAT_IS_REAL:
  case Theory::REAL_IS_REAL:

  case Theory::INT_TO_INT:
  case Theory::RAT_TO_RAT:
  case Theory::REAL_TO_REAL:
    return true;

  default:
    return false;
  }
}

}
