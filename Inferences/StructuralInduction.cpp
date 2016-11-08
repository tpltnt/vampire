/**
 * @file StructuralInduction.cpp
 */

#include "Lib/Environment.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/EqHelper.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/Signature.hpp"
#include "Kernel/TermIterators.hpp"

#include "Shell/Skolem.hpp"

#include "StructuralInduction.hpp"

namespace Inferences {

  struct StructuralInduction::GenerateTermAlgebraLiteralsFn {
    DECL_RETURN_TYPE(VirtualIterator<List<Literal*>*>);
    OWN_RETURN_TYPE operator()(pair<Literal*, TermList> generalisation) {
      LiteralGeneralisationFn generalise(generalisation.first, generalisation.second);

      FunctionType* function = env.signature->getFunction(generalisation.second.term()->functor())->fnType();
      TermAlgebra* termAlgebra = env.signature->getTermAlgebraOfSort(function->result());

      List<List<Literal*>*>* constructorsLiterals(0);
      for (unsigned c = 0; c < termAlgebra->nConstructors(); c++) {
        List<Literal*>* constructorLiterals = generateConstructorLiterals(termAlgebra->constructor(c), generalise);
        List<List<Literal*>*>::push(constructorLiterals, constructorsLiterals);
      }

      return pvi(List<List<Literal*>*>::Iterator(generateCartesianProduct(constructorsLiterals)));
    }

  private:
    struct LiteralGeneralisationFn {
      LiteralGeneralisationFn(Literal* literal, TermList subterm) : _literal(literal), _subterm(subterm) {}

      DECL_RETURN_TYPE(Literal*);
      OWN_RETURN_TYPE operator()(TermList subterm) {
        return EqHelper::replace(_literal, _subterm, subterm);
      }

    private:
      Literal* _literal;
      TermList _subterm;
    };

    List<Literal*>* generateConstructorLiterals(TermAlgebraConstructor* constructor, LiteralGeneralisationFn generalisation) {
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
          List<Literal*>::push(generalisation(skolemTerm), constructorLiterals);
        }
      }

      TermList termAlgebraTerm = TermList(Term::create(constructor->functor(), arity, skolems.begin()));
      Literal* termAlgebraLiteral = Literal::complementaryLiteral(generalisation(termAlgebraTerm));
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
            Literal* summand = si.next();
            List<List<Literal*>*>::push(factor->cons(summand), extendedProduct);
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
      Stack<Literal*> literals(_premise->length() - 1 + generatedLiterals->length());
      for (unsigned i = 0; i < _premise->length(); i++) {
        if ((*_premise)[i] != _selectedLiteral) {
          literals.push((*_premise)[i]);
        } else {
          literals.loadFromIterator(List<Literal*>::Iterator(generatedLiterals));
        }
      }
      static Inference* inference = new Inference1(Inference::STRUCTURAL_INDUCTION, _premise);
      Clause* conclusion = Clause::fromStack(literals, _premise->inputType(), inference);
      conclusion->setAge(_premise->age() + 1);
      return conclusion;
    }

  private:
    Clause* _premise;
    Literal* _selectedLiteral;
  };

  struct StructuralInduction::InductiveGeneralisationIterator {
    InductiveGeneralisationIterator(Literal* selectedLiteral) {
      TermIterator nvi = TermIterator(new NonVariableIterator(selectedLiteral));
      auto uniqueSubterms = getUniquePersistentIterator(nvi);
      _tasi = pvi(getFilteredIterator(uniqueSubterms, IsTermAlgebraSubtermFn()));
    }

    bool hasNext() { return _tasi.hasNext(); }

    DECL_ELEMENT_TYPE(TermList);
    OWN_ELEMENT_TYPE next() { return _tasi.next(); }

  private:
    TermIterator _tasi;

    struct IsTermAlgebraSubtermFn {
      DECL_RETURN_TYPE(bool);
      OWN_RETURN_TYPE operator()(TermList subterm) {
        unsigned functor = subterm.term()->functor();

        FunctionType* function = env.signature->getFunction(functor)->fnType();

        if (function->arity() > 0) {
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

        return true;
      }
    };
  };

  struct StructuralInduction::InductiveSubtermFn {
    InductiveSubtermFn(Clause* premise): _premise(premise) {}

    DECL_RETURN_TYPE(ClauseIterator);
    OWN_RETURN_TYPE operator()(Literal* selectedLiteral) {
      Literal* negatedSelectedLiteral = Literal::complementaryLiteral(selectedLiteral);
      auto inductiveGeneralisations = pushPairIntoRightIterator(negatedSelectedLiteral, InductiveGeneralisationIterator(selectedLiteral));

      auto termAlgebraLiterals = getMapAndFlattenIterator(inductiveGeneralisations, GenerateTermAlgebraLiteralsFn());

      auto conclusions = getMappingIterator(termAlgebraLiterals, ExtendPremiseFn(_premise, selectedLiteral));

      return pvi(conclusions);
    }

  private:
    Clause* _premise;
  };

  struct StructuralInduction::IsGroundLiteral {
    DECL_RETURN_TYPE(bool);
    OWN_RETURN_TYPE operator()(Literal* literal) { return literal->ground(); }
  };

  ClauseIterator StructuralInduction::generateClauses(Clause* premise) {
    CALL("StructuralInduction::generateClauses");
    auto selectedLiterals = premise->getSelectedLiteralIterator();
    auto groundSelectedLiterals = getFilteredIterator(selectedLiterals, IsGroundLiteral());
    return pvi(getMapAndFlattenIterator(groundSelectedLiterals, InductiveSubtermFn(premise)));
  }

}