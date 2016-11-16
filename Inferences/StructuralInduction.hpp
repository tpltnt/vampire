/**
 * @file StructuralInduction.hpp
 * Defines class StructuralInduction.
 */

#ifndef __STRUCTURAL_INDUCTION__
#define __STRUCTURAL_INDUCTION__

#include "Lib/Set.hpp"

#include "Forwards.hpp"
#include "InferenceEngine.hpp"

namespace Inferences {

  class StructuralInduction : public GeneratingInferenceEngine {
    public:
      StructuralInduction() {}

      CLASS_NAME(StructuralInduction);
      USE_ALLOCATOR(StructuralInduction);
      ClauseIterator generateClauses(Clause* premise);

    private:
      static Set<Literal*> _generalisedLiterals;

      struct IsEligibleLiteral;
      struct InductiveSubtermFn;
      struct IsTermAlgebraSubtermFn;
      struct VirtualNonVariableIterator;
      struct InductiveGeneralisationIterator;
      struct GenerateTermAlgebraLiteralsFn;
      struct ExtendPremiseFn;
  };

}

#endif
