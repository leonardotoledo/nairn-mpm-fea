/********************************************************************************
    MatPtLoadBC.cpp
    nairn-mpm-fea
    
    Created by John Nairn on Tues Feb 5 2002.
    Copyright (c) 2001 John A. Nairn, All rights reserved.
********************************************************************************/

#include "stdafx.h"
#include "Boundary_Conditions/MatPtLoadBC.hpp"
#include "MPM_Classes/MPMBase.hpp"
#include "Materials/MaterialBase.hpp"
#include "NairnMPM_Class/MeshInfo.hpp"
#include "Elements/ElementBase.hpp"
#include "System/UnitsController.hpp"
#include "NairnMPM_Class/MeshInfo.hpp"

// global
MatPtLoadBC *firstLoadedPt=NULL;

#pragma mark MatPtLoadBC::Constructors and Destructors

// Constructors
MatPtLoadBC::MatPtLoadBC(int num,int dof,int sty)
			: BoundaryCondition(sty,(double)0.,(double)0.)
{
    ptNum=num;
    direction=dof;		// note that 3 or 4 means z here, which may not be Z_DIRECTION (4) bit location
						// it is assumed z if not x (=1) or y (=2)
    holding=false;
}

// When particles reordered, fix point num if needed
// nonrigid particle p2+1 just moved to p1+1 and rigid particle
//		p1+1 moved to p2+1
MatPtLoadBC *MatPtLoadBC::ReorderPtNum(int p1, int p2)
{	if(ptNum==p2+1)
		ptNum = p1+1;
	else if(ptNum==p1+1)
		ptNum = p2+1;
	return (MatPtLoadBC *)GetNextObject();
}

#pragma mark MatPtLoadBC: Methods

// print to output
BoundaryCondition *MatPtLoadBC::PrintBC(ostream &os)
{
    char nline[200];
    
    sprintf(nline,"%7d %2d %2d %15.7e %15.7e",ptNum,direction,style,
			UnitsController::Scaling(1.e-6)*GetBCValueOut(),GetBCFirstTimeOut());
    os << nline;
	PrintFunction(os);
    
    // initial value is F in N or N/numParticles if net, but if function
    //      initial value is 1 or 1/numParticles if net
	
	// rescale ... for fuction value is 1 or 1/numParticles if net force
	if(style==FUNCTION_VALUE)
		scale = GetBCValue();
	
    return (BoundaryCondition *)GetNextObject();
}

// set external load to zero in both directions
MatPtLoadBC *MatPtLoadBC::ZeroMPLoad(void)
{
	ZeroVector(mpm[ptNum-1]->GetPFext());
    return (MatPtLoadBC *)GetNextObject();
}

// increment external load on a particle
// input is analysis time in seconds
MatPtLoadBC *MatPtLoadBC::AddMPLoad(double bctime)
{
	if(style!=SILENT)
	{	Vector *pFext=mpm[ptNum-1]->GetPFext();
		
		if(direction==X_DIRECTION)
			 pFext->x+=BCValue(bctime);
		else if(direction==Y_DIRECTION)
			 pFext->y+=BCValue(bctime);
		else
			 pFext->z+=BCValue(bctime);
	}
	else
	{	MPMBase *mptr = mpm[ptNum-1];
		double mp=mptr->mp;												// in g
		int matnum=mptr->MatID();
		double cd=theMaterials[matnum]->CurrentWaveSpeed(FALSE,mpm[ptNum-1],0);	// in mm/sec (2D or isotropic only)
		double cs=theMaterials[matnum]->ShearWaveSpeed(FALSE,mpm[ptNum-1],0);		// in mm/sec
		Vector pVel=mptr->vel;
		Vector *pFext=mptr->GetPFext();
		
		// get forces in g-mm/sec^2
		if(direction==X_DIRECTION)
		{	pFext->x-=mp*cd*pVel.x/(2.*mptr->GetParticleXSize());
			pFext->y-=mp*cs*pVel.y/(2.*mptr->GetParticleXSize());
			pFext->z-=mp*cs*pVel.z/(2.*mptr->GetParticleXSize());
		}
		else if(direction==Y_DIRECTION)
		{	pFext->x-=mp*cs*pVel.x/(2.*mptr->GetParticleYSize());
			pFext->y-=mp*cd*pVel.y/(2.*mptr->GetParticleYSize());
			pFext->z-=mp*cs*pVel.z/(2.*mptr->GetParticleYSize());
		}
		else
		{	pFext->x-=mp*cs*pVel.x/(2.*mptr->GetParticleZSize());
			pFext->y-=mp*cs*pVel.y/(2.*mptr->GetParticleZSize());
			pFext->z-=mp*cd*pVel.z/(2.*mptr->GetParticleZSize());
		}
	}
		
    return (MatPtLoadBC *)GetNextObject();
}

// reverse active linear loads. Leave rest alone
// input is analysis time in seconds
// if LINEAR_VALUE, set finalTime to time when load returns to zero
MatPtLoadBC *MatPtLoadBC::ReverseLinearLoad(double bctime,double *finalTime,bool holdFirst)
{
    switch(style)
    {	case LINEAR_VALUE:
            if(bctime>=GetBCFirstTime())
			{   SetBCOffset(BCValue(bctime));
                if(holdFirst)
				{	// change to offset (set above) with zero slope to get a constant load
                    holdValue = GetBCValue();
                    holding = true;
                    SetBCValue(0.);
                }
                else
				{	// get unloading slope (units/time)
					double bcvalue = holding ? -holdValue : -GetBCValue() ;
					
					// change to offset+(slope)*(time-ftime) by setting offset (above), new slope, and new first time 
					SetBCValueCU(bcvalue);
                    SetBCFirstTimeCU(bctime);
                    
					// find time when BC returns to zero
                    // new BC is offset+value*(mstime-ftime), which is zero when mstime = ftime-offset/value
                    *finalTime = GetBCFirstTime() - GetBCOffset()/GetBCValue();
                }
                
            }
            break;
        default:
            break;
    }
    return (MatPtLoadBC *)GetNextObject();
}

// hold active linear loads at current values. Leave rest alone
// input is analysis time in seconds
MatPtLoadBC *MatPtLoadBC::MakeConstantLoad(double bctime)
{
    switch(style)
    {	case LINEAR_VALUE:
            if(bctime>=GetBCFirstTime())
            {	style=CONSTANT_VALUE;
				SetBCValueCU(BCValue(bctime));
            }
            break;
        default:
            break;
    }
    return (MatPtLoadBC *)GetNextObject();
}

// compact CPDI surface nodes into arrays
int MatPtLoadBC::CompactCornerNodes(int numDnds,Vector *corners,int *cElem,double ratio,int *nds,double *fn)
{	
    // loop over corners finding all nodes and add to force
    // maximum is numDnds nodes with 8 nodes (if 3D) for each
    int i,j,numnds,ncnds=0;
#ifdef CONST_ARRAYS
	int cnodes[8 * 4];
	double twt[8*4];
#else
	int cnodes[8 * numDnds];
	double twt[8 * numDnds];
#endif
    double scale = 1.;
	
    for(i=0;i<numDnds;i++)
	{	// get straight grid shape functions
		theElements[cElem[i]]->GridShapeFunctions(&numnds,nds,&corners[i],fn);
		
		// loop over shape grid shape functions and collect in arrays
        // means to add cnodes[k] with shape function weight twt[k]
		for(j=1;j<=numnds;j++)
		{   cnodes[ncnds] = nds[j];
			twt[ncnds] = fn[j]*scale;
			ncnds++;
		}
		
		// in case axisymmetric, scale weight for second node (numDnds will be 2)
		scale = ratio;
	}
	
 	// shell sort by node numbers in cnodes[] (always 16 for linear CPDI)
    // sort to get repeating nodes together
	int lognb2=(int)(log((double)ncnds)*1.442695022+1.0e-5);	// log base 2
	int k=ncnds,l,cmpNode;
	double cmpSi;
	for(l=1;l<=lognb2;l++)
	{	k>>=1;		// divide by 2
		for(j=k;j<ncnds;j++)
		{	i=j-k;
			cmpNode = cnodes[j];
			cmpSi = twt[j];
			
			// Back up until find insertion point
			while(i>=0 && cnodes[i]>cmpNode)
			{	cnodes[i+k] = cnodes[i];
				twt[i+k] = twt[i];
				i-=k;
			}
			
			// Insert point
			cnodes[i+k]=cmpNode;
			twt[i+k]=cmpSi;
		}
	}
    
 	// compact same node number
	int count = 0;
	nds[0] = -1;
	fn[0] = 1.;
	for(i=0;i<ncnds;i++)
	{   if(cnodes[i] == nds[count])
        {   fn[count] += twt[i];
        }
        else
        {	if(fn[count]>1.e-10) count++;       // keep only if shape is nonzero
            nds[count] = cnodes[i];
            fn[count] = twt[i];
        }
	}
	if(fn[count]<1.e-10) count--;
    
    return count;
}

#pragma mark MatPtLoadBC:Flux method in sub classes

// add "flux" condition - must override in sub class flux BCx
MatPtLoadBC *MatPtLoadBC::AddMPFluxBC(double bctime)
{	return (MatPtLoadBC *)GetNextObject();
}


#pragma mark MatPtLoadBC:Accessors

// get current position of particle
void MatPtLoadBC::GetPosition(double *xpos,double *ypos,double *zpos,double *rot)
{	*xpos=mpm[ptNum-1]->pos.x;
	*ypos=mpm[ptNum-1]->pos.y;
	*zpos=mpm[ptNum-1]->pos.z;
	*rot=mpm[ptNum-1]->GetParticleRotationZ();
}

// set value (and scale legacy N to uN, and MPa to Pa)
void MatPtLoadBC::SetBCValue(double bcvalue)
{	BoundaryCondition::SetBCValue(UnitsController::Scaling(1.e6)*bcvalue);
}

#pragma mark MatPtLoadBC: Class Methods

// Calculate forces applied to particles at time stepTime in g-mm/sec^2
void MatPtLoadBC::SetParticleFext(double stepTime)
{
    MatPtLoadBC *nextLoad=firstLoadedPt;
    while(nextLoad!=NULL)
    	nextLoad=nextLoad->ZeroMPLoad();
    nextLoad=firstLoadedPt;
    while(nextLoad!=NULL)
    	nextLoad=nextLoad->AddMPLoad(stepTime);
}

