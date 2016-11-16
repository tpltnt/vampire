/**
 * @file StructuralInduction.cpp
 */

#include "Lib/Environment.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/EqHelper.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/Signature.hpp"
#include "Kernel/TermIterators.hpp"

#include "Shell/Options.hpp"
#include "Shell/Skolem.hpp"

#include "StructuralInduction.hpp"

namespace Inferences {

  struct StructuralInduction::GenerateTermAlgebraLiteralsFn {
    GenerateTermAlgebraLiteralsFn(Literal* literal) : _literal(literal) {}

    DECL_RETURN_TYPE(VirtualIterator<List<Literal*>*>);
    OWN_RETURN_TYPE operator()(TermList subterm) {
      CALL("StructuralInduction::GenerateTermAlgebraLiteralsFn::operator()");

      FunctionType* function = env.signature->getFunction(subterm.term()->functor())->fnType();
      TermAlgebra* termAlgebra = env.signature->getTermAlgebraOfSort(function->result());

      List<List<Literal*>*>* constructorsLiterals(0);
      for (unsigned c = 0; c < termAlgebra->nConstructors(); c++) {
        List<Literal*>* constructorLiterals = generateConstructorLiterals(termAlgebra->constructor(c), subterm);
        List<List<Literal*>*>::push(constructorLiterals, constructorsLiterals);
      }

      return pvi(List<List<Literal*>*>::Iterator(generateCartesianProduct(constructorsLiterals)));
    }

  private:
    Literal* _literal;

    List<Literal*>* generateConstructorLiterals(TermAlgebraConstructor* constructor, TermList subterm) {
      CALL("StructuralInduction::GenerateTermAlgebraLiteralsFn::generateConstructorLiterals");
      unsigned arity = constructor->arity();

      List<Literal*>* constructorLiterals = List<Literal*>::empty();

      Stack<TermList> skolems(arity);
      for (unsigned argument = 0; argument < arity; argument++) {
        vstring skolemSuffix = constructor->destructorName(argument);
        unsigned skolemFunction = Skolem::addSkolemFunction(0, 0, constructor->argSort(argument), skolemSuffix.data());
        TermList skolemTerm(Term::createConstant(skolemFunction));
        skolems.push(skolemTerm);
        if (constructor->argSort(argument) == constructor->rangeSort()) {
          List<Literal*>::push(EqHelper::replace(_literal, subterm, skolemTerm), constructorLiterals);
        }
      }

      TermList termAlgebraTerm = TermList(Term::create(constructor->functor(), arity, skolems.begin()));
      Literal* termAlgebraLiteral = Literal::complementaryLiteral(EqHelper::replace(_literal, subterm, termAlgebraTerm));
      List<Literal*>::push(termAlgebraLiteral, constructorLiterals);

      return constructorLiterals;
    }

    List<List<Literal*>*>* generateCartesianProduct(List<List<Literal*>*>* sources) {
      CALL("StructuralInduction::generateCartesianProduct");

      List<List<Literal*>*>* product = new List<List<Literal*>*>(List<Literal*>::empty());

      List<List<Literal*>*>::Iterator ssi(sources);
      while (ssi.hasNext()) {
        List<Literal*>* source = ssi.next();
        List<List<Literal*>*>* extendedProduct(0);
        List<List<Literal*>*>::Iterator pi(product);
        while (pi.hasNext()) {
          List<Literal*>* factor = pi.next();
          List<Literal*>::Iterator si(source);
          while (si.hasNext()) {
            List<List<Literal*>*>::push(factor->cons(si.next()), extendedProduct);
          }
        }
        product = extendedProduct;
      }

      return product;
    }
  };

  struct StructuralInduction::ExtendPremiseFn {
    ExtendPremiseFn(Clause* premise, Literal* selectedLiteral)
      : _premise(premise), _selectedLiteral(selectedLiteral) {}

    DECL_RETURN_TYPE(Clause*);
    OWN_RETURN_TYPE operator()(List<Literal*>* generatedLiterals) {
      CALL("StructuralInduction::ExtendPremiseFn::operator()");
      Stack<Literal*> literals(_premise->length() - 1 + generatedLiterals->length());
      for (unsigned i = 0; i < _premise->length(); i++) {
        if ((*_premise)[i] != _selectedLiteral) {
          literals.push((*_premise)[i]);
        } else {
          literals.loadFromIterator(List<Literal*>::Iterator(generatedLiterals));
        }
      }
      Inference* inference = new Inference1(Inference::STRUCTURAL_INDUCTION, _premise);
      Clause* conclusion = Clause::fromStack(literals, _premise->inputType(), inference);
      conclusion->setAge(_premise->age() + 1);
      return conclusion;
    }

  private:
    Clause* _premise;
    Literal* _selectedLiteral;
  };

  struct StructuralInduction::InductiveGeneralisationIterator {
    InductiveGeneralisationIterator(Literal* literal) {
      TermIterator nvi = TermIterator(new NonVariableIterator(literal));
      _tasi = pvi(getFilteredIterator(nvi, IsTermAlgebraSubtermFn(literal)));
    }

    bool hasNext() { return _tasi.hasNext(); }

    DECL_ELEMENT_TYPE(TermList);
    OWN_ELEMENT_TYPE next() { return _tasi.next(); }

  private:
    TermIterator _tasi;

    struct IsTermAlgebraSubtermFn {
      IsTermAlgebraSubtermFn(Literal* literal): _literal(literal) {}

      DECL_RETURN_TYPE(bool);
      OWN_RETURN_TYPE operator()(TermList subterm) {
        CALL("StructuralInduction::IsTermAlgebraSubtermFn::operator()");

        unsigned functor = subterm.term()->functor();

        FunctionType* function = env.signature->getFunction(functor)->fnType();

        if (env.options->structuralInductionSubtermArity() >= 0 &&
            function->arity() > env.options->structuralInductionSubtermArity()) {
          return false;
        }

        unsigned resultSort = function->result();

        if (!env.signature->isTermAlgebraSort(resultSort)) {
          return false;
        }

        TermAlgebra* ta = env.signature->getTermAlgebraOfSort(resultSort);

        bool recursive = false;
        for (unsigned c = 0; c < ta->nConstructors(); c++) {
          if (ta->constructor(c)->recursive()) {
            recursive = true;
            break;
          }
        }

        if (!recursive) {
          return false;
        }

        if (isTermAlgebraConstant(subterm, ta)) {
          return false;
        }

        /** Note: _generalisedLiterals contains literals with exactly one variable that
         *  represents the generalised subterm of a term algebra datatype.
         *
         *  The current implementation relies on the fact that we only consider ground
         *  literals, so we could simply replace the subterm with a variable.
         *
         *  For the extension to the non-ground case that should be improved
         */
        ASS(_literal->ground());
        Literal* generalisedLiteral = EqHelper::replace(_literal, subterm, TermList(0, false));
        if (_generalisedLiterals.contains(generalisedLiteral)) {
          return false;
        } else {
          _generalisedLiterals.insert(generalisedLiteral);
        }

//        cout << "[SI] Instantiating structural induction schema for the negation of " << generalisedLiteral->toString()
//             << " which is a generalisation over " << subterm.toString() << endl;

        return true;
      }

    private:
      Literal* _literal;

      static bool isTermAlgebraConstant(TermList ts, TermAlgebra* ta) {
        Set<unsigned> constructors;
        for (unsigned c = 0; c < ta->nConstructors(); c++) {
          constructors.insert(ta->constructor(c)->functor());
        }

        static Stack<TermList> terms(8);
        ASS(terms.isEmpty());
        terms.push(ts);

        while (terms.isNonEmpty()) {
          TermList t = terms.pop();

          if (!t.isTerm()) {
            continue;
          }

          Term* term = t.term();

          if (!constructors.contains(term->functor())) {
            terms.reset();
            return false;
          }

          terms.loadFromIterator(Term::Iterator(term));
        }

        return true;
      }
    };
  };

  Set<Literal*> StructuralInduction::_generalisedLiterals;

  struct StructuralInduction::InductiveSubtermFn {
    InductiveSubtermFn(Clause* premise): _premise(premise) {}

    DECL_RETURN_TYPE(ClauseIterator);
    OWN_RETURN_TYPE operator()(Literal* selectedLiteral) {
      CALL("StructuralInduction::InductiveSubtermFn::operator()");
      auto termAlgebraSubterms = InductiveGeneralisationIterator(selectedLiteral);
      GenerateTermAlgebraLiteralsFn generator(Literal::complementaryLiteral(selectedLiteral));
      auto termAlgebraLiterals = getMapAndFlattenIterator(termAlgebraSubterms, generator);
      auto conclusions = getMappingIterator(termAlgebraLiterals, ExtendPremiseFn(_premise, selectedLiteral));
      return pvi(conclusions);
    }

  private:
    Clause* _premise;
  };

  struct StructuralInduction::IsEligibleLiteral {
    DECL_RETURN_TYPE(bool);
    OWN_RETURN_TYPE operator()(Literal* literal) {
      if (!literal->ground()) {
        return false;
      }

      if (literal->polarity()) {
        if (!env.options->structuralInductionPositiveLiterals()) {
          return false;
        }
      } else {
        if (!env.options->structuralInductionNegativeLiterals()) {
          return false;
        }
      }

      return true;
    }
  };

  ClauseIterator StructuralInduction::generateClauses(Clause* premise) {
    CALL("StructuralInduction::generateClauses");
    auto selectedLiterals = premise->getSelectedLiteralIterator();
    auto groundSelectedLiterals = getFilteredIterator(selectedLiterals, IsEligibleLiteral());
    return pvi(getMapAndFlattenIterator(groundSelectedLiterals, InductiveSubtermFn(premise)));
  }

}