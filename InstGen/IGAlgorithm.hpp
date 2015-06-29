/**
 * @file IGAlgorithm.hpp
 * Defines class IGAlgorithm.
 */

#ifndef __IGAlgorithm__
#define __IGAlgorithm__

#include "Forwards.hpp"

#include "Lib/DHMap.hpp"
#include "Lib/DHSet.hpp"
#include "Lib/RatioKeeper.hpp"
#include "Lib/ScopedPtr.hpp"
#include "Lib/SmartPtr.hpp"
#include "Lib/Stack.hpp"

#include "Kernel/LiteralSelector.hpp"
#include "Kernel/MainLoop.hpp"
#include "Kernel/Ordering.hpp"
#include "Kernel/RCClauseStack.hpp"
#include "Kernel/TermIterators.hpp"

#include "Indexing/ClauseVariantIndex.hpp"
#include "Indexing/IndexManager.hpp"
#include "Indexing/LiteralIndex.hpp"
#include "Indexing/LiteralSubstitutionTreeWithoutTop.hpp"

#include "Inferences/GlobalSubsumption.hpp"
#include "Inferences/InferenceEngine.hpp"
#include "Inferences/TautologyDeletionISE.hpp"
#include "Inferences/URResolution.hpp"
#include "Inferences/DistinctEqualitySimplifier.hpp"

#include "SAT/SATSolver.hpp"

#include "Saturation/AWPassiveClauseContainer.hpp"

#include "Shell/Options.hpp"
#include "Shell/Statistics.hpp"
#include "Shell/EqualityProxy.hpp"

#include "Kernel/Grounder.hpp"

namespace InstGen {

using namespace Kernel;
using namespace Inferences;
using namespace Indexing;
using namespace SAT;
using namespace Saturation;
using namespace Shell;

class IGAlgorithm : public MainLoop {
public:
  CLASS_NAME(IGAlgorithm);
  USE_ALLOCATOR(IGAlgorithm);  
  
  typedef Statistics::TerminationReason TerminationReason;

  IGAlgorithm(Problem& prb, const Options& opt);
  ~IGAlgorithm();

  GroundingIndex& getGroundingIndex() { return *_groundingIndex.ptr(); }

  ClauseIterator getActive();

protected:
  virtual void init();
  virtual MainLoopResult runImpl();
private:

  bool addClause(Clause* cl);

  void doInprocessing(RCClauseStack& clauses);

  void restartWithCurrentClauses();
  void restartFromBeginning();


  void wipeIndexes();

  void processUnprocessed();
  void activate(Clause* cl, bool wasDeactivated=false);

  void deactivate(Clause* cl);
  void doImmediateReactivation();
  void doPassiveReactivation();

  void selectAndAddToIndex(Clause* cl);
  void removeFromIndex(Clause* cl);

  void tryGeneratingInstances(Clause* cl, unsigned litIdx);

  bool startGeneratingClause(Clause* orig, ResultSubstitution& subst, bool isQuery, Clause* otherCl,Literal* origLit, LiteralStack& genLits, bool& properInstance);
  void finishGeneratingClause(Clause* orig, ResultSubstitution& subst, bool isQuery, Clause* otherCl,Literal* origLit, LiteralStack& genLits);

  bool isSelected(Literal* lit);

  Clause* getFORefutation(SATClause* satRefutation);


  void onResolutionClauseDerived(Clause* cl);
  void doResolutionStep();


  MainLoopResult onModelFound();

  /**
   * True if we're running freshly restarted instantiation
   * to see if new clauses are generated, or we have a
   * satisfiable problem.
   */
  bool _doingSatisfiabilityCheck;

  RatioKeeper _instGenResolutionRatio;


  SATSolver* _satSolver;
  ScopedPtr<IGGrounder> _gnd;

  /** Used by global subsumption */
  ScopedPtr<GroundingIndex> _groundingIndex;
  ScopedPtr<GlobalSubsumption> _globalSubsumption;

  Options _saturationOptions;
  ScopedPtr<IndexManager> _saturationIndexManager;
  ScopedPtr<Problem> _saturationProblem;
  ScopedPtr<SaturationAlgorithm> _saturationAlgorithm;

  OrderingSP _ordering;
  ScopedPtr<LiteralSelector> _selector;


//  ScopedPtr<UnitClauseLiteralIndex> _unitLitIndex;
//  ScopedPtr<NonUnitClauseLiteralIndex> _nonUnitLitIndex;
//  ScopedPtr<URResolution> _urResolution;
//  PlainClauseContainer _resolutionContainer;

  /** Clauses that weren't yet added into the SATSolver */
  RCClauseStack _unprocessed;
  /** Clauses that are inside the SATSolver but not used for instantiation */
  AWClauseContainer _passive;
  /** Clauses inside the SATSolver and used for instantiation */
  RCClauseStack _active;

  /** Clauses that need to be activated again because of the change in selection */
  ClauseStack _deactivated;
  DHSet<Clause*> _deactivatedSet;

  RCClauseStack _inputClauses;

  ClauseVariantIndex* _variantIdx;

  LiteralSubstitutionTree* _selected;

  DuplicateLiteralRemovalISE _duplicateLiteralRemoval;
  TrivialInequalitiesRemovalISE _trivialInequalityRemoval;
  TautologyDeletionISE _tautologyDeletion;
  DistinctEqualitySimplifier _distinctEqualitySimplifier;

  bool _use_niceness;
  bool _use_dm;
  bool _shallow_dm;

  /**
   * Dismatching constraints should provide these two functions
   */
  struct DismatchingConstraints {
    virtual void add(Literal* org, Literal* inst,RobSubstitution* subst) = 0;
    virtual bool shouldBlock(Literal* org, Literal* inst, RobSubstitution* subst) = 0;
  };

  /**
   * A struct for holding clause's dms, on per literal basis.
   * This version is general i.e allows arbitary substitutions
   */
  struct DismatchingConstraintsGeneral : public DismatchingConstraints {
    typedef DHMap<Literal*,DismatchingLiteralIndex*> Lit2Index;

    Lit2Index lit2index;

    void add(Literal* orig, Literal* inst, RobSubstitution* subst) {
      CALL("DismatchingConstraintsGeneral::add");
      DismatchingLiteralIndex* index;
      if (!lit2index.find(orig,index)) {
        LiteralIndexingStructure * is = new LiteralSubstitutionTreeWithoutTop();
        index = new DismatchingLiteralIndex(is); // takes care of deleting is
        ALWAYS(lit2index.insert(orig,index));
      }
      index->addLiteral(inst);
    }

    bool shouldBlock(Literal* orig, Literal* inst, RobSubstitution* subst) {
      CALL("DismatchingConstraintsGeneral::shouldBlock");
      DismatchingLiteralIndex* index;
      // if we store for orig a generalization of its instance inst, we block:
      return lit2index.find(orig,index) && index->getGeneralizations(inst,false,false).hasNext();
    }

    ~DismatchingConstraintsGeneral() {
      Lit2Index::Iterator iit(lit2index);
      while(iit.hasNext()){
        DismatchingLiteralIndex* index = iit.next();
        delete index;
      }
    }
  };

  /**
   * A struct for holding clause's dms, on per literal basis.
   * This version is shallow i.e. assumes substitutions are shallow 
   */
  struct DismatchingConstraintsShallow : public DismatchingConstraints {

    // Very importantly, we assume that the variable bank used is 0
    // i.e. QRS_QUERY_BANK as defined in Indexing/LiteralSubstitutionTree
    static const int query_bank = 0;

    // map orig lits to subs related to them
    // store seen substitutions by their size
    // when checking for inclusion we cannot be included by a bigger one!
    DHMap<Literal*,Array<List<DHMap<unsigned,TermList>*>*>*> lit2subs;

    DHMap<unsigned,TermList>* translate(Literal* orig, RobSubstitution* subst){
      CALL("DismatchingConstraintsShallow::translate");

      DHMap<unsigned,TermList>* m = new DHMap<unsigned,TermList>();
      VariableIterator vit(orig);
      while(vit.hasNext()){
        TermList var = vit.next();
        ASS(var.isVar());
        TermList t = subst->apply(var,query_bank);
        m->insert(var.var(),t);
      }
      return m;
    }

    Array<List<DHMap<unsigned,TermList>*>*>* getSubs(Literal* orig){
      Array<List<DHMap<unsigned,TermList>*>*>* ret;
      if(!lit2subs.find(orig,ret)){
        ret = new Array<List<DHMap<unsigned,TermList>*>*>();
        lit2subs.insert(orig,ret);
      }
      return ret;
    }

    void add(Literal* orig, Literal* inst, RobSubstitution* subst) {
      CALL("DismatchingConstraintsShallow::add");
      DHMap<unsigned,TermList>* m = translate(orig,subst);
      unsigned size = m->size();
      Array<List<DHMap<unsigned,TermList>*>*>* subs = getSubs(orig); 
      List<DHMap<unsigned,TermList>*>* subsList = subs->get(size);
      (*subs)[size] = subsList->cons(m);
    }

    bool shouldBlock(Literal* orig, Literal* inst, RobSubstitution* subst) {
      CALL("DismatchingConstraintsShallow::shouldBlock");

      DHMap<unsigned,TermList>* m = translate(orig,subst);
      Array<List<DHMap<unsigned,TermList>*>*>* subs = getSubs(orig);

      for(unsigned i=1;i<m->size();i++){
        List<DHMap<unsigned,TermList>*>::Iterator it(subs->get(i)); 
        while(it.hasNext()){
          DHMap<unsigned,TermList>* existing = it.next();
          // now check if existing generalises subst
          // i.e. if they are consistent and the size of existing
          //      is smaller or equal to subst
          VirtualIterator<unsigned> dit = existing->domain();
          while(dit.hasNext()){
            unsigned v = dit.next();
            // if m binds v it should bind it in the same way
            TermList other;
            if(m->find(v,other)){
              if(!existing->get(v).sameContent(&other)) return true;
            } 
          }
        }
      }

      return false;
    }

    ~DismatchingConstraintsShallow() {
    }
  };

  typedef DHMap<Clause*,DismatchingConstraints*> DismatchMap;

  DismatchMap _dismatchMap;

  /**
   * The internal representation of all the clauses inside IG
   * must replace the equality symbol with a proxy.
   * The main reason is that equalities in term sharing
   * assume non-deterministic orientations and
   * most of indexing is done "modulo orientation of equality",
   * which is undesirable for InstGen.
   */
  EqualityProxy* _equalityProxy;
};

}

#endif // __IGAlgorithm__
