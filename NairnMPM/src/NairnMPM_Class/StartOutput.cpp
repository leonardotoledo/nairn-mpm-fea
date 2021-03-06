/*********************************************************************
    OutputSetup.cpp
    Nairn Research Group MPM Code
    
    Created by jnairn on Wed Jan 23 2002.
    Copyright (c) 2001 John A. Nairn, All rights reserved.
*********************************************************************/

#include "stdafx.h"
#include "NairnMPM_Class/NairnMPM.hpp"
#include "Materials/MaterialBase.hpp"
#include "Global_Quantities/ThermalRamp.hpp"
#include "System/ArchiveData.hpp"
#include "Global_Quantities/BodyForce.hpp"
#include "Cracks/CrackSurfaceContact.hpp"
#include "Exceptions/CommonException.hpp"
#include "Elements/ElementBase.hpp"
#include "Nodes/NodalPoint.hpp"
#include "Custom_Tasks/DiffusionTask.hpp"
#include "Custom_Tasks/ConductionTask.hpp"
//#include "Boundary_Conditions/NodalConcBC.hpp"
//#include "Boundary_Conditions/NodalTempBC.hpp"
#include "Boundary_Conditions/NodalVelBC.hpp"
//#include "Boundary_Conditions/MatPtLoadBC.hpp"
//#include "Boundary_Conditions/MatPtTractionBC.hpp"
//#include "Boundary_Conditions/MatPtFluxBC.hpp"
//#include "Boundary_Conditions/MatPtHeatFluxBC.hpp"
#include "Cracks/CrackHeader.hpp"
#include "System/UnitsController.hpp"

/*********************************************************************
    Print setup information
*********************************************************************/

// print title of results file
void NairnMPM::PrintAnalysisTitle(void)
{
	cout << "MPM ANALYSIS BY ";
	CoutCodeVersion();
}

// print analysis type
// throws CommonException()
void NairnMPM::PrintAnalysisType(void)
{
	// new section
    PrintSection("MPM ANALYSIS");
    
	// MPM method
    cout << "MPM Analysis Method: ";
    switch(mpmApproach)
    {	case USF_METHOD:
            cout << "USF";
            break;
        case USAVG_METHOD:
            cout << "USAVG";
        case SZS_METHOD:
            if(mpmApproach==SZS_METHOD) cout << "USL";
			if(fmobj->skipPostExtrapolation)
				cout << " (-2nd extrap)";
			else
				cout << " (+2nd extrap)";
            break;
        default:
            throw CommonException("Invalid MPM analysis method provided.","NairnMPM::PrintAnalysisType");
    }
	
    switch(ElementBase::useGimp)
    {   case UNIFORM_GIMP:
		case UNIFORM_GIMP_AS:
            cout << " / GIMP";
            break;
        case LINEAR_CPDI:
		case LINEAR_CPDI_AS:
            cout << " / Linear CPDI";
            break;
        case QUADRATIC_CPDI:
            cout << " / Quadratric CPDI";
            break;
    }
	cout << endl;
    
	// time step and max time
    cout << "Time step: min(" << timestep*UnitsController::Scaling(1.e3) << " " << UnitsController::Label(ALTTIME_UNITS) << ", "
		<< fmobj->GetCFLCondition() << " time for wave to cross one cell)\n";
    cout << "Maximum time: " << maxtime*UnitsController::Scaling(1.e3) << " " << UnitsController::Label(ALTTIME_UNITS) << "\n\n";
}

// rest of MPM information
// throws CommonException()
void NairnMPM::MyStartResultsOutput(void)
{
    //---------------------------------------------------
    // Gravity and Damping and Thermal Ramp
    PrintSection("GRAVITY, DAMPING, AND THERMAL");
	bodyFrc.Output();
	thermal.Output();
    ConductionTask::ThermodynamicsOutput();
    cout << endl;

    //---------------------------------------------------
    // Cracks
    if(firstCrack!=NULL)
    {   PrintSection("CRACKS AND CRACK CONTACT");
		int i;
		for(i=0;i<=1;i++)
		{	if(i==0)
				cout << "Default propagation criterion: ";
			else
			{	if(propagate[i]==NO_PROPAGATION) break;
				cout << "Alternate propagation criterion: ";
			}
			switch(propagate[i])
			{   case NO_PROPAGATION:
					cout << "No propagation" << endl;
					break;
				case MAXHOOPSTRESS:
					cout << "Maximum hoop stress" << MaterialBase::PreferredDirection(propagateDirection[i]) << endl;
					break;
				case STEADYSTATEGROWTH:
					cout << "Constant crack speed" << MaterialBase::PreferredDirection(propagateDirection[i]) << endl;
					break;
				case CRITICALERR:
					cout << "Critical energy release rate" << MaterialBase::PreferredDirection(propagateDirection[i]) << endl;
					break;
				case STRAINENERGYDENSITY:
					cout << "Minimum strain energy density" << MaterialBase::PreferredDirection(propagateDirection[i]) << endl;
					break;
				case EMPIRICALCRITERION:
					cout << "Empirical criterion" << MaterialBase::PreferredDirection(propagateDirection[i]) << endl;
					break;
				case MAXCTODCRITERION:
					cout << "Maximum CTOD criterion " << MaterialBase::PreferredDirection(propagateDirection[i]) << endl;
					break;
				default:
					cout << "Unknown criterion" << endl;
					break;
			}
			if(propagate[i]!=NO_PROPAGATION && propagateMat[i]>0)
			{	cout << "   New crack surface has traction law material " << propagateMat[i] << endl;
				if(propagateMat[i]>nmat)
					throw CommonException("Propagation traction law material is not defined","NairnMPM::MyStartResultsOutput");
				if(theMaterials[propagateMat[i]-1]->MaterialStyle()!=TRACTION_MAT)
					throw CommonException("Propagation traction law is not a traction law material","NairnMPM::MyStartResultsOutput");
			}
		}
		
		// contact output and allocations
		contact.Output();
		
        // location for COD calculation - range is 0 to 3 segments from crack tip
        // future may want to read this as parameter
        // Only used to partition J or to implement COD-based criteria
        CrackHeader::SetCodLocation(2.);
        cout << "Crack COD found (when needed) " << 2 << " segments from crack tip" << endl;
        
		// crack details
		cout << "Number of cracks = " << numberOfCracks << endl;
		CrackHeader *nextCrack=firstCrack;
		while(nextCrack!=NULL)
		{	nextCrack->Output();
			nextCrack=(CrackHeader *)nextCrack->GetNextObject();
		}
		cout << endl;
        
    }
    
    //---------------------------------------------------
    // Multimaterial Contact
	if(multiMaterialMode)
    {   PrintSection("MULTIMATERIAL CONTACT");
		contact.MaterialOutput();
		int i;
		for(i=0;i<nmat;i++)
			theMaterials[i]->ContactOutput(i+1);
		cout << endl;
	}
	
    //---------------------------------------------------
    // Fixed Displacements
    if(firstVelocityBC!=NULL)
    {   PrintSection("NODAL POINTS WITH FIXED DISPLACEMENTS");
        archiver->ArchiveVelocityBCs(firstVelocityBC);
    }
    
}
