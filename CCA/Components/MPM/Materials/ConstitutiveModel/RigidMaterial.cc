/*
 * The MIT License
 *
 * Copyright (c) 1997-2020 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "RigidMaterial.h"
#include <CCA/Components/MPM/Materials/MPMMaterial.h>
#include <Core/Grid/Patch.h>
#include <CCA/Ports/DataWarehouse.h>
#include <Core/Grid/Task.h>
#include <CCA/Components/MPM/Core/MPMLabel.h>
#include <Core/Grid/Variables/VarTypes.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <iostream>

using std::cerr;
using namespace Uintah;

RigidMaterial::RigidMaterial(ProblemSpecP& ps, MPMFlags* Mflag) : 
  ConstitutiveModel(Mflag), ImplicitCM()
{
  d_initialData.G = 1.0e200;
  ps->get("shear_modulus",d_initialData.G);
  d_initialData.K = 1.0e200;
  ps->get("bulk_modulus",d_initialData.K);
}

RigidMaterial::~RigidMaterial()
{
}

void RigidMaterial::outputProblemSpec(ProblemSpecP& ps,bool output_cm_tag)
{
  ProblemSpecP cm_ps = ps;
  if (output_cm_tag) {
    cm_ps = ps->appendChild("constitutive_model");
    cm_ps->setAttribute("type","rigid");
  }

  cm_ps->appendElement("shear_modulus",d_initialData.G);
  cm_ps->appendElement("bulk_modulus",d_initialData.K);
}



RigidMaterial* RigidMaterial::clone()
{
  return scinew RigidMaterial(*this);
}


void 
RigidMaterial::initializeCMData(const Patch* patch,
                                const MPMMaterial* matl,
                                DataWarehouse* new_dw)
{
  // Initialize the variables shared by all constitutive models
  // This method is defined in the ConstitutiveModel base class.
  if (flag->d_integrator == MPMFlags::Implicit) 
    initSharedDataForImplicit(patch, matl, new_dw);
  else {
    initSharedDataForExplicit(patch, matl, new_dw);
    new_dw->put(delt_vartype(1.0e10), lb->delTLabel, patch->getLevel());
  }
}

void 
RigidMaterial::addComputesAndRequires(Task* task,
                                      const MPMMaterial* matl,
                                      const PatchSet* patches) const
{
  // Add the computes and requires that are common to all explicit 
  // constitutive models.  The method is defined in the ConstitutiveModel
  // base class.
  const MaterialSubset* matlset = matl->thisMaterial();
  if (flag->d_integrator == MPMFlags::Implicit) {
    addSharedCRForImplicit(task, matlset, patches);
  } else {
    addSharedCRForExplicit(task, matlset, patches);
  }
}

void 
RigidMaterial::computeStressTensor(const PatchSubset* patches,
                                   const MPMMaterial* matl,
                                   DataWarehouse* old_dw,
                                   DataWarehouse* new_dw)
{
  carryForward(patches, matl, old_dw, new_dw);
}

//JIAHAO: one way to escape injury computation
void RigidMaterial::computeInjury(const PatchSubset* patches,
                                const MPMMaterial* matl,
                                DataWarehouse* old_dw,
                                DataWarehouse* new_dw){
  // JIAHAO: I tried to use carryForward method. It is beyond my capability now. 
  //carryForward(patches, matl, old_dw, new_dw);
  Ghost::GhostType  gan = Ghost::AroundNodes;

  // Normal patch loop
  for(int pp=0;pp<patches->size();pp++){
    //JIAHAO: debug statements
    //std::cout<<"the current patch pp is: "<<pp<<std::endl;
    //std::cout<<"number of pathces is: "<<patches->size()<<std::endl;
    const Patch* patch = patches->get(pp);

    // Get particle info and patch info
    int dwi              = matl->getDWIndex();
//    ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);
    ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch,
                                                     gan, 0, lb->pXLabel);
    Vector dx            = patch->dCell();

    // Particle and grid data universal to model type
    // Old data containers
    constParticleVariable<double>  pInjury;
    Matrix3 Identity; Identity.Identity();

    // New data containers
    ParticleVariable<double>       pInjury_new;
    

    // Universal Gets
    old_dw->get(pInjury,             lb->pInjuryLabel             ,pset);
    
    // JIAHAO: injury Allocations
    new_dw->allocateAndPut(pInjury_new, lb->pInjuryLabel_preReloc, pset);

    ParticleSubset::iterator iter = pset->begin();
    for(; iter != pset->end(); iter++){
      particleIndex idx = *iter;
      pInjury_new[idx]=0.;   
  }
}
}

void 
RigidMaterial::computeStressTensorImplicit(const PatchSubset* patches,
                                           const MPMMaterial* matl,
                                           DataWarehouse* old_dw,
                                           DataWarehouse* new_dw)
{
  int dwi = matl->getDWIndex();
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

    // Carry forward the data common to all constitutive models 
    // when using RigidMPM.
    // This method is defined in the ConstitutiveModel base class.
    carryForwardSharedDataImplicit(pset, old_dw, new_dw, matl);
//    new_dw->put(sum_vartype(0.),     lb->StrainEnergyLabel);
  }
}

void 
RigidMaterial::carryForward(const PatchSubset* patches,
                            const MPMMaterial* matl,
                            DataWarehouse* old_dw,
                            DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    int dwi = matl->getDWIndex();
    ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

    // Carry forward the data common to all constitutive models 
    // when using RigidMPM.
    // This method is defined in the ConstitutiveModel base class.
    carryForwardSharedData(pset, old_dw, new_dw, matl);
    new_dw->put(delt_vartype(1.0), lb->delTLabel, patch->getLevel());
    
    if (flag->d_reductionVars->accStrainEnergy ||
        flag->d_reductionVars->strainEnergy) {
      new_dw->put(sum_vartype(0.),  lb->StrainEnergyLabel);
    }
  }
}

void
RigidMaterial::addComputesAndRequires(Task* task,
                                      const MPMMaterial* matl,
                                      const PatchSet* patches,
                                      const bool recurse,
                                      const bool SchedParent) const
{
  const MaterialSubset* matlset = matl->thisMaterial();
  addSharedCRForImplicit(task, matlset, patches, recurse, SchedParent);
}

void 
RigidMaterial::computeStressTensorImplicit(const PatchSubset* patches,
                                           const MPMMaterial* matl,
                                           DataWarehouse* ,
                                           DataWarehouse* new_dw,
                                           Solver* ,
                                           const bool )

{
  int dwi = matl->getDWIndex();
  DataWarehouse* parent_dw = new_dw->getOtherDataWarehouse(Task::ParentOldDW);
  for (int pp = 0; pp < patches->size(); pp++) {
    const Patch* patch = patches->get(pp);
    ParticleSubset* pset = parent_dw->getParticleSubset(dwi, patch);
    carryForwardSharedDataImplicit(pset, parent_dw, new_dw, matl);
  }
}

void 
RigidMaterial::addParticleState(std::vector<const VarLabel*>& ,
                                std::vector<const VarLabel*>& )
{
}

double 
RigidMaterial::computeRhoMicroCM(double ,
                                 const double ,
                                 const MPMMaterial* matl,
                                 double temperature,
                                 double rho_guess)
{
  return matl->getInitialDensity();
}

void 
RigidMaterial::computePressEOSCM(double , double& pressure,
                                 double p_ref,
                                 double& dp_drho, double& tmp,
                                 const MPMMaterial* matl,
                                 double temperature)
{
  double K = d_initialData.K;
  double rho_0 = matl->getInitialDensity();
  pressure = p_ref;
  dp_drho  = K/rho_0;
  tmp = dp_drho;  // speed of sound squared
}

double RigidMaterial::getCompressibility()
{
  return (1.0/d_initialData.K);
}

