#include "ControlPBP.h"
#include "DynamicPdfSampler.h"
#define ENABLE_DEBUG_OUTPUT
#include "Debug.h"
//#include "SamplingTree.h"
#include "IndexRandomizer.h"
#include "EigenMathUtils.h"
#include <iostream>
using namespace Eigen;
namespace AaltoGames
{
	/* TODO:

	Fwd propagation:
	Model prior as two-component Gaussian mixture (best samples + "uniform") (or compute this at the previous back propagation)
	Sample otherwise as currently

	Backpropagation:

	let s_opt(nSteps) = ending state of best sample, or E(state(nSteps)) based on weighed samples
	let c_opt denote the optimal control
	for (int step=nSteps; step>0; step--)
	{
		model p(state(step-1),control(step-1),state(step)) as Gaussian;  //do we have to multiply in the control prior and cost(state(step-1)) or just weight the samples using them? Maybe at least the control prior
		s_opt(step-1),c_opt(step-1) <- E[state(step-1), control(step-1) | state(step)=s_opt(step)]

	}

	Alternative backpropagation:

	let p_bwd(nSteps)=p(state(nSteps))  //gaussian based on samples weighed using the terminal cost
	for (int step=nSteps; step>0; step--)
	{
		p_bwd(step-1) <- marginal of p_bwd(step) p_fwd(state(step-1),control(step-1),state(step)), where the latter component is a gaussian based on weighed samples from fwd pass
	}


	Visualize: show how the state marginal gaussian is affected by the back propagation

	Alternative backpropagation 2, similar to the alternative above but with diagonal GMMs:

	similar to current, but w_(step,i)_(step-1,j) = p(state(step-1,j),control(step-1,j),state(step,i)), where p(control(step-1,j)) is the next iteration's proposal gaussian, and thus the density can be evaluated once we know how control std maps to state std

	*/
	static bool gaussianAroundBest=false;   //if true, the gaussian back propagation uses the best path as the mean instead of the population weighed mean
	static bool useControlPrior=true;
	static double uniformBias=0.2;	//note that as the uniform bias is implemented using a wide Gaussian that spans way beyond the sampling bounds, the bias does not integrate to 1 over the bounds => have to set this closer to 1 than what would be intuitive
	static float statePriorKernelScale=1.0f;
//	static float volumePow=0.0f;
//	static SamplingTree samplingTree;
	static IndexRandomizer indexRandomizer;
	static bool keepOldBest=true;
	static float bestSmoothAmount=0.0f;  //0...1
	static double resampleThreshold=0.5;  //0 = never resample, >1 = always resample
	static const double maxSqCost=10.0;	 //costs of samples will be scaled so that minimum sample cost does not exceed this
	static bool useGaussianBackPropagation=true;
	static float gbpRegularization=0.001f;
	void ControlPBP::init(int nSamples, int nSteps, int nStateDimensions, int nControlDimensions, const float *controlMinValues, const float *controlMaxValues, const float *controlMean, const float *controlPriorStd, const float *controlDiffPriorStd, const float *controlDiffDiffPriorStd, float controlMutationStdScale, const float *stateKernelStd)
	{
		iterationIdx=0;
		marginals.resize(nSteps+1);
		prior.resize(nSteps+1);
		this->nSamples=nSamples;
		fullSamples.resize(nSamples);
		fullSampleCosts.resize(nSamples);
		this->nSteps=nSteps;
		this->nStateDimensions=nStateDimensions;
		this->nControlDimensions=nControlDimensions;
		this->controlMin.resize(nControlDimensions);
		this->controlMax.resize(nControlDimensions);
		this->controlMean.resize(nControlDimensions);
		this->controlPriorStd.resize(nControlDimensions);
		this->controlDiffPriorStd.resize(nControlDimensions);
		this->controlDiffDiffPriorStd.resize(nControlDimensions);
		this->stateKernelStd.resize(nStateDimensions);
		gaussianBackPropagated.resize(nControlDimensions*nSteps);
		gaussianBackPropagated.setZero();
		oldBest.resize(nSteps+1);
		memcpy(&this->controlMin[0],controlMinValues,sizeof(float)*nControlDimensions);
		memcpy(&this->controlMax[0],controlMaxValues,sizeof(float)*nControlDimensions);
		if (controlMean!=NULL)
		{
			memcpy(&this->controlMean[0],controlMean,sizeof(float)*nControlDimensions);
		}
		else
		{
			this->controlMean.setZero();
		}
		for (int step=0; step<=nSteps; step++)
		{
			oldBest[step].init(nStateDimensions,nControlDimensions);
			marginals[step].resize(nSamples);
			for (int i=0; i<nSamples; i++)
			{
				marginals[step][i].init(nStateDimensions,nControlDimensions);
			}
		}
		selector=new DynamicPdfSampler(nSamples);
		conditionalSelector=new DynamicPdfSampler(nSamples*nSamples);
		bestCost=FLT_MAX;  //not yet found any solution
		timeAdvanced=false;
		//kd-tree
		indexRandomizer.init(nSamples);
		Eigen::VectorXf fullMin(nControlDimensions*nSteps),fullMax(nControlDimensions*nSteps);
		for (int step=0; step<nSteps; step++)
		{
			for (int d=0; d<nControlDimensions; d++)
			{
				fullMin[step*nControlDimensions+d]=controlMin[d];
				fullMax[step*nControlDimensions+d]=controlMax[d];
			}
		}
//		samplingTree.init(nControlDimensions*nSteps,0,fullMin.data(),fullMax.data(),nSamples*3);
		setSamplingParams(controlPriorStd,controlDiffPriorStd,controlDiffDiffPriorStd,controlMutationStdScale,stateKernelStd);
		bestFullSampleIdx=0;
	}
	ControlPBP::~ControlPBP()
	{
		delete selector;
		delete conditionalSelector;
	}

	void  ControlPBP::setSamplingParams( const float *controlPriorStd, const float *controlDiffPriorStd, const float *controlDiffDiffPriorStd, float controlMutationStdScale, const float *stateKernelStd )
	{
		if (stateKernelStd!=NULL)
		{
			memcpy(&this->stateKernelStd[0],stateKernelStd,sizeof(float)*nStateDimensions);
			useStateKernels=true;
			for (int step=0; step<=nSteps; step++)
			{
				//Init the priors (used as priors & proposals in the sampling) to have a single almost uniform component (i.e. no effect on sampling)
				//Note that we do this only at first iteration or if the prior is not initialized. Subsequent calls do not reinit the prior, as
				//it is updated at each call to endIteration()
				if (prior[step].mean.size()==0 || iterationIdx==0)
				{
					prior[step].resize(1,nStateDimensions+nControlDimensions);
					for (int d=0; d<nStateDimensions; d++)
					{
						prior[step].mean[0][d]=0;
						prior[step].std[0][d]=10.0f*stateKernelStd[d];
					}
					for (int d=nStateDimensions; d<nStateDimensions+nControlDimensions; d++)
					{
						prior[step].mean[0][d]=controlMean[d-nStateDimensions];
						prior[step].std[0][d]=10.0f*controlPriorStd[d];
					}
				}
			}
		}
		else
		{
			useStateKernels=false;
		}
		memcpy(&this->controlPriorStd[0],controlPriorStd,sizeof(float)*nControlDimensions);
		memcpy(&this->controlDiffPriorStd[0],controlDiffPriorStd,sizeof(float)*nControlDimensions);
		memcpy(&this->controlDiffDiffPriorStd[0],controlDiffDiffPriorStd,sizeof(float)*nControlDimensions);
		controlMutationStd=controlMutationStdScale*this->controlPriorStd;
		staticPrior.resize(1,nControlDimensions);
		staticPrior.mean[0]=controlMean;
		staticPrior.setStds(this->controlPriorStd);
		staticPrior.weights[0]=1;
		staticPrior.weightsUpdated();
	}

	//Here, we do two things:
	//1) adaptively scale the costs so that the weights do not all fall to zero,
	//2) implement the PBP forward message summation
	void ControlPBP::finalizeWeights(int step )
	{
		double total=0;
		double maxUnnormalized=0;
		double minFoundSqCost=FLT_MAX;
		std::vector<MarginalSample> &v=marginals[step+1];  //the weights will be used at the next step
		for (int i=0; i<nSamples; i++)
		{
			minFoundSqCost=_min(minFoundSqCost,v[i].stateCost);
		}
		if (minFoundSqCost > maxSqCost)
		{
			double scale=maxSqCost/minFoundSqCost;
			for (int i=0; i<nSamples; i++)
			{
				v[i].stateCost*=scale;
				//v[i].controlCost*=scale;
			}
		}
		//convert costs to potentials (here equal to weights, as it is assumed that the control cost is also included in the proposal and can thus be ignored due to importance weighting)
		for (int i=0; i<nSamples; i++)
		{
			v[i].statePotential=exp(-0.5*v[i].stateCost);
			v[i].controlPotential=exp(-0.5*v[i].controlCost);
		}

		//compute the forward message values and forward beliefs for each sample (forward belief = state potential * forward message from previous node
		VectorXf stateDiff(nStateDimensions);
		for (int i=0; i<nSamples; i++)
		{
			if (step==0)
			{
				v[i].fwdMessage=1;
				v[i].forwardBelief=v[i].statePotential;
			}
			else
			{
				if (useStateKernels)
				{
					double sum=0;
					for (int j=0; j<nSamples; j++)
					{
						MarginalSample &previousJ=marginals[step][v[i].previousMarginalSampleIdx];
						stateDiff=previousJ.state-v[i].state;
						stateDiff=stateDiff.cwiseQuotient(stateKernelStd);
						double transitionPotentialJtoI=exp(-0.5*stateDiff.dot(stateDiff));
						sum+=transitionPotentialJtoI*previousJ.forwardBelief;  //previousJ.forwardBelief=previousJ.statePotential*previousJ.fwdMessage;
					}
					v[i].fwdMessage=sum;
				}
				else
				{
					MarginalSample &previousSample=marginals[step][v[i].previousMarginalSampleIdx];
					v[i].fwdMessage=previousSample.forwardBelief;
				}
				v[i].forwardBelief=v[i].statePotential;
				if (!resample)
					v[i].forwardBelief*=marginals[step][v[i].previousMarginalSampleIdx].forwardBelief;
			}
		}

		//now normalize weights
		for (int i=0; i<nSamples; i++)
		{
			total+=v[i].forwardBelief;
			maxUnnormalized=_max(v[i].forwardBelief,maxUnnormalized);
		}
		if (validFloat(maxUnnormalized/total))
		{
			for (int i=0; i<nSamples; i++)
			{
				v[i].forwardBelief/=total;
			}
		}
		else
		{
			//if normalization fails, reinit uniform weights
			for (int i=0; i<nSamples; i++)
			{
				v[i].forwardBelief=1.0/(double)nSamples;
			}
		}



	}
	void ControlPBP::update( const float *currentState, TransitionFunction transitionFwd, StateCostFunction stateCostFunction, OnStepDone onStepDone )
	{
		//Backward pass, NBP if a backwards transition function defined

		startIteration(false, currentState);
		static std::vector<VectorXf> sampleStates(nSamples);
		static std::vector<VectorXf> nextSampleStates(nSamples);
		for (int i=0; i<nSamples; i++)
		{
			if (sampleStates[i].rows()==0)
				sampleStates[i].resize(nStateDimensions);
			memcpy(&sampleStates[i][0],currentState,sizeof(float)*nStateDimensions);
		}
		static VectorXf control(nControlDimensions);
		static VectorXf newState(nStateDimensions);
		for (int step=0; step<nSteps; step++)
		{
			startPlanningStep(step);
			for (int i=0; i<nSamples; i++)
			{
				int currentStateIdx=-1;
				currentStateIdx=getPreviousSampleIdx(i);
				getControl(i,&control[0]);
				transitionFwd(step,sampleStates[currentStateIdx].data(),control.data(),&newState[0]);
				nextSampleStates[i]=newState;
				updateResults(i,control.data(),newState.data(),stateCostFunction(step+1,newState.data()));
			}
			sampleStates=nextSampleStates;
			endPlanningStep(step);
			//TODO: different operation without kernels - old best with priorSampleIdx==0 (sorted best) for resampling, uniform inits with priorSampleIdx==-1 (stay uniform after resampling)
//			propagate(step,true,transitionFwd,stateCostFunction,onStepDone);
			if (onStepDone)
				onStepDone();
		}
		endIteration();
	}

	double ControlPBP::getSquaredCost()
	{
		return bestCost;
	}
/*

	void ControlPBP::getConditionalControlGMM( int timeStep, const Eigen::VectorXf &state, DiagonalGMM &dst )
	{
		__declspec(thread) static DiagonalGMM *tmp=NULL;
		if (tmp==NULL)
			tmp=new DiagonalGMM();
		prior[timeStep].makeConditional(state,*tmp);
		DiagonalGMM::multiply(staticPrior,*tmp,dst);
	}
*/
	void  ControlPBP::startIteration(bool advanceTime, const float *initialState)
	{
		int nUniform=(int)(uniformBias*(float)nSamples);
		for (int i=0; i<nSamples; i++)
		{
			memcpy(&marginals[0][i].state[0],initialState,sizeof(float)*nStateDimensions);
			marginals[0][i].control.setZero();
			marginals[0][i].previousControl.setZero();
			marginals[0][i].previousPreviousControl.setZero();
			marginals[0][i].forwardBelief=1;
			marginals[0][i].fwdMessage=1;
			marginals[0][i].fullCost=0;
			marginals[0][i].stateCost=0;
			marginals[0][i].previousMarginalSampleIdx=i;
			marginals[0][i].bestStateCost=FLT_MAX;
			marginals[0][i].fullControlCost=0;
			//assign marginal samples to full-dimensional prior samples if available
			if ((iterationIdx>0) && (i > nUniform))
				marginals[0][i].priorSampleIdx=selector->sample();
			else
				marginals[0][i].priorSampleIdx=-1;  //negative value signals that the sample is not following any prior sample
		}
		if (advanceTime && iterationIdx>0)
		{

			for (int step=0; step<nSteps-1; step++)
			{
				if (useStateKernels)
					prior[step].copyFrom(prior[step+1]);
				oldBest[step]=oldBest[step+1];
				for (int sample=0; sample<nSamples; sample++)
				{
					fullSamples[sample][step]=fullSamples[sample][step+1];
				}
				gaussianBackPropagated.block(step*nControlDimensions,0,nControlDimensions,1)=gaussianBackPropagated.block((step+1)*nControlDimensions,0,nControlDimensions,1);
			}
			//for correct control difference minimization, we update all marginal samples assuming that the system has been driven towards using the previous best sample
			//TODO: let client specify the applied control
			for (int i=0; i<nSamples; i++)
			{
				marginals[0][i].control=oldBest[0].control;
				marginals[0][i].previousControl=oldBest[0].previousControl;
				marginals[0][i].previousPreviousControl=oldBest[0].previousPreviousControl;
			}
		}
		timeAdvanced=advanceTime;
	}

	int  ControlPBP::getBestSampleLastIdx()
	{
		return bestFullSampleIdx;
	}

	float ControlPBP::getGBPRegularization()
	{
		return gbpRegularization;
	}


	void  ControlPBP::endIteration()
	{
		//The PBP backwards pass
		for (int i=0; i<nSamples; i++)
		{
			marginals[nSteps][i].belief=marginals[nSteps][i].forwardBelief;
			marginals[nSteps][i].bwdMessage=1;
		}
		VectorXf stateDiff(nStateDimensions);
		for (int step=nSteps-1; step>=0; step--)
		{
			double totalPropagated=0;
			//first set backward messages to 0 to account for cases where resampling terminated the path and zero state kernel does not transfer any belief
			for (int i=0; i<nSamples; i++)
			{
				marginals[step][i].bwdMessage=0;
			}

			//update messages
			for (int i=0; i<nSamples; i++)
			{
				if (!useStateKernels)
				{
					MarginalSample &src=marginals[step+1][i];
					MarginalSample &dst=marginals[step][src.previousMarginalSampleIdx];
					dst.bwdMessage=src.statePotential*src.bwdMessage;
				}
				else
				{
					double total=0;
					MarginalSample &dst=marginals[step][i];
					for (int j=0; j<nSamples; j++)
					{
						MarginalSample &src=marginals[step+1][j];
						stateDiff=src.previousState-dst.state;//previousState as the state needs to be propagated through the dynamics
						stateDiff=stateDiff.cwiseQuotient(stateKernelStd);
						double transitionPotentialSrcToDst=exp(-0.5*stateDiff.dot(stateDiff));
						total+=transitionPotentialSrcToDst*src.statePotential*src.bwdMessage;
					}
					dst.bwdMessage=total;
				}
			}

			//finally, update belief. This is in a separate loop, as the zero state kernel loop above only iterates through the marginal states connected to next step
			for (int i=0; i<nSamples; i++)
			{
				MarginalSample &sample=marginals[step][i];
				sample.belief=sample.statePotential*sample.fwdMessage*sample.bwdMessage;
			}
		} //PBP backwards pass


		//Recover the full posterior samples (all backtracked paths from end to beginning (tree leaves to root)
		//The result is a linearly indexed array that can be more easily used as a prior in the
		for (int i=0; i<nSamples; i++)
		{
			for (int step=nSteps; step>=0; step--)
			{
				marginals[step][i].fullSampleIdx=-1;
			}
		}
		double minFullCost=FLT_MAX;
		for (int fullIdx=0; fullIdx<nSamples; fullIdx++)
		{
			fullSamples[fullIdx].resize(nControlDimensions*nSteps);
			double w=1;
			float controlCost=0;
			int marginalIdx=fullIdx;
			fullSampleCosts[fullIdx]=marginals[nSteps][marginalIdx].fullCost;
			minFullCost=_min(minFullCost,fullSampleCosts[fullIdx]);
			for (int step=nSteps; step>0; step--)
			{
				marginals[step][marginalIdx].fullSampleIdx=fullIdx;
				//we index by step-1 because the control is "incoming", i.e., control that resulted in the marginal state
				fullSamples[fullIdx].block((step-1)*nControlDimensions,0,nControlDimensions,1)=marginals[step][marginalIdx].control;
				marginalIdx=marginals[step][marginalIdx].previousMarginalSampleIdx;
			}
		}
		//cap the minimum full sample cost and convert costs to weights for resampling
		double scale=1.0;

		if (minFullCost > maxSqCost)
		{
			scale=maxSqCost/minFullCost;
		}

		for (int fullIdx=0; fullIdx<nSamples; fullIdx++)
		{
	//		selector->setDensity(fullIdx,exp(-0.5f*fullSampleCosts[fullIdx]*scale));  //this includes the control costs doubly (as it's also in the potential). However without the explicitly included control costs, the initial guesses get even more biased weights
			selector->setDensity(fullIdx,marginals[nSteps][fullIdx].belief); //belief explicitly contains state potentials, implicitly contains control potentials (included in the proposal)
		}

		//The kd-tree part not in use. Does not work - effectively cancels the kernel backpropagation, as paths that were terminated
		//in the fwd pass resampling are not part of any full control vector. The correct way to use this would be to resample
		//backwards, creating new paths through the kernel propagation and then storing all these in the tree.

		//build multiple kd-trees to estimate the volume occupied by each sample, then use this to importance-weight the samples
	/*	if (volumePow>0)
		{
			std::vector<double> fullSampleVolumes(nSamples);
			for (int i=0; i<nSamples; i++)
			{
				fullSampleVolumes[i]=0;
			}
			for (int k=0; k<10; k++)
			{
				samplingTree.clear(false);
				indexRandomizer.init(nSamples);
				for (int i=0; i<nSamples; i++)
				{

					samplingTree.putSample(&fullSamples[indexRandomizer.get()][0],1);
				}
				for (int i=0; i<nSamples; i++)
				{
					fullSampleVolumes[i]+=samplingTree.findNodeAt(&fullSamples[i][0])->volumePow;
				}
			}
			for (int i=0; i<nSamples; i++)
			{
				selector->setDensity(i,pow(exp(-0.5f*fullSampleCosts[i]*scale),volumePow));
			}
		}
		*/

		//store the best full-dimensional sample and its cost
		bestCost=FLT_MAX;
		int bestIdx=0;
		for (int i=0; i<nSamples; i++)
		{
			if (fullSampleCosts[i]<bestCost)
			{
				bestCost=fullSampleCosts[i];
				bestIdx=i;
			}
/*			if (marginals[nSteps][i].fullCost < bestCost)
			{
				bestIdx=i;
				bestCost=marginals[nSteps][i].fullCost;
			}
			*/
		}
		bestFullSampleIdx=bestIdx;	//index of the last marginal sample of the best full sample
		for (int step=nSteps; step>=0; step--)
		{
			oldBest[step]=marginals[step][bestIdx];
			bestIdx=marginals[step][bestIdx].previousMarginalSampleIdx;
		}

		//attempt a local refinement step
		if (useGaussianBackPropagation)
			gaussianBackPropagation();


		//If we are not using the marginal state-space kernels (i.e., we are relying on full-dimensional prior samples), we're done
		if (!useStateKernels)
		{
			iterationIdx++;
			return;
		}
		//update the prior
		for (int step=0; step<nSteps; step++)
		{
			/*  //Uniform bias not in use to make the Q=0 and Q!=0 cases as similar as possible. The uniform bias component is also not working that great.
			//Marginal prior for each time step is a GMM based on the samples and backpropagated weights, plus an additional Gaussian component
			//that approximates a user-adjustable uniform bias to facilitate more global search
			prior[step].resize(nSamples,nStateDimensions+nControlDimensions);

			VectorXf stateMean(nStateDimensions);
			stateMean.setZero();
			for (int i=0; i<nSamples; i++)
			{
				stateMean+=marginals[step][i].state;
			}
			stateMean/=(float)nSamples;

			VectorXf stateVar(nStateDimensions);
			stateVar.setZero();
			for (int i=0; i<nSamples; i++)
			{
				for (int j=0; j<nStateDimensions; j++)
				{
					stateVar[j]+=squared(marginals[step][i].state[j]-stateMean[j]);
				}
			}
			stateVar/=(float)nSamples;
			VectorXf stateStd(nStateDimensions);
			for (int j=0; j<nStateDimensions; j++)
			{
				stateStd[j]=_max(stateKernelStd[j]*statePriorKernelScale,sqrtf(stateVar[j]));  //need the max in case all samples have same state
			}
			prior[step].mean[nSamples].block(0,0,nStateDimensions,1)=stateMean;			//state mean is the mean of the marginal states
			prior[step].mean[nSamples].block(nStateDimensions,0,nControlDimensions,1).setConstant(0);	//control mean is 0
			prior[step].std[nSamples].block(0,0,nStateDimensions,1)=stateStd*2.0f;
			prior[step].std[nSamples].block(nStateDimensions,0,nControlDimensions,1)=4.0f*(controlMax-controlMin);		//control std is several times the possible range of control values
			prior[step].weights[nSamples]=uniformBias;	//user-adjusted parameter
			*/
			prior[step].resize(nSamples,nStateDimensions+nControlDimensions);

			//init components, one for each sample
			for (int i=0; i<nSamples; i++)
			{
				//state mean and std. Note that we index the messages by step+1, as each message contains the "incoming" control, i.e., the control that brought it to the state
				prior[step].mean[i].block(0,0,nStateDimensions,1)=marginals[step+1][i].previousState;
				prior[step].std[i].block(0,0,nStateDimensions,1)=stateKernelStd*statePriorKernelScale;	//user-adjusted. TODO: automatically infer optimal kernel based on the simulation data

				//control mean and std
				prior[step].mean[i].block(nStateDimensions,0,nControlDimensions,1)=marginals[step+1][i].control;	//sample around previous control
				prior[step].std[i].block(nStateDimensions,0,nControlDimensions,1)=controlMutationStd;		//user-adjusted

				//weight. marginals weights sum to 1 at this point. marginals weight represents the "fitness" of the paths that can be taken from this timestep onwards by using the control mean and std above
				prior[step].weights[i]=(1.0-uniformBias)*marginals[step+1][i].belief;
//					prior[step].weights[i]=marginals[step+1][i].weight;//*fwdMessage[step+1][i].weight;
			}
			prior[step].weightsUpdated();
#if 0				//this part not in use as related to the kd-tree stuff above
				if (volumePow>0)
				{
				//	TODO this doesn't work as a single marginal sample may be a part of multiple full-dimensional samples (due to resampling)
					double volume=0;
					int fullSampleIdx=fwdMessage[step+1][i].fullSampleIdx;
					if (fullSampleIdx>=0)	//-1 denotes this evaluated marginal state was not part of any full sample
					{
						volume=pow(fullSampleVolumes[fullSampleIdx],volumePow);
					}
					prior[step].weights[i]*=volume;
				}
#endif
		}
		iterationIdx++;

	}

	void  ControlPBP::startPlanningStep( int step)
	{
		currentStep=step;
		nextStep=currentStep+1;

		//Check for resampling
		VectorXd weights(nSamples);
		for (int sample=0; sample<nSamples; sample++)
		{
			weights[sample]=marginals[currentStep][sample].forwardBelief;
		}
		weights/=weights.sum();
		double effCount=1.0/weights.squaredNorm();
		resample=(effCount < resampleThreshold*(double)nSamples) || step==0;
		if (resample)
		{
			for (int sample=0; sample<nSamples; sample++)
			{
				selector->setDensity(sample,weights[sample]);
			}
		}

		//keep track of how many initial guesses we have
		nInitialGuesses=0;
		if (bestCost<FLT_MAX && keepOldBest)  //bestCost<FLT_MAX means that the previous iteration found some path that is not totally useless
		{
			nInitialGuesses++;
			if (useGaussianBackPropagation)
				nInitialGuesses++;
		}

		//keep track of the forward sampling tree linkage
		for (int sampleIdx=0; sampleIdx<nSamples; sampleIdx++)
		{
			MarginalSample &nextSample=marginals[nextStep][sampleIdx];
			if (sampleIdx<nInitialGuesses || !resample)
			{
				nextSample.previousMarginalSampleIdx=sampleIdx;
			}
			else
			{
				nextSample.previousMarginalSampleIdx=selector->sample();
			}
			nextSample.priorSampleIdx=marginals[currentStep][nextSample.previousMarginalSampleIdx].priorSampleIdx;
		}

	}

	void  ControlPBP::endPlanningStep( int stepIdx )
	{
		finalizeWeights(stepIdx);
	}

	void  ControlPBP::updateResults( int sampleIdx, const float *finalControl, const float *newState, double squaredStateCost, const float *priorMean, const float *priorStd  )
	{
		MarginalSample &nextSample=marginals[nextStep][sampleIdx];

		//simulate and evaluate
		memcpy(&nextSample.state[0],newState,sizeof(float)*nStateDimensions);
		memcpy(&nextSample.control[0],finalControl,sizeof(float)*nControlDimensions);

		//Note: when resampling, we don't multiply by the weight from the previous time step (in SIR resample, the weight of each particle is set to 1/nSamples).
		//Note that control potential exp(-0.5f*controlCost) is not part of this, as it is also included in the proposal => canceled out in importance weighting
		nextSample.stateCost=squaredStateCost;
		MarginalSample &previousSample=marginals[currentStep][nextSample.previousMarginalSampleIdx];

		//update full cost.
		float controlCost=0;
		for (int d=0; d<nControlDimensions; d++)
		{
			controlCost+=squared(nextSample.control[d]-controlMean[d])/squared(controlPriorStd[d]);  //control minimization
			controlCost+=squared(nextSample.control[d]-nextSample.previousControl[d])/squared(controlDiffPriorStd[d]);  //control difference minimization
			controlCost+=squared(nextSample.control[d]-2.0f*nextSample.previousControl[d]+nextSample.previousPreviousControl[d])/squared(controlDiffDiffPriorStd[d]);  //control difference minimization
			//factor in the optional prior defined by the client
			if (priorMean!=NULL && priorStd!=NULL)
			{
				controlCost+=squared(nextSample.control[d]-priorMean[d])/squared(priorStd[d]);
			}
		}
		nextSample.controlCost=controlCost;
		if (currentStep==0)
			nextSample.fullControlCost=controlCost;
		else
			nextSample.fullControlCost=previousSample.fullControlCost+controlCost;
		if (currentStep > (int)(0.5/0.033f)/5)	//EXPERIMENTAL (TODO)
		{
			nextSample.bestStateCost=_min(previousSample.bestStateCost,squaredStateCost);
		}
		else
		{
			nextSample.bestStateCost=FLT_MAX;
		}
		nextSample.fullCost=previousSample.fullCost+squaredStateCost+controlCost;//+nextSample.stateDeviationCost; //TODO: should we use the state deviation cost or not?

		//printf("control cost in loop %f \n", nextSample.fullCost);

	}

	void  ControlPBP::getBestControl( int timeStep, float *out_control )
	{
		//the +1 because oldBest stores marginal states which store "incoming" controls instead of "outgoing"
		memcpy(out_control,oldBest[timeStep+1].control.data(),nControlDimensions*sizeof(float));
	}

	void  ControlPBP::setParams( double _uniformBias, double _resampleThreshold, bool _useGaussianBackPropagation, float _gbpRegularization )
	{
		uniformBias=_uniformBias;
		resampleThreshold=_resampleThreshold;
		useGaussianBackPropagation=_useGaussianBackPropagation;
		gbpRegularization=_gbpRegularization;
	}
	int  ControlPBP::getPreviousSampleIdx(int sampleIdx)
	{
		MarginalSample &nextSample=marginals[nextStep][sampleIdx];
		return nextSample.previousMarginalSampleIdx;
	}

	void  ControlPBP::getControl( int sampleIdx, float *out_control, const float *priorMean, const float *priorStd)
	{

		//printf("getting control \n");

		if (!useStateKernels)
		{

			
			getControlWithoutStateKernel(sampleIdx,out_control, priorMean,priorStd);
				

			return;
		}
		//link the marginal samples to each other so that full-dimensional samples can be recovered
		MarginalSample &nextSample=marginals[nextStep][sampleIdx];
		MarginalSample &currentSample=marginals[currentStep][nextSample.previousMarginalSampleIdx];
		Eigen::Map<VectorXf> control(out_control,nControlDimensions);
		nextSample.stateDeviationCost=0;
		//special processing for initial guesses
		if (sampleIdx<nInitialGuesses && (currentStep<nSteps-1 || !timeAdvanced))
		{
			//the old best solution
			if (sampleIdx==0)
			{
				control=oldBest[nextStep].control;	//nextStep as index because the control is always stored to the next sample ("control that brought me to this state")
			}
			else if (sampleIdx==1)
			{
				control=gaussianBackPropagated.block(currentStep*nControlDimensions,0,nControlDimensions,1);
			}
		}
		//processing for samples other than initial guesses
		else
		{
			//compute the conditional density for new controls, multiplied with other priors
		     static	__thread DiagonalGMM *controlGivenState=NULL;
			 static __thread DiagonalGMM *proposal=NULL;
			 static __thread DiagonalGMM *diffPrior=NULL;
			 static __thread DiagonalGMM *singleGaussianPrior=NULL;
			if (controlGivenState==NULL)
			{
				proposal=new DiagonalGMM();
				controlGivenState=new DiagonalGMM();
				diffPrior=new DiagonalGMM();
				diffPrior->resize(1,nControlDimensions);
				singleGaussianPrior=new DiagonalGMM();
				singleGaussianPrior->resize(1,nControlDimensions);
			}


			//First, compute all priors that can be expressed as a single gaussian

			//Initialize with the "static" prior
			singleGaussianPrior->copyFrom(staticPrior);

			//Multiply the "static" prior with first difference
			if (!(iterationIdx==0 && currentStep==0))	//prior for difference not available at first step of first iteration
			{
				diffPrior->mean[0]=currentSample.control;
				diffPrior->std[0]=controlDiffPriorStd;
				diffPrior->weights[0]=1;
				diffPrior->weightsUpdated();
				DiagonalGMM::multiply(*diffPrior,*singleGaussianPrior,*singleGaussianPrior);
			}
			//Multiply in the second difference prior
			if (!(iterationIdx==0 && currentStep<2))	//prior for difference not available at first two steps of first iteration
			{
				diffPrior->mean[0]=2.0f*currentSample.control-currentSample.previousControl;
				diffPrior->std[0]=controlDiffDiffPriorStd;
				diffPrior->weights[0]=1;
				diffPrior->weightsUpdated();
				DiagonalGMM::multiply(*diffPrior,*singleGaussianPrior,*singleGaussianPrior);
			}
			if (iterationIdx==0 || (currentStep==nSteps-1 && timeAdvanced) || (nextSample.priorSampleIdx<0))
			{
				//At first iteration or last step, just sample from the single gaussian prior
				singleGaussianPrior->sampleWithLimits(control,controlMin,controlMax);
			}
			else
			{
				//At subsequent iterations, also multiply in the conditional density p(control | state)
				double unnormalizedWeights=prior[currentStep].makeConditional(currentSample.state,*controlGivenState);
				if (unnormalizedWeights > 1e-40)
				{
					DiagonalGMM::multiply(*singleGaussianPrior,*controlGivenState,*proposal);
					proposal->sampleWithLimits(control,controlMin,controlMax);
					nextSample.stateDeviationCost=-2.0*log(unnormalizedWeights);
				}
				else
				{
					singleGaussianPrior->sampleWithLimits(control,controlMin,controlMax);
					nextSample.stateDeviationCost=-2.0*log(1e-40);
				}
			}
		}
		nextSample.control=control;
		nextSample.previousState=currentSample.state;
		nextSample.previousControl=currentSample.control;  //needed later for computing control cost with jerk term
		nextSample.previousPreviousControl=currentSample.previousControl;  //needed later for computing control cost with jerk term
	}

	void  ControlPBP::getControlWithoutStateKernel( int sampleIdx, float *out_control, const float *priorMean, const float *priorStd )
	{
		//link the marginal samples to each other so that full-dimensional samples can be recovered
		//printf("without state kernel \n");

		MarginalSample &nextSample=marginals[nextStep][sampleIdx];
		MarginalSample &currentSample=marginals[currentStep][nextSample.previousMarginalSampleIdx];
		Eigen::Map<VectorXf> control(out_control,nControlDimensions);
		//special processing for initial guesses
		if (sampleIdx<nInitialGuesses && (currentStep<nSteps-1 || !timeAdvanced))  //don't use the old best for the last frame (as it has been "scrolled")
		{
			if (sampleIdx==0)
			{
				//the old best solution
				control=oldBest[nextStep].control;	//nextStep as index because the control is always stored to the next sample ("control that brought me to this state")
				;

			}
			else if (sampleIdx==1)
			{
				
				control=gaussianBackPropagated.block(currentStep*nControlDimensions,0,nControlDimensions,1);
			}
		}
		//processing for samples other than initial guesses
		else
		{
			
			//When no kernels used, we sample from the product of the static control prior, the difference priors given the previous sampled controls,
			//and the "mutation prior" of the sample from previous frame
			 static __thread DiagonalGMM *proposal=NULL;
		 static __thread DiagonalGMM *prior=NULL;
			if (prior==NULL)
			{
				prior=new DiagonalGMM();
				prior->resize(1,nControlDimensions);
				prior->weights[0]=1;	//only need to set once, as the prior will always have just a single component
				prior->weightsUpdated();
				proposal=new DiagonalGMM();
				proposal->resize(1,nControlDimensions);
			}


			//first the difference prior
			prior->mean[0]=currentSample.control;
			prior->std[0]=controlDiffPriorStd;

			//multiply the difference prior and static prior to yield the proposal
			DiagonalGMM::multiply(staticPrior,*prior,*proposal);

			//the same for the second difference

			prior->mean[0]=2.0f*currentSample.control-currentSample.previousControl; //currentSample.control+(currentSample.control-currentSample.previousControl);
			prior->std[0]=controlDiffDiffPriorStd;
			DiagonalGMM::multiply(*prior,*proposal,*proposal);	//ok to have same source and destination if both have only 1 gaussian

			//the optional prior passed as argument
			if (priorMean!=NULL && priorStd!=NULL)
			{
				memcpy(&prior->mean[0][0],priorMean,sizeof(float)*nControlDimensions);
				memcpy(&prior->std[0][0],priorStd,sizeof(float)*nControlDimensions);
				DiagonalGMM::multiply(*prior,*proposal,*proposal);	//ok to have same source and destination if both have only 1 gaussian
			}

			//formulate and multiply in the sample prior if prior sample known, i.e.,
			//(1) this sample is not an uniformly distributed init and
			//(2) we are not at last control step where we can't have a reasonable prior based on previous frame except for the control difference (jerk)
			if (nextSample.priorSampleIdx>=0 && (currentStep<nSteps-1 || !timeAdvanced))
			{
				prior->mean[0]=fullSamples[nextSample.priorSampleIdx].block(currentStep*nControlDimensions,0,nControlDimensions,1);
				prior->std[0]=controlMutationStd;
				DiagonalGMM::multiply(*prior,*proposal,*proposal);
			}

			//sample the new control vector
			
			
			//printf("Proposal \n");
			proposal->sampleWithLimits(control,controlMin,controlMax);
			

		}
		nextSample.control=control;
		nextSample.previousState=marginals[currentStep][nextSample.previousMarginalSampleIdx].state;
		nextSample.previousControl=marginals[currentStep][nextSample.previousMarginalSampleIdx].control;  //needed later for computing control cost with jerk term
		nextSample.previousPreviousControl=marginals[currentStep][nextSample.previousMarginalSampleIdx].previousControl;  //needed later for computing control cost with jerk term
	}

	void ControlPBP::gaussianBackPropagation(TransitionFunction transitionFwd, OnStepDone onStepDone)
	{
		//adapt the regularization coeff
		/*
		if (iterationIdx>0)
		{
			double oldBestCost=marginals[nSteps-1][0].fullCost;
			double oldBPCost=marginals[nSteps-1][1].fullCost;
			//if cost of the old best sample smaller than the previously backpropagated path (measured at next to last frame, as the old best and old backpropagated are valid only up to that when time moves between iterations)
			if (oldBestCost<oldBPCost)
			{
				gbpRegularization=_min(1,gbpRegularization*5.0f);
			}
			else
			{
				gbpRegularization=_max(1e-10f,gbpRegularization*0.2f);
			}
			Debug::printf("Set gbpRegularization to %f\n",gbpRegularization);
		}
		*/
		/*

		In the following, x denotes state and u denotes control

		Here, we assume that each marginal sample gaussian N(x,u,x_next | mu, cov) defines a multidimensional "pipe" segment.
		The density inside the pipe is not uniform but depends on the control and state costs.

		We traverse the pipe backwards by fixing x_next and solving for maximum likelihood x,u. For next backward step, x becomes x_next.

		*/
		VectorXf xu(nStateDimensions+nControlDimensions),x_next(nStateDimensions),x(nStateDimensions);

		//Matrix of state vectors
		MatrixXf X(nStateDimensions,nSamples);
		VectorXf u(nControlDimensions);
		//Vector of data vector weights
		VectorXd w(nSamples);
		VectorXf stateDiff(nStateDimensions);
		//shorthands for dimensions
		int uDim=nControlDimensions;
		int xDim=nStateDimensions;
		int xuDim=xDim+uDim;
		//regularization matrix
		MatrixXf regularization(xDim,xDim);
		regularization.setIdentity();
		regularization*=gbpRegularization;
		MatrixXf xuRegularization(xuDim,xuDim);
		regularization.setIdentity();
		regularization*=gbpRegularization;

		//For initialization, compute the mean of last state samples weighed with the state potentials
		for (int sample=0; sample<nSamples; sample++)
		{
			MarginalSample &s=marginals[nSteps][sample];
			X.col(sample)=s.state;
			w[sample]=s.statePotential;
			if (gaussianAroundBest && sample!=bestFullSampleIdx)
			{
				stateDiff=s.state-marginals[nSteps][bestFullSampleIdx].state;
				w[sample]*=exp(-0.5f*stateDiff.squaredNorm()*gbpRegularization);  //the regularization is interpreted as an additional diagonal gaussian prior centered at the best sample
				if (!validFloat(w[sample]))
					w[sample]=0;
			}
		}
		calcMeanWeighed(X,w,x_next);

		//Now iterate backwards over the steps
		int bestIdx=bestFullSampleIdx;
		// of column data vectors [x u x_next]', and the respective covariance and mean
		MatrixXf XUX(nStateDimensions*2+nControlDimensions,nSamples);
		MatrixXf XUXCov(nStateDimensions*2+nControlDimensions,nStateDimensions*2+nControlDimensions);
		VectorXf XUXMean(nStateDimensions*2+nControlDimensions);
		for (int step=nSteps; step>0; step--)
		{

			//Gather samples and compute XUXCov, XUXMean
			//TODO: assume that the linearization valid over several steps (and iterations), aggregate samples. Number of samples should be larger than nStateDimensions + nControlDimensions.
			//This should be possible at least in an offline case where samples should probably be weighed using a kernel to linearize close to the current expected backpropagated state
			for (int sample=0; sample<nSamples; sample++)
			{
				MarginalSample &s=marginals[step][sample];
				XUX.block(0,sample,nStateDimensions,1)=s.previousState;	//x
				XUX.block(nStateDimensions,sample,nControlDimensions,1)=s.control;  //u
				XUX.block(nStateDimensions+nControlDimensions,sample,nStateDimensions,1)=s.state;  //x_next

				//Weight the samples using the previous state and control costs, neglecting next state potential, as the next state is already solved.
				MarginalSample &previous=marginals[step-1][s.previousMarginalSampleIdx];
//				w[sample]=1.0;
//				w[sample]=exp(-0.5*(previous.stateCost+s.controlCost));
				w[sample]=previous.forwardBelief;  //if resampled, forwardBelief=statePotential, otherwise carries info from previous samples. Control potential doesn't need to be included, as it is already included in the proposal that generated the samples

				//if (gaussianAroundBest && sample!=bestIdx)
				//{
				//	stateDiff=s.previousState-marginals[step][bestIdx].previousState;
				//	w[sample]*=exp(-0.5f*stateDiff.squaredNorm()*gbpRegularization);  //the regularization is interpreted as an additional diagonal gaussian prior centered at the best sample
				//	if (!validFloat(w[sample]))
				//		w[sample]=0;
				//}
			}
			if (gaussianAroundBest)
			{
				XUXMean.block(0,0,nStateDimensions,1)=marginals[step][bestIdx].previousState;
				XUXMean.block(nStateDimensions,0,nControlDimensions,1)=marginals[step][bestIdx].control;
				XUXMean.block(nStateDimensions+nControlDimensions,0,nStateDimensions,1)=marginals[step][bestIdx].state;
				calcCovarWeighed(XUX,w,XUXCov,XUXMean);
			}
			else
			{
				static Eigen::MatrixXf temp;
				calcMeanAndCovarWeighedVectorized(XUX,w,XUXCov,XUXMean, temp);
		//		calcMeanAndCovarWeighed(XUX,w,XUXCov,XUXMean);
			}

			//Compute regression so that E[x,u | x_next]=mu_xu + R(x_next - mu_x_next). For reference, see "multivariate normal distribution" on Wikipedia
			MatrixXf R=XUXCov.block(0,xuDim,xuDim,xDim) //covariance of x,u with x_next. x_next is the lower right corner of the full covariance
				* (XUXCov.block(xuDim,xuDim,xDim,xDim)+regularization).inverse();  //inverse of the autocovariance of x_next
			VectorXf mu_xu=XUXMean.block(0,0,xuDim,1);
			VectorXf mu_x_next=XUXMean.block(xuDim,0,xDim,1);

			//update new x_next
			xu=mu_xu + R*(x_next - mu_x_next);
			u=xu.block(nStateDimensions,0,nControlDimensions,1);


			//clamp to range.
			bool boundsViolated=false;
			for (int i=0; i<nControlDimensions; i++)
			{
				if (u[i]<controlMin[i])
				{
					boundsViolated=true;
					u[i]=controlMin[i];
				}
				else if (u[i]>controlMax[i])
				{
					boundsViolated=true;
					u[i]=controlMax[i];
				}
			}
			//If the regression result for the control vector did not have to be clamped, accept the regression result for state as the new x_next.
			if (!boundsViolated)
			{
				x=xu.block(0,0,nStateDimensions,1);
			}
			else
			{
				//Control had to be clamped => compute a new regression for x as a function of both x_next and u, the latter fixed to the clamped value
				MatrixXf R=XUXCov.block(0,xDim,xDim,xuDim) //covariance of x with u,x_next.
					* (XUXCov.block(xDim,xDim,xuDim,xuDim)+xuRegularization).inverse();  //inverse of the autocovariance of x_next
				VectorXf mu_x=XUXMean.block(0,0,xDim,1);
				VectorXf mu_ux_next=XUXMean.block(xDim,0,xuDim,1);
				VectorXf ux_next(nControlDimensions+nStateDimensions);
				ux_next.block(0,0,nControlDimensions,1)=u;
				ux_next.block(nControlDimensions,0,nStateDimensions,1)=x_next;
				x=mu_x + R*(ux_next - mu_ux_next);
			}
			//check that the floats are valid
			for (int i=0; i<nControlDimensions; i++)
			{
				if (!validFloat(u[i]))
          					u[i]=controlMean[i];
			}
			gaussianBackPropagated.block((step-1)*nControlDimensions,0,nControlDimensions,1)=u;
			//TODO: handle control difference minimization (set a prior for controls)
			//Use the callbacks - may be used by the client for, e.g., debug visualization
			if (transitionFwd!=NULL)
			{
				float temp[2];
				//note that x_next is now the x_next of next backpropagation step, i.e., x of this step
				transitionFwd(step-1,x.data(),u.data(),temp);
				if (onStepDone)
					onStepDone();
			}
			x_next=x;
			bestIdx=marginals[step][bestIdx].previousMarginalSampleIdx;
		}
	}


	//void ControlPBP::backpropagation()
	//{
	//	/*

	//	In the following, we write the backwards conditional probability as the product of the state potential N(x | xMean,xCov)
	//	and the integral of control prior N(u | 0,uCov) and the next probability N(x_next | xMean_next, xCov_next):

	//	p(x | x_next) propto N(x | xMean, xCov) * integrate_over_u{ N(u | 0,uCov) * N( x_next | xMean_next, xCov_next)  }

	//	Substituting the linearization x_next = Ax + a + Bu, we obtain

	//	p(x | x_next) propto N(x | xMean, xCov) * integrate_over_u{ N(u | 0,uCov) * N( Ax + a + Bu | xMean_next, xCov_next)}


	//	Toussaint's (2009) AICO paper gives

	//	integrate_over_u {N(x_t+1 | Ax_t + a + Bu_t,Q) N [u_t | 0,H]}
	//	= N(x_t+1 | Ax_t + a, Q + B inv(H) B')

	//	The details for the integration are not given, but probably it was using the Gaussian integral formula
	//	int(exp(-1/2(u'Du+Fu)),u) propto exp(1/2*F*inv(D)*F')  (http://en.wikipedia.org/wiki/Gaussian_integral, http://www.weylmann.com/gaussian.pdf)


	//	The bracket notation defines a canonical form Gaussian, i.e., N(u_t |0,H) = N[u_t | 0,inv(H)]

	//	Further, N(x | mu,cov)=N(mu | x, cov).

	//	=>

	//	p(x | x_next) propto N(x | xMean, xCov) * N( Ax + a | xMean_next, xCov_next + B * uCov * B')

	//	To get both normal distributions to have x as the variable, we apply the multivariate Gaussian affine transform properties

	//	E[Ax+a]=A*E[x]+a=xMean_next => E[x]=inv(A)*(xMean_next - a)

	//	cov(Ax+a) = A*cov(x)*A' = xCov_next + B * uCov * B' => cov(x)=inv(A)*(xCov_next + B * uCov * B')*inv(A')

	//	=>

	//	p(x | x_next) propto N(x | xMean, xCov) * N( x | inv(A)*(xMean_next-a), inv(A)*(xCov_next + B * uCov * B')*inv(A'))
	//	                   =  N(x | xMean, xCov) * N(x | xMean_integrated, xCov_integrated)

	//	Now we can simply multiply the gaussians together to get xMean_next and xCov_next for the next iteration.

	//	*/
	//	VectorXf x(nStateDimensions),xMean(nStateDimensions),a(nStateDimensions),xMean_next(nStateDimensions);
	//	MatrixXf A(nStateDimensions,nStateDimensions);
	//	MatrixXf B(nStateDimensions,nControlDimensions);
	//	VectorXf x_next(nStateDimensions),xMean_next(nStateDimensions);
	//	MatrixXf xCov(nStateDimensions,nStateDimensions);
	//	MatrixXf xCov_next(nStateDimensions,nStateDimensions);

	//	//Matrix of state vectors
	//	MatrixXf X(nStateDimensions,nSamples);
	//	//Vector of data vector weights
	//	VectorXd w(nSamples);

	//	//For initialization, gather the marginal state samples and weights (potentials) for the last step, and compute an Gaussian approximation of the corresponding marginal density
	//	for (int sample=0; sample<nSamples; sample++)
	//	{
	//		MarginalSample &s=marginals[nSteps][sample];
	//		X.col(sample)=s.state;
	//		w[sample]=s.statePotential;
	//	}
	//	calcMeanAndCovarWeighed(X,w,xCov_next,xMean_next);



	//	//Now iterate backwards over the steps
	//	for (int step=nSteps-1; step>=0; step--)
	//	{
	//		//Regularized linearization using the samples
	//		//matrix of column data vectors [x u x_next]', and the respective covariance and mean
	//		MatrixXf XUX(nStateDimensions*2+nControlDimensions,nSamples);
	//		MatrixXf XUXCov(nStateDimensions*2+nControlDimensions,nStateDimensions*2+nControlDimensions);
	//		VectorXf XUXMean(nStateDimensions*2+nControlDimensions);

	//		//Gather samples and compute XUXCov, XUXMean
	//		//TODO: assume that the linearization valid over several steps (and iterations), aggregate samples. Number of samples should be larger than nStateDimensions + nControlDimensions.
	//		//This should be possible at least in an offline case where samples should probably be weighed using a kernel to linearize close to the current expected backpropagated state
	//		for (int sample=0; sample<nSamples; sample++)
	//		{
	//			MarginalSample &s=marginals[step+1][sample];
	//			XUX.block(0,sample,nStateDimensions,1)=s.state;	//x_next comprises the first part, needed for the regression formulation below
	//			XUX.block(nStateDimensions,sample,nControlDimensions,1)=s.control;  //u
	//			XUX.block(nStateDimensions+nControlDimensions,sample,nStateDimensions,1)=s.previousState;  //x
	//			w[sample]=s.statePotential;
	//		}
	//		calcMeanAndCovarWeighed(XUX,w,XUXCov,XUXMean);

	//		//Compute regression matrix R
	//		int xuDim=nStateDimensions+nControlDimensions;     //we want a regression where we know x,u
	//		int xDim=nStateDimensions;						//and compute x_next
	//		int uDim=nControlDimensions;
	//		MatrixXf regularization(xuDim,xuDim);
	//		regularization.setIdentity();
	//		regularization*=0.0001f;
	//		MatrixXf R=XUXCov.block(0,xDim,xDim,xuDim) //covariance of x_next with x,u
	//			* (XUXCov.block(xDim,xDim,xuDim,xuDim)+regularization).inverse();  //inverse autocovariance of x_next

	//		//extract the A,a, B for the x_next = Ax + a + Bu, considering that x_next=R([x u]' - [xMean uMean]') = [R1 R2][x u]' - R[xMean uMean]'
	//		A=R.block(0,0,xDim,xDim);
	//		B=R.block(0,xDim,xDim,uDim);
	//		a=-R*XUXMean.block(xDim,0,xuDim,1);

	//		//now, update the conditional p(x | x_next) propto N(x | xMean, xCov) * N( x | inv(A)*(xMean_next-a), inv(A)*(xCov_next + B * uCov * B')*inv(A'))

	//		//compute xMean,xCov
	//		for (int sample=0; sample<nSamples; sample++)
	//		{
	//			MarginalSample &s=marginals[step][sample];
	//			X.col(sample)=s.state;
	//			w[sample]=s.statePotential;
	//		}
	//		calcMeanAndCovarWeighed(X,w,xCov,xMean);

	//		//compute xMean_integrated, xCov_integrated

	//		TODO

	//		//multiply the gaussians to get xMean_next and xCov_next for the next iteration

	//		TODO

	//		AND NOW WHAT? Just pick the sequence of expected states and solve controls from the linearization?
	//	}
	//}


} //AaltoGames;




