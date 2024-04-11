/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
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

#include <CCA/Components/MPM/Materials/ConstitutiveModel/HGOC.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Grid/Patch.h>
#include <CCA/Ports/DataWarehouse.h>
#include <Core/Grid/Variables/NCVariable.h>
#include <Core/Grid/Variables/ParticleVariable.h>
#include <Core/Grid/Task.h>
#include <Core/Grid/Level.h>
#include <Core/Grid/Variables/VarLabel.h>
#include <Core/Grid/Variables/VarTypes.h>
#include <CCA/Components/MPM/Core/MPMLabel.h>
#include <Core/Math/Matrix3.h>
#include <Core/Math/TangentModulusTensor.h> //added this for stiffness
#include <CCA/Components/MPM/Materials/MPMMaterial.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/Math/MinMax.h>
#include <Core/Malloc/Allocator.h>
#include <fstream>
#include <iostream>

using namespace std;
using namespace Uintah;

// _________________HGOC
// Shailesh (SG) Compile try - Built for UINTAH-1.5.0
// Ahmed (aaa) modified for UINTAH-2.1.0
// Compare with SG HGOC file to find changes aaa made

HGOC::HGOC(ProblemSpecP& ps,MPMFlags* Mflag)
  : ConstitutiveModel(Mflag)

{
  d_useModifiedEOS = false;
  //______________________material properties
  ps->require("bulk_modulus", d_initialData.Bulk);
  ps->require("c1", d_initialData.c1);//Mooney Rivlin constant 1
  ps->require("c2", d_initialData.c2);//Mooney Rivlin constant 2
  ps->require("k1", d_initialData.k1);//accounts for the additional stiffness provided by the fibers
  ps->require("k2", d_initialData.k2);//controls nonlinearity of anisotropic response
  //ps->require("fiber_stretch", d_initialData.lambda_star);//toe region limit
  ps->require("direction_of_symm", d_initialData.a0);//fiber direction(initial)
  ps->require("FA", d_initialData.fa0); //FA(initial)
  //ps->require("failure_option",d_initialData.failure);//failure flag True/False
  //ps->require("max_fiber_strain",d_initialData.crit_stretch);//failure limit fibers
  //ps->require("max_matrix_strain",d_initialData.crit_shear);//failure limit matrix
  //ps->get("useModifiedEOS",d_useModifiedEOS);//no negative pressure for solids
  ps->require("y1", d_initialData.y1);//viscoelastic prop's
  ps->require("y2", d_initialData.y2);
  ps->require("y3", d_initialData.y3);
  ps->require("y4", d_initialData.y4);
  ps->require("y5", d_initialData.y5);
  ps->require("y6", d_initialData.y6);
  ps->require("t1", d_initialData.t1);//relaxation times
  ps->require("t2", d_initialData.t2);
  ps->require("t3", d_initialData.t3);
  ps->require("t4", d_initialData.t4);
  ps->require("t5", d_initialData.t5);
  ps->require("t6", d_initialData.t6);

  pStretchLabel = VarLabel::create("p.stretch",
     ParticleVariable<double>::getTypeDescription());
  pStretchLabel_preReloc = VarLabel::create("p.stretch+",
     ParticleVariable<double>::getTypeDescription());

 // pFailureLabel = VarLabel::create("p.fail",
    // ParticleVariable<double>::getTypeDescription());
 // pFailureLabel_preReloc = VarLabel::create("p.fail+",
    // ParticleVariable<double>::getTypeDescription());
     
  pElasticStressLabel = VarLabel::create("p.ElasticStress",
        ParticleVariable<Matrix3>::getTypeDescription());
  pElasticStressLabel_preReloc = VarLabel::create("p.ElasticStress+",
        ParticleVariable<Matrix3>::getTypeDescription());

  pHistory1Label = VarLabel::create("p.history1",
        ParticleVariable<Matrix3>::getTypeDescription());
  pHistory1Label_preReloc = VarLabel::create("p.history1+",
        ParticleVariable<Matrix3>::getTypeDescription());

  pHistory2Label = VarLabel::create("p.history2",
        ParticleVariable<Matrix3>::getTypeDescription());
  pHistory2Label_preReloc = VarLabel::create("p.history2+",
        ParticleVariable<Matrix3>::getTypeDescription());

  pHistory3Label = VarLabel::create("p.history3",
        ParticleVariable<Matrix3>::getTypeDescription());
  pHistory3Label_preReloc = VarLabel::create("p.history3+",
        ParticleVariable<Matrix3>::getTypeDescription());

  pHistory4Label = VarLabel::create("p.history4",
        ParticleVariable<Matrix3>::getTypeDescription());
  pHistory4Label_preReloc = VarLabel::create("p.history4+",
        ParticleVariable<Matrix3>::getTypeDescription());

  pHistory5Label = VarLabel::create("p.history5",
        ParticleVariable<Matrix3>::getTypeDescription());
  pHistory5Label_preReloc = VarLabel::create("p.history5+",
        ParticleVariable<Matrix3>::getTypeDescription());

  pHistory6Label = VarLabel::create("p.history6",
        ParticleVariable<Matrix3>::getTypeDescription());
  pHistory6Label_preReloc = VarLabel::create("p.history6+",
        ParticleVariable<Matrix3>::getTypeDescription());
}

#if 0 //aaa
HGOC::HGOC(const HGOC* cm)
  : ConstitutiveModel(cm)
{
  d_useModifiedEOS = cm->d_useModifiedEOS;

  d_initialData.Bulk = cm->d_initialData.Bulk;
  d_initialData.c1 = cm->d_initialData.c1;
  d_initialData.c2 = cm->d_initialData.c2;
  d_initialData.k1 = cm->d_initialData.k1;
  d_initialData.k2 = cm->d_initialData.k2;
  //d_initialData.lambda_star = cm->d_initialData.lambda_star;
  d_initialData.a0 = cm->d_initialData.a0;
  d_initialData.fa0 = cm->d_initialData.fa0;
  //d_initialData.failure = cm->d_initialData.failure;
  //d_initialData.crit_stretch = cm->d_initialData.crit_stretch;
  //d_initialData.crit_shear = cm->d_initialData.crit_shear;
  
  d_initialData.y1 = cm->d_initialData.y1;//visco parameters
  d_initialData.y2 = cm->d_initialData.y2;
  d_initialData.y3 = cm->d_initialData.y3;
  d_initialData.y4 = cm->d_initialData.y4;
  d_initialData.y5 = cm->d_initialData.y5;
  d_initialData.y6 = cm->d_initialData.y6;
  d_initialData.t1 = cm->d_initialData.t1;
  d_initialData.t2 = cm->d_initialData.t2;
  d_initialData.t3 = cm->d_initialData.t3;
  d_initialData.t4 = cm->d_initialData.t4;
  d_initialData.t5 = cm->d_initialData.t5;
  d_initialData.t6 = cm->d_initialData.t6;
}
#endif //aaa

HGOC::~HGOC()
  // _______________________DESTRUCTOR
{
  VarLabel::destroy(pStretchLabel);
  VarLabel::destroy(pStretchLabel_preReloc);
  //VarLabel::destroy(pFailureLabel);
  //VarLabel::destroy(pFailureLabel_preReloc);
  VarLabel::destroy(pElasticStressLabel);
  VarLabel::destroy(pElasticStressLabel_preReloc);//visco labels
  VarLabel::destroy(pHistory1Label);
  VarLabel::destroy(pHistory1Label_preReloc);
  VarLabel::destroy(pHistory2Label);
  VarLabel::destroy(pHistory2Label_preReloc);
  VarLabel::destroy(pHistory3Label);
  VarLabel::destroy(pHistory3Label_preReloc);
  VarLabel::destroy(pHistory4Label);
  VarLabel::destroy(pHistory4Label_preReloc);
  VarLabel::destroy(pHistory5Label);
  VarLabel::destroy(pHistory5Label_preReloc);
  VarLabel::destroy(pHistory6Label);
  VarLabel::destroy(pHistory6Label_preReloc);
}


void HGOC::outputProblemSpec(ProblemSpecP& ps,bool output_cm_tag)
{
  ProblemSpecP cm_ps = ps;
  if (output_cm_tag) {
    cm_ps = ps->appendChild("constitutive_model");
    cm_ps->setAttribute("type","HGOC");
  }

  cm_ps->appendElement("bulk_modulus", d_initialData.Bulk);
  cm_ps->appendElement("c1", d_initialData.c1);
  cm_ps->appendElement("c2", d_initialData.c2);
  cm_ps->appendElement("k1", d_initialData.k1);
  cm_ps->appendElement("k2", d_initialData.k2);
  // cm_ps->appendElement("fiber_stretch", d_initialData.lambda_star);
  cm_ps->appendElement("direction_of_symm", d_initialData.a0);
  cm_ps->appendElement("FA", d_initialData.fa0);
  //cm_ps->appendElement("failure_option",d_initialData.failure);
  //cm_ps->appendElement("max_fiber_strain",d_initialData.crit_stretch);
  //cm_ps->appendElement("max_matrix_strain",d_initialData.crit_shear);
  //cm_ps->appendElement("useModifiedEOS",d_useModifiedEOS);
  cm_ps->appendElement("y1", d_initialData.y1);
  cm_ps->appendElement("y2", d_initialData.y2);
  cm_ps->appendElement("y3", d_initialData.y3);
  cm_ps->appendElement("y4", d_initialData.y4);
  cm_ps->appendElement("y5", d_initialData.y5);
  cm_ps->appendElement("y6", d_initialData.y6);
  cm_ps->appendElement("t1", d_initialData.t1);
  cm_ps->appendElement("t2", d_initialData.t2);
  cm_ps->appendElement("t3", d_initialData.t3);
  cm_ps->appendElement("t4", d_initialData.t4);
  cm_ps->appendElement("t5", d_initialData.t5);
  cm_ps->appendElement("t6", d_initialData.t6);
}


HGOC* HGOC::clone()
{
  return scinew HGOC(*this);
}

void HGOC::initializeCMData(const Patch* patch,
                            const MPMMaterial* matl,
                            DataWarehouse* new_dw)
// _____________________STRESS FREE REFERENCE CONFIG
{
  // Initialize the variables shared by all constitutive models
  // This method is defined in the ConstitutiveModel base class.
  ParticleSubset* pset = new_dw->getParticleSubset(matl->getDWIndex(), patch);
  initSharedDataForExplicit(patch, matl, new_dw);
  
  computeStableTimestep(patch, matl, new_dw);

  // Put stuff in here to initialize each particle's
  // constitutive model parameters and deformationMeasure
  Matrix3 Identity, zero(0.);
  Identity.Identity();

  //JIAHAO
  ParticleVariable<double> pInjury;
  new_dw->getModifiable(pInjury,     lb->pInjuryLabel,             pset);
  // JIAHAO
  double injuryInitial(0.0);
  
  int dwi = matl->getDWIndex(); // aaa based on HypoElastic.cc
  //ParticleSubset* pset = new_dw->getParticleSubset(matl->getDWInd ex(), patch); //old
  //ParticleSubset* pset = new_dw->getParticleSubset(dwi, patch); //aaa

  ParticleVariable<double> stretch;
  //ParticleVariable<double> stretch,fail;
  ParticleVariable<Matrix3> ElasticStress;
  ParticleVariable<Matrix3> history1,history2,history3;
  ParticleVariable<Matrix3> history4,history5,history6;

  new_dw->allocateAndPut(stretch,pStretchLabel,   pset);
 // new_dw->allocateAndPut(fail,   pFailureLabel,   pset);

  new_dw->allocateAndPut(ElasticStress,pElasticStressLabel,pset);
  new_dw->allocateAndPut(history1,     pHistory1Label,     pset);
  new_dw->allocateAndPut(history2,     pHistory2Label,     pset);
  new_dw->allocateAndPut(history3,     pHistory3Label,     pset);
  new_dw->allocateAndPut(history4,     pHistory4Label,     pset);
  new_dw->allocateAndPut(history5,     pHistory5Label,     pset);
  new_dw->allocateAndPut(history6,     pHistory6Label,     pset);

  ParticleSubset::iterator iter = pset->begin();
  for(;iter != pset->end(); iter++){
    //fail[*iter] = 0.0 ;
    stretch[*iter] = 1.0;
    ElasticStress[*iter] = zero;// no pre-initial stress
    history1[*iter] = zero;// no initial 'relaxation'
    history2[*iter] = zero;
    history3[*iter] = zero;
    history4[*iter] = zero;
    history5[*iter] = zero;
    history6[*iter] = zero;
    pInjury[*iter]  = injuryInitial;
  }
}

void HGOC::addParticleState(std::vector<const VarLabel*>& from,
                                     std::vector<const VarLabel*>& to)
//____________________KEEPS TRACK OF THE PARTICLES AND THE RELATED VARIABLES
//____________________(EACH CM ADD ITS OWN STATE VARS)
//____________________AS PARTICLES MOVE FROM PATCH TO PATCH
{
  // Add the local particle state data for this constitutive model.
  from.push_back(lb->pFiberDirLabel);
  from.push_back(lb->pFALabel); //sg
  from.push_back(pStretchLabel);
  //from.push_back(pFailureLabel);
  from.push_back(pElasticStressLabel);//visco_labels
  from.push_back(pHistory1Label);
  from.push_back(pHistory2Label);
  from.push_back(pHistory3Label);
  from.push_back(pHistory4Label);
  from.push_back(pHistory5Label);
  from.push_back(pHistory6Label);

  to.push_back(lb->pFiberDirLabel_preReloc);
  to.push_back(lb->pFALabel_preReloc); //sg
  to.push_back(pStretchLabel_preReloc);
 // to.push_back(pFailureLabel_preReloc);
  to.push_back(pElasticStressLabel_preReloc);
  to.push_back(pHistory1Label_preReloc);
  to.push_back(pHistory2Label_preReloc);
  to.push_back(pHistory3Label_preReloc);
  to.push_back(pHistory4Label_preReloc);
  to.push_back(pHistory5Label_preReloc);
  to.push_back(pHistory6Label_preReloc);
}

void HGOC::computeStableTimestep(const Patch* patch,
                                          const MPMMaterial* matl,
                                          DataWarehouse* new_dw)
//______TIME STEP DEPENDS ON:
//______CELL SPACING, VEL OF PARTICLE, MATERIAL WAVE SPEED @ EACH PARTICLE
//______REDUCTION OVER ALL dT'S FROM EVERY PATCH PERFORMED
//______(USE THE SMALLEST dT)
{
  // This is only called for the initial timestep - all other timesteps
  // are computed as a side-effect of computeStressTensor
  Vector dx = patch->dCell();
  int dwi = matl->getDWIndex();
  // Retrieve the array of constitutive parameters
  ParticleSubset* pset = new_dw->getParticleSubset(dwi, patch);
  constParticleVariable<double> pmass, pvolume;
  constParticleVariable<Vector> pvelocity;

  new_dw->get(pmass,     lb->pMassLabel, pset);
  new_dw->get(pvolume,   lb->pVolumeLabel, pset);
  new_dw->get(pvelocity, lb->pVelocityLabel, pset);

  double c_dil = 0.0;
  Vector WaveSpeed(1.e-12,1.e-12,1.e-12);

  // __Compute wave speed at each particle, store the maximum

  double Bulk = d_initialData.Bulk;
  double c1 = d_initialData.c1;

  for(ParticleSubset::iterator iter = pset->begin();iter != pset->end();iter++){
    particleIndex idx = *iter;

    // this is valid only for F=Identity
    c_dil = sqrt((Bulk+8./3.*c1)*pvolume[idx]/pmass[idx]);
   // c_dil = sqrt((Bulk+2./3.*c1)*pvolume[idx]/pmass[idx]); // wrong formula??

    WaveSpeed=Vector(Max(c_dil+fabs(pvelocity[idx].x()),WaveSpeed.x()),
                     Max(c_dil+fabs(pvelocity[idx].y()),WaveSpeed.y()),
                     Max(c_dil+fabs(pvelocity[idx].z()),WaveSpeed.z()));
  }
  WaveSpeed = dx/WaveSpeed;
  double delT_new = WaveSpeed.minComponent();
  new_dw->put(delt_vartype(delT_new), lb->delTLabel, patch->getLevel());
}

Vector HGOC::getInitialFiberDir()
{
  return d_initialData.a0;
}
double HGOC::getInitialFA()
{
  return d_initialData.fa0;
}
//-------------------------------------------------------------------------
void HGOC::computeStressTensor(const PatchSubset* patches,
                                        const MPMMaterial* matl,
                                        DataWarehouse* old_dw,
                                        DataWarehouse* new_dw)
//COMPUTES THE STRESS ON ALL THE PARTICLES IN A GIVEN PATCH FOR A GIVEN MATERIAL
//CALLED ONCE PER TIME STEP CONTAINS A COPY OF computeStableTimestep
{
  for(int pp=0;pp<patches->size();pp++){
    const Patch* patch = patches->get(pp);

    double J,p;
    Matrix3 Identity; Identity.Identity();
    Matrix3 rightCauchyGreentilde_new, leftCauchyGreentilde_new;
    Matrix3 pressure, deviatoric_stress, fiber_stress;
    double I1tilde,I2tilde,I4tilde,lambda_tilde;
    double exponent;
    double WI1tilde,WI2tilde,WI4tilde;
    double dWdI1tilde,d2WdI1tilde2;
    double dWdI2tilde,d2WdI2tilde2;
    double dWdI4tilde,d2WdI4tilde2;
    double shear;
    Vector deformed_fiber_vector;
    double U,W,se=0.;
    double c_dil=0.0;
    Vector WaveSpeed(1.e-12,1.e-12,1.e-12);
    double kapa;

    Vector dx = patch->dCell();

    int dwi = matl->getDWIndex();
    ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);
    constParticleVariable<Matrix3> deformationGradient_new;
    constParticleVariable<Matrix3> deformationGradient;
    //constParticleVariable<double> pmass,fail_old,pvolume;
    constParticleVariable<double> pmass,pvolume;
    //ParticleVariable<double> stretch,fail;
    ParticleVariable<double> stretch;
    constParticleVariable<Vector> pvelocity, pfiberdir;
    ParticleVariable<Vector> pfiberdir_carry;
    constParticleVariable<double> pfa;            //sg
    ParticleVariable<double> pfa_carry;          //sg
    
    ParticleVariable<Matrix3> pstress,ElasticStress;//visco
    ParticleVariable<Matrix3> history1,history2,history3;
    ParticleVariable<Matrix3> history4,history5,history6;
    ParticleVariable<double> pdTdt,p_q;
    constParticleVariable<Matrix3> history1_old,history2_old,history3_old;
    constParticleVariable<Matrix3> history4_old,history5_old,history6_old;
    constParticleVariable<Matrix3> ElasticStress_old;
    constParticleVariable<Matrix3> velGrad;

    delt_vartype delT;
    old_dw->get(delT, lb->delTLabel, getLevel(patches));
    
    old_dw->get(pmass,               lb->pMassLabel,               pset);
    old_dw->get(pvelocity,           lb->pVelocityLabel,           pset);
    old_dw->get(pfiberdir,           lb->pFiberDirLabel,           pset);
    old_dw->get(pfa,           lb->pFALabel,           pset);              //sg
    old_dw->get(deformationGradient, lb->pDeformationMeasureLabel, pset);
    //old_dw->get(fail_old,            pFailureLabel,                pset);
    old_dw->get(ElasticStress_old,   pElasticStressLabel,          pset);
    old_dw->get(history1_old,        pHistory1Label,               pset);
    old_dw->get(history2_old,        pHistory2Label,               pset);
    old_dw->get(history3_old,        pHistory3Label,               pset);
    old_dw->get(history4_old,        pHistory4Label,               pset);
    old_dw->get(history5_old,        pHistory5Label,               pset);
    old_dw->get(history6_old,        pHistory6Label,               pset);
    
    new_dw->allocateAndPut(pstress,         lb->pStressLabel_preReloc,   pset);
    new_dw->allocateAndPut(pfiberdir_carry, lb->pFiberDirLabel_preReloc, pset);
    new_dw->allocateAndPut(pfa_carry,       lb->pFALabel_preReloc, pset);   //sg
    new_dw->allocateAndPut(stretch,         pStretchLabel_preReloc,       pset);
    //new_dw->allocateAndPut(fail,          pFailureLabel_preReloc,       pset);
    new_dw->allocateAndPut(ElasticStress,   pElasticStressLabel_preReloc, pset);
    new_dw->allocateAndPut(history1,        pHistory1Label_preReloc,      pset);
    new_dw->allocateAndPut(history2,        pHistory2Label_preReloc,      pset);
    new_dw->allocateAndPut(history3,        pHistory3Label_preReloc,      pset);
    new_dw->allocateAndPut(history4,        pHistory4Label_preReloc,      pset);
    new_dw->allocateAndPut(history5,        pHistory5Label_preReloc,      pset);
    new_dw->allocateAndPut(history6,        pHistory6Label_preReloc,      pset);
    new_dw->allocateAndPut(pdTdt,           lb->pdTdtLabel,               pset); // aaa
    new_dw->allocateAndPut(p_q,             lb->p_qLabel_preReloc,        pset);
    new_dw->get(pvolume,          lb->pVolumeLabel_preReloc,              pset);
    new_dw->get(deformationGradient_new,
                                  lb->pDeformationMeasureLabel_preReloc,  pset);
    new_dw->get(velGrad,          lb->pVelGradLabel_preReloc,             pset);

    //_____________________________________________material parameters
    double Bulk  = d_initialData.Bulk;
    double c1 = d_initialData.c1;
    double c2 = d_initialData.c2;
    double k1 = d_initialData.k1;
    double k2 = d_initialData.k2;
    // double lambda_star = d_initialData.lambda_star;
    double rho_orig = matl->getInitialDensity();
    double y1 = d_initialData.y1;// visco
    double y2 = d_initialData.y2;
    double y3 = d_initialData.y3;
    double y4 = d_initialData.y4;
    double y5 = d_initialData.y5;
    double y6 = d_initialData.y6;
    double t1 = d_initialData.t1;
    double t2 = d_initialData.t2;
    double t3 = d_initialData.t3;
    double t4 = d_initialData.t4;
    double t5 = d_initialData.t5;
    double t6 = d_initialData.t6;

    for(ParticleSubset::iterator iter = pset->begin();
        iter != pset->end(); iter++){
      particleIndex idx = *iter;

      // Assign zero internal heating by default - modify if necessary.
      pdTdt[idx] = 0.0;

      // get the volumetric part of the deformation
      J = deformationGradient_new[idx].Determinant();

      // carry forward fiber direction
      pfiberdir_carry[idx] = pfiberdir[idx];
      deformed_fiber_vector = pfiberdir[idx]; // not actually deformed yet
      pfa_carry[idx] = pfa[idx];              //sg
      
      kapa= 0.5*(-6+4*pow(pfa[idx],2)+2*sqrt(3*pow(pfa[idx],2)-2*pow(pfa[idx],4)))/(-9+6*pow(pfa[idx],2));

      //_______________________UNCOUPLE DEVIATORIC AND DILATIONAL PARTS
      //_______________________Ftilde=J^(-1/3)*F, Fvol=J^1/3*Identity
      //_______________________right Cauchy Green (C) tilde and invariants
      rightCauchyGreentilde_new = deformationGradient_new[idx].Transpose()
        * deformationGradient_new[idx]* pow(J,-(2./3.));
      I1tilde = rightCauchyGreentilde_new.Trace();
      I2tilde = .5*(I1tilde*I1tilde -
                 (rightCauchyGreentilde_new*rightCauchyGreentilde_new).Trace());
      I4tilde = Dot(deformed_fiber_vector,
                    (rightCauchyGreentilde_new*deformed_fiber_vector));
      lambda_tilde = sqrt(I4tilde);

      double I4 = I4tilde*pow(J,(2./3.));// For diagnostics only
      stretch[idx] = sqrt(I4);
      deformed_fiber_vector = deformationGradient_new[idx]*deformed_fiber_vector
        *(1./lambda_tilde*pow(J,-(1./3.)));
      Matrix3 DY(deformed_fiber_vector,deformed_fiber_vector);

      //________________________________left Cauchy Green (B) tilde
      leftCauchyGreentilde_new = deformationGradient_new[idx]
        * deformationGradient_new[idx].Transpose()*pow(J,-(2./3.));

      //________________________________strain energy derivatives
      
      if (lambda_tilde <= 1.){
        exponent= 1.;
        dWdI4tilde = 0.;
        d2WdI4tilde2 = 0.;
        dWdI1tilde = c1;
        d2WdI1tilde2= 0.;
        dWdI2tilde = c2;
        d2WdI2tilde2= 0.;
        shear = 2.*c1+c2;  // I have no clue about source of this expression 
        
      }
      else{
      exponent= exp(k2*pow((kapa*(I1tilde-3)+(1-3*kapa)*(I4tilde-1)),2));
      dWdI4tilde = k1*(kapa*(I1tilde-3)*(1-3*kapa)+(1-3*kapa)*(1-3*kapa)*(I4tilde-1))*exponent;
      d2WdI4tilde2= k1*(1-3*kapa)*(1-3*kapa)*exponent+dWdI4tilde*(2*k2*(1-3*kapa)*(kapa*(I1tilde-3)+(1-3*kapa)*(I4tilde-1)));
      dWdI1tilde = c1 + k1*(kapa*kapa*(I1tilde-3)+ kapa *(1-3*kapa)*(I4tilde-1))*exponent;
      d2WdI1tilde2= k1*kapa*kapa*exponent+dWdI1tilde*(2*k2*kapa*(kapa*(I1tilde-3)+(1-3*kapa)*(I4tilde-1)));
      dWdI2tilde = c2;
      d2WdI2tilde2= 0.;
      shear = 2.*c1+c2+I4tilde*(4.*d2WdI4tilde2*lambda_tilde*lambda_tilde
                                      -2.*dWdI4tilde*lambda_tilde); // I have no clue about source of this expression 
      }
      deviatoric_stress = (leftCauchyGreentilde_new*(dWdI1tilde+dWdI2tilde*I1tilde)
                        - leftCauchyGreentilde_new*leftCauchyGreentilde_new*dWdI2tilde
                        - Identity*(1./3.)*(dWdI1tilde*I1tilde+2.*dWdI2tilde*I2tilde))*2./J;
      //kept c2 terms for sake of extendibility to Mooney Rivlin type isotropic response
      fiber_stress = (DY*dWdI4tilde*I4tilde
                                    - Identity*(1./3.)*dWdI4tilde*I4tilde)*2./J;
      p = 0.5*Bulk*(J*J-1)/J; // p -= qVisco;
            if (p >= -1.e-5 && p <= 1.e-5)
              p = 0.;
            pressure = Identity*p;
    //_______________________________Cauchy stress
      ElasticStress[idx] = pressure + deviatoric_stress + fiber_stress;     
      
      // Compute deformed volume and local wave speed
      double rho_cur = rho_orig/J;
      //c_dil = sqrt((Bulk+1./3.*shear)/rho_cur); // I have no clue about source of this expression 
      c_dil = sqrt((Bulk+8./3.*c1)*pvolume[idx]/pmass[idx]);

      //_______________________________Viscoelastic stress
      double fac1,fac2,fac3,fac4,fac5,fac6;
      double exp1=0.,exp2=0.,exp3=0.,exp4=0.,exp5=0.,exp6=0.;
      Matrix3 Zero(0.);

      if(t1>0.){    // If t1>0, enter this section, otherwise skip it
        exp1 = exp(-delT/t1);
        fac1 = (1. - exp1)*t1/delT;
        history1[idx] = history1_old[idx]*exp1+
                       (ElasticStress[idx]-ElasticStress_old[idx])*fac1;
        if (t2 > 0.){
         exp2 = exp(-delT/t2);
         fac2 = (1. - exp2)*t2/delT;
         history2[idx] = history2_old[idx]*exp2+
                       (ElasticStress[idx]-ElasticStress_old[idx])*fac2;
        }
        else{
         history2[idx]= Zero;
        }
        if (t3 > 0.){
         exp3 = exp(-delT/t3);
         fac3 = (1. - exp3)*t3/delT;
         history3[idx] = history3_old[idx]*exp3+
                      (ElasticStress[idx]-ElasticStress_old[idx])*fac3;
        }
        else{
         history3[idx]= Zero;
        }
        if (t4 > 0.){
         exp4 = exp(-delT/t4);
         fac4 = (1. - exp4)*t4/delT;
         history4[idx] = history4_old[idx]*exp4+
                       (ElasticStress[idx]-ElasticStress_old[idx])*fac4;
        }
        else{
         history4[idx]= Zero;
        }
        if (t5 > 0.){
         exp5 = exp(-delT/t5);
         fac5 = (1. - exp5)*t5/delT;
         history5[idx] = history5_old[idx]*exp5+
                       (ElasticStress[idx]-ElasticStress_old[idx])*fac5;
        }
        else{
         history5[idx]= Zero;
        }
        if (t6 > 0.){
         exp6 = exp(-delT/t6);
         fac6 = (1. - exp6)*t6/delT;
         history6[idx] = history6_old[idx]*exp6+
                       (ElasticStress[idx]-ElasticStress_old[idx])*fac6;
        }
        else{
         history6[idx]= Zero;
       }
     }
     else{
        history1[idx]= Zero;
        history2[idx]= Zero;
        history3[idx]= Zero;
        history4[idx]= Zero;
        history5[idx]= Zero;
        history6[idx]= Zero;
     }

      pstress[idx] = history1[idx]*y1+history2[idx]*y2+history3[idx]*y3
                   + history4[idx]*y4+history5[idx]*y5+history6[idx]*y6
                   + ElasticStress[idx];
      //________________________________end stress

      // Compute the strain energy for all the particles
      U = .5*Bulk*((J*J-1)/2-log(J));
      W = c1*(I1tilde-3.)+c2*(I2tilde-3.)+0.5*k1/k2*(exponent-1);
      double e = (U + W)*pvolume[idx]/J;
      se += e;
      
      Vector pvelocity_idx = pvelocity[idx];

      WaveSpeed=Vector(Max(c_dil+fabs(pvelocity_idx.x()),WaveSpeed.x()),
                       Max(c_dil+fabs(pvelocity_idx.y()),WaveSpeed.y()),
                       Max(c_dil+fabs(pvelocity_idx.z()),WaveSpeed.z()));

      // Compute artificial viscosity term
      if (flag->d_artificial_viscosity) {
        double dx_ave = (dx.x() + dx.y() + dx.z())/3.0;
        double c_bulk = sqrt(Bulk/rho_cur);
        Matrix3 D=(velGrad[idx] + velGrad[idx].Transpose())*0.5;
        p_q[idx] = artificialBulkViscosity(D.Trace(), c_bulk, rho_cur, dx_ave);
      } else {
        p_q[idx] = 0.;
      }
    }  // end loop over particles

    WaveSpeed = dx/WaveSpeed;
    double delT_new = WaveSpeed.minComponent();
    new_dw->put(delt_vartype(delT_new), lb->delTLabel, patch->getLevel());
    
    if (flag->d_reductionVars->accStrainEnergy ||
        flag->d_reductionVars->strainEnergy) {
      new_dw->put(sum_vartype(se),      lb->StrainEnergyLabel);
    }
  }
}

//JIAHAO: compute injury
void HGOC::computeInjury(const PatchSubset* patches,
                                const MPMMaterial* matl,
                                DataWarehouse* old_dw,
                                DataWarehouse* new_dw){

// JIAHAO: comments for debugging purpose: testing where 
// sus utility has been executed
/*std::cout<<"*************"<<std::endl;
std::cout<<"UCNH: compute injury"<<std::endl;
std::cout<<"*************"<<std::endl;
*/

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
    constParticleVariable<Matrix3> pDefGrad;
    constParticleVariable<Matrix3> pDefGrad_new;
    constParticleVariable<double>  pInjury;
    Matrix3 Identity; Identity.Identity();
    //std::cout<<"pInjury is created"<<std::endl;
    // New data containers
    ParticleVariable<double>       pInjury_new;
    
    
    //std::cout<<"pInjury_new is created"<<std::endl;

    // Universal Gets

    old_dw->get(pDefGrad,            lb->pDeformationMeasureLabel, pset);
    old_dw->get(pInjury,             lb->pInjuryLabel             ,pset);
    new_dw->get(pDefGrad_new,lb->pDeformationMeasureLabel_preReloc,pset);

    
    // JIAHAO: injury Allocations
    new_dw->allocateAndPut(pInjury_new, lb->pInjuryLabel_preReloc, pset);
    
    //std::cout<<"pInjury_new allocated to pset"<<std::endl;

    ParticleSubset::iterator iter = pset->begin();
    for(; iter != pset->end(); iter++){
      particleIndex idx = *iter;

      // JIAHAO:
      //std::cout<<"the current pID is: "<<idx<<std::endl;

      double old_e1(0.0),old_e2(0.0),old_e3(0.0);
      double new_e1(0.0),new_e2(0.0),new_e3(0.0);
      double old_MPS(0.0), new_MPS(0.0);
      double threshold(1.0);

      //std::cout<<"pInjury before computation: "<<pInjury[idx]<<std::endl;
      // Matrix3 pDefGradTranspose = pDefGrad[idx].Transpose();
      Matrix3 EulerLagriangeTensorOld   = 0.5*(pDefGrad[idx].Transpose()*pDefGrad[idx]-Identity);
      Matrix3 EulerLagriangeTensorNew   = 0.5*(pDefGrad_new[idx].Transpose()*pDefGrad_new[idx]-Identity);
      int numEigenvalues_old=EulerLagriangeTensorOld.getEigenValues(old_e1,old_e2,old_e3);
      int numEigenvalues_new=EulerLagriangeTensorNew.getEigenValues(new_e1,new_e2,new_e3);
      old_MPS=std::abs(std::max(std::max(old_e1,old_e2),old_e3));
      new_MPS=std::abs(std::max(std::max(new_e1,new_e2),new_e3));
    

      if (new_MPS>=threshold||old_MPS>=threshold){
        if(old_MPS<threshold){
          pInjury_new[idx]=pInjury[idx]+std::abs(new_MPS-threshold)*0.5;
        }else if (new_MPS<threshold){
          pInjury_new[idx]=pInjury[idx]+std::abs(old_MPS-threshold)*0.5;
        }else{
          pInjury_new[idx]=pInjury[idx]+std::abs(new_MPS-old_MPS)*0.5;
        }
        
      }else{
        pInjury_new[idx]=pInjury[idx];
      }
    } // end loop over particles

  }
}

void HGOC::carryForward(const PatchSubset* patches,
                                 const MPMMaterial* matl,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw)
  //_____________________________used with RigidMPM
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    int dwi = matl->getDWIndex();
    ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

    // Carry forward the data common to all constitutive models 
    // when using RigidMPM.visco_one_cell_expl_parallel.ups
    // This method is defined in the ConstitutiveModel base class.
    carryForwardSharedData(pset, old_dw, new_dw, matl);

    // Carry forward the data local to this constitutive model 
    constParticleVariable<Vector> pfibdir;  
    ParticleVariable<Vector> pfibdir_new;
    ParticleVariable<double> pstretch;
    //constParticleVariable<Vector> pfail_old;
    //ParticleVariable<double> pfail;
    constParticleVariable<double> pfa1;
    ParticleVariable<double> pfa1_new;
    
    ParticleVariable<Matrix3> ElasticStress_new;//visco_label
    constParticleVariable<Matrix3> ElasticStress;
    ParticleVariable<Matrix3> history1,history2,history3;
    ParticleVariable<Matrix3> history4,history5,history6;
    constParticleVariable<Matrix3> history1_old,history2_old,history3_old;
    constParticleVariable<Matrix3> history4_old,history5_old,history6_old;

    old_dw->get(pfibdir,         lb->pFiberDirLabel,                   pset);
    old_dw->get(pfa1,         lb->pFALabel,                   pset); //sg
    //old_dw->get(pfail_old,       pFailureLabel,                        pset);
    old_dw->get(ElasticStress,    pElasticStressLabel,                 pset);
    old_dw->get(history1_old,     pHistory1Label,                      pset);
    old_dw->get(history2_old,     pHistory2Label,                      pset);
    old_dw->get(history3_old,     pHistory3Label,                      pset);
    old_dw->get(history4_old,     pHistory4Label,                      pset);
    old_dw->get(history5_old,     pHistory5Label,                      pset);
    old_dw->get(history6_old,     pHistory6Label,                      pset);
    
    new_dw->allocateAndPut(ElasticStress_new,pElasticStressLabel_preReloc,pset);
    new_dw->allocateAndPut(history1,         pHistory1Label_preReloc,     pset);
    new_dw->allocateAndPut(history2,         pHistory2Label_preReloc,     pset);
    new_dw->allocateAndPut(history3,         pHistory3Label_preReloc,     pset);
    new_dw->allocateAndPut(history4,         pHistory4Label_preReloc,     pset);
    new_dw->allocateAndPut(history5,         pHistory5Label_preReloc,     pset);
    new_dw->allocateAndPut(history6,         pHistory6Label_preReloc,     pset);
    new_dw->allocateAndPut(pfibdir_new,      lb->pFiberDirLabel_preReloc, pset);
    new_dw->allocateAndPut(pfa1_new,      lb->pFALabel_preReloc, pset);  //sg
    new_dw->allocateAndPut(pstretch,         pStretchLabel_preReloc,      pset);
    //new_dw->allocateAndPut(pfail,            pFailureLabel_preReloc,      pset);

    for(ParticleSubset::iterator iter = pset->begin();iter!=pset->end();iter++){
      particleIndex idx = *iter;
      pfibdir_new[idx] = pfibdir[idx];
      pfa1_new[idx] = pfa1[idx];
      pstretch[idx] = 1.0;
      //pfail[idx] = 0.0;
      
      ElasticStress_new[idx] = ElasticStress[idx];//visco_label
      history1[idx] = history1_old[idx];
      history2[idx] = history2_old[idx];
      history3[idx] = history3_old[idx];
      history4[idx] = history4_old[idx];
      history5[idx] = history5_old[idx];
      history6[idx] = history6_old[idx];

    }
    new_dw->put(delt_vartype(1.e10), lb->delTLabel, patch->getLevel());
    
    if (flag->d_reductionVars->accStrainEnergy ||
        flag->d_reductionVars->strainEnergy) {
      new_dw->put(sum_vartype(0.),     lb->StrainEnergyLabel);
    }
  }
}

void HGOC::addInitialComputesAndRequires(Task* task,
                                                    const MPMMaterial* matl,
                                                    const PatchSet*) const
{
  const MaterialSubset* matlset = matl->thisMaterial();
  task->computes(pStretchLabel,              matlset);
  //task->computes(pFailureLabel,              matlset);
  task->computes(pElasticStressLabel,        matlset);//visco_label
  task->computes(pHistory1Label,             matlset);
  task->computes(pHistory2Label,             matlset);
  task->computes(pHistory3Label,             matlset);
  task->computes(pHistory4Label,             matlset);
  task->computes(pHistory5Label,             matlset);
  task->computes(pHistory6Label,             matlset);
}

void HGOC::addComputesAndRequires(Task* task,
                                           const MPMMaterial* matl,
                                           const PatchSet* patches) const
  //___________TELLS THE SCHEDULER WHAT DATA
  //___________NEEDS TO BE AVAILABLE AT THE TIME computeStressTensor IS CALLED
{
  // Add the computes and requires that are common to all explicit 
  // constitutive models.  The method is defined in the ConstitutiveModel
  // base class.

  const MaterialSubset* matlset = matl->thisMaterial();
  addSharedCRForExplicit(task, matlset, patches);

  // Other constitutive model and input dependent computes and requires
  Ghost::GhostType  gnone = Ghost::None;

  task->requires(Task::OldDW, lb->pFiberDirLabel, matlset,gnone);
  task->requires(Task::OldDW, lb->pFALabel, matlset,gnone); //sg
  //task->requires(Task::OldDW, pFailureLabel,      matlset,gnone);

  task->requires(Task::OldDW, pElasticStressLabel,matlset,gnone);
  task->requires(Task::OldDW, pHistory1Label,     matlset,gnone);
  task->requires(Task::OldDW, pHistory2Label,     matlset,gnone);
  task->requires(Task::OldDW, pHistory3Label,     matlset,gnone);
  task->requires(Task::OldDW, pHistory4Label,     matlset,gnone);
  task->requires(Task::OldDW, pHistory5Label,     matlset,gnone);
  task->requires(Task::OldDW, pHistory6Label,     matlset,gnone);

  task->computes(lb->pFiberDirLabel_preReloc, matlset);
  task->computes(lb->pFALabel_preReloc, matlset);       //sg
  task->computes(pStretchLabel_preReloc,      matlset);
 // task->computes(pFailureLabel_preReloc,      matlset);
  task->computes(pElasticStressLabel_preReloc,          matlset);//visco_label
  task->computes(pHistory1Label_preReloc,               matlset);
  task->computes(pHistory2Label_preReloc,               matlset);
  task->computes(pHistory3Label_preReloc,               matlset);
  task->computes(pHistory4Label_preReloc,               matlset);
  task->computes(pHistory5Label_preReloc,               matlset);
  task->computes(pHistory6Label_preReloc,               matlset);
}

void HGOC::addComputesAndRequires(Task* ,
                                           const MPMMaterial* ,
                                           const PatchSet* ,
                                           const bool ) const
  //_________________________________________here this one's empty
{
}

// The "CM" versions use the pressure-volume relationship of the CNH model
double HGOC::computeRhoMicroCM(double pressure, 
                                        const double p_ref,
                                        const MPMMaterial* matl,
                                        double temperature,
                                        double rho_guess)
{
  double rho_orig = matl->getInitialDensity();
  double Bulk = d_initialData.Bulk;
  
  double p_gauge = pressure - p_ref;
  double rho_cur;
 
  if(d_useModifiedEOS && p_gauge < 0.0) {
    double A = p_ref;           // MODIFIED EOS
    double n = p_ref/Bulk;
    rho_cur = rho_orig*pow(pressure/A,n);
  } else {                      // STANDARD EOS
    rho_cur = rho_orig*(p_gauge/Bulk + sqrt((p_gauge/Bulk)*(p_gauge/Bulk) +1));
  }
  return rho_cur;
}

void HGOC::computePressEOSCM(const double rho_cur,double& pressure, 
                                      const double p_ref,
                                      double& dp_drho, double& tmp,
                                      const MPMMaterial* matl,
                                      double temperature)
{
  double Bulk = d_initialData.Bulk;
  double rho_orig = matl->getInitialDensity();

  if(d_useModifiedEOS && rho_cur < rho_orig){
    double A = p_ref;           // MODIFIED EOS
    double n = Bulk/p_ref;
    pressure = A*pow(rho_cur/rho_orig,n);
    dp_drho  = (Bulk/rho_orig)*pow(rho_cur/rho_orig,n-1);
    tmp      = dp_drho;         // speed of sound squared
  } else {                      // STANDARD EOS            
    double p_g = .5*Bulk*(rho_cur/rho_orig - rho_orig/rho_cur);
    pressure   = p_ref + p_g;
    dp_drho    = .5*Bulk*(rho_orig/(rho_cur*rho_cur) + 1./rho_orig);
    tmp        = Bulk/rho_cur;  // speed of sound squared
  }
}

double HGOC::getCompressibility()
{
  return 1.0/d_initialData.Bulk;
}


namespace Uintah {
  
#if 0
  static MPI_Datatype makeMPI_CMData()
  {
    ASSERTEQ(sizeof(HGOC::StateData), sizeof(double)*0);
    MPI_Datatype mpitype;
    MPI_Type_vector(1, 0, 0, MPI_DOUBLE, &mpitype);
    MPI_Type_commit(&mpitype);
    return mpitype;
  }
  
  const TypeDescription* fun_getTypeDescription(HGOC::StateData*)
  {
    static TypeDescription* td = 0;
    if(!td){
      td = scinew TypeDescription(TypeDescription::Other,
                                  "HGOC::StateData", true, &makeMPI_CMData);
    }
    return td;
  }
#endif
} // End namespace Uintah

