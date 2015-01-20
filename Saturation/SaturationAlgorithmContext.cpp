/**
 * @file SaturationAlgorithmContext.cpp
 *
 * @since 2 Jun 2014
 * @author dmitry
 */

#include "SaturationAlgorithmContext.hpp"

#include "Kernel/MainLoopContextFwd.hpp"
#include "Kernel/ProblemFwd.hpp"

#include "SAT/SAT2FO.hpp"

#include "Saturation/SaturationAlgorithm.hpp"
#include "Saturation/SSplitter.hpp"

namespace Saturation {

using Kernel::MainLoopContext;
using Kernel::Problem;
using Shell::Options;
using SAT::SAT2FO;

bool SaturationAlgorithmContext::_branchSelectorInitialised = false;
SAT2FO SaturationAlgorithmContext::_sat2fo;
SSplittingBranchSelector SaturationAlgorithmContext::_branchSelector(&SaturationAlgorithmContext::_sat2fo);
ClauseVariantIndex SaturationAlgorithmContext::_componentIdx;
Lib::DHMap<Kernel::Clause*,Kernel::SplitLevel> SaturationAlgorithmContext::_compNames;

SaturationAlgorithmContext::SaturationAlgorithmContext(Problem& prb, Options& opts):
		MainLoopContext(prb, opts) {
	CALL("SaturationAlgorithmContext::SaturationAlgorithmContext");

	AutoSwitch s(this);
	SaturationAlgorithm* sa = SaturationAlgorithm::createFromOptions(*_prb, opts);
        ASS(sa);
	_ml = sa;
	// splitter is only set of splitting is on in the options
        if(sa->splitter()){
		_splitter = static_cast<SSplitter*>(sa -> splitter());
		//[dmitry] TODO: Merge Splitter and SSplitter and remove this cast
		//[giles] this merge will happen when we rebase on master
		if(!_branchSelectorInitialised){
			//_sat2fo = SAT2FO();
			//_branchSelector = SSplittingBranchSelector(&_sat2fo);
			_branchSelector.init(opts);
			_branchSelectorInitialised = true;
		}
		_splitter -> setBranchSelector(&_branchSelector);
		_splitter -> setComponentIndex(&_componentIdx);
		_splitter -> setSAT2FO(&_sat2fo);
  		_splitter -> setComponentNames(&_compNames);
        }
}

SaturationAlgorithmContext::~SaturationAlgorithmContext() {
	CALL("SaturationAlgorithmContext::~SaturationAlgorithmContext");

	AutoSwitch s(this);
	delete _ml;
}

};
