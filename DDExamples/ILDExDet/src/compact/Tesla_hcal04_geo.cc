// $Id:$
//====================================================================
//  AIDA Detector description implementation for LCD
//--------------------------------------------------------------------
//
//  Author     : M.Frank
//
//====================================================================
#include "DD4hep/DetFactoryHelper.h"
#include "TGeoTube.h"

using namespace std;
using namespace DD4hep;
using namespace DD4hep::Geometry;

static Ref_t create_element(LCDD& lcdd, const xml_h& e, SensitiveDetector& sens)  {
  struct param_t { double inner, outer, zhalf; };
  xml_det_t   x_det  = e;
  string      name   = x_det.nameStr();
  DetElement  sdet(name,x_det.id());
  Assembly    assembly(name+"_assembly");
  xml_comp_t  x_barrel = x_det.child(Unicode("barrel"));
  xml_comp_t  x_endcap = x_det.child(Unicode("endcap"));
  param_t barrel = { x_barrel.inner_r(), x_barrel.outer_r(), x_barrel.zhalf()};
  param_t endcap = { x_endcap.inner_r(), x_endcap.outer_r(), x_endcap.thickness()/2.0};
  int symmetry   = x_det.child(Unicode("symmetry")).attr<int>(_A(value));
  double tilt    = M_PI/symmetry - M_PI/2.0;

  // Visualisation attributes
  VisAttr vis = lcdd.visAttributes(x_det.visStr());
#if 0
  //--------- BarrelHcal Sensitive detector -----

  // sensitive Model
  db->exec("select Model from `sensitive`;");
  db->getTuple();
  SensitiveModel = db->fetchString("Model");

  G4String SensitiveLog= "The sensitive model in Hcal chambers is " + SensitiveModel;
  Control::Log(SensitiveLog.data());

  
  // if RPC1 read the RPC parameters
  if (SensitiveModel == "RPC1")
    {
      db->exec("select * from rpc1;");
      db->getTuple();
      g10_thickness=db->fetchDouble("g10_thickness");
      glass_thickness=db->fetchDouble("glass_thickness");
      gas_thickness=db->fetchDouble("gas_thickness");
      spacer_thickness=db->fetchDouble("spacer_thickness");
      spacer_gap=db->fetchDouble("spacer_gap");
    }

  db->exec("select cell_dim_x, cell_dim_z, chamber_tickness from barrel_module,hcal;");
  db->getTuple();

  double chamber_tickness = x_params.attr<double>(Unicode("chamber_tickness"));
  double cell_dim_x = x_params.attr<double>(Unicode("cell_dim_x"));
  double cell_dim_z = x_params.attr<double>(Unicode("cell_dim_z"));

  // The cell boundaries does not really exist as G4 volumes. So,
  // to avoid long steps over running  several cells, the 
  // theMaxStepAllowed inside the sensitive material is the
  // pad smaller x or z dimension.
  theMaxStepAllowed= std::min(cell_dim_x,cell_dim_z);
#if 0  
  //  Hcal  barrel regular modules
  theBarrilRegSD =     new SD(cell_dim_x,cell_dim_z,chamber_tickness,HCALBARREL,"HcalBarrelReg");
  RegisterSensitiveDetector(theBarrilRegSD);
  
  // Hcal  barrel end modules
  theBarrilEndSD = new SD(cell_dim_x,cell_dim_z,chamber_tickness,HCALBARREL,"HcalBarrelEnd");
  RegisterSensitiveDetector(theBarrilEndSD);

  // Hcal  endcap modules
  theENDCAPEndSD = new HECSD(cell_dim_x,cell_dim_z,chamber_tickness,HCALENDCAPMINUS,"HcalEndCaps");
  RegisterSensitiveDetector(theENDCAPEndSD);
#endif
  // Set up the Radiator Material to be used
  Material radiatorMat = lcdd.material(x_radiator.materialStr());
  db->exec("select material from radiator;");
  db->getTuple();
  
  string RadiatorMaterialName = db->fetchString("material");

  //----------------------------------------------------
  // Barrel
  //----------------------------------------------------
  BarrelRegularModules(WorldLog);
  BarrelEndModules(WorldLog);
  
  //----------------------------------------------------
  // EndCap Modules
  //----------------------------------------------------
  Endcaps(WorldLog);
}



//~              Barrel Regular Modules               ~
void Hcal04::BarrelRegularModules(G4LogicalVolume* MotherLog)
{
  // Regular modules

  db->exec("select bottom_dim_x/2 AS BHX,midle_dim_x/2. AS MHX, top_dim_x/2 AS THX, y_dim1_for_x/2. AS YX1H,y_dim2_for_x/2. AS YX2H,module_dim_z/2. AS DHZ from barrel_module,barrel_regular_module;");
  db->getTuple();
  double BottomDimY = db->fetchDouble("YX1H");
  double chambers_y_off_correction = db->fetchDouble("YX2H");
  
  // Attention: on b�tit le module dans la verticale
  // � cause du G4Trd et on le tourne avant de le positioner
  Trd   trdBottom(bottom_dim_x/2, midle_dim_x/2, module_dim_z/2, module_dim_z/2, y_dim1_for_x/2);
  Trd   trdTop   (midle_dim_x/2,  top_dim_x/2,   module_dim_z/2, module_dim_z/2, y_dim2_for_x/2);

  UnionSolid modSolid (trdBottom, trdTop, Position(0,0,(y_dim1_for_x+y_dim2_for_x)/2.));
  Volume     modVolume(name+"_module",modSolid,radiatorMat);
  modVolume.setVosAttributes(moduleVis);

  // Chambers in the Hcal Barrel 
  BarrelRegularChambers(EnvLogHcalModuleBarrel,chambers_y_off_correction);

  //   // BarrelStandardModule placements
  db->exec("select stave_id,module_id,module_type,stave_phi_offset,inner_radius,module_z_offset from barrel,barrel_stave, barrel_module, barrel_modules where module_type = 1;"); //  un module: AND stave_id=1 AND module_id = 2
  db->getTuple();
  double Y;
  Y = db->fetchDouble("inner_radius")+BottomDimY;

  do {
    double phirot = db->fetchDouble("stave_phi_offset")*pi/180;
    G4RotationMatrix *rot=new G4RotationMatrix();
    rot->rotateX(pi*0.5); // on couche le module.
    rot->rotateY(phirot);
    new MyPlacement(rot,
		    G4ThreeVector(-Y*sin(phirot),
				  Y*cos(phirot),
				  db->fetchDouble("module_z_offset")),
		    EnvLogHcalModuleBarrel,
		    "BarrelHcalModule",
		    MotherLog,
		    false,
		    HCALBARREL*100+db->fetchInt("stave_id")*10+
		    db->fetchInt("module_id"));
    theBarrilRegSD->SetStaveRotationMatrix(db->fetchInt("stave_id"),phirot);
    theBarrilRegSD->
      SetModuleZOffset(db->fetchInt("module_id"),
		       db->fetchDouble("module_z_offset"));
  } while(db->getTuple()!=NULL);
}

void Hcal04::BarrelRegularChambers(G4LogicalVolume* MotherLog,double chambers_y_off_correction)
{
  
  G4LogicalVolume * ChamberLog[200];
  G4Box * ChamberSolid;
  
  G4VisAttributes *VisAtt = new G4VisAttributes(G4Colour(.2,.8,.2));
  VisAtt->SetForceWireframe(true);
  //VisAtt->SetForceSolid(true);
  
  db->exec("select layer_id,chamber_dim_x/2. AS xdh,chamber_tickness/2. AS ydh,chamber_dim_z/2. AS zdh, fiber_gap from hcal,barrel_regular_layer,barrel_regular_module;");
  
  G4UserLimits* pULimits=
    new G4UserLimits(theMaxStepAllowed);

  // the scintillator width is the chamber width - fiber gap 
  double fiber_gap = 0.0;
  double dx = chamber_dim_x;
  double dy = chamber_dim_y;
  double dz = chamber_dim_z;
  Material polystyrene = lcdd.material("polystyrene");
  for(size_t i=0; i<num_Layers; ++i)    {
    Box  chamberBox(dx,dy,dz);
    if(SensitiveModel == "scintillator")      {
      //fg: introduce (empty) fiber gap - should be filled with fibres and cables - so far we fill it  with air ...
      Volume chamberVol(name+"_chamber",chamberBox,lcdd.air());
      double scintHalfWidth =  dy - fiber_gap / 2. ;
      // fiber gap can't be larger than total chamber
      assert( scintHalfWidth > 0. ) ;
      
      Box    scintillatorBox(dx,dz,scintHalfWidth);
      Volume scintillatorVol(name+"_scintillator",scintillatorBox,polystyrene);
      // only scinitllator is sensitive
      //ScintLog->SetSensitiveDetector(theBarrilRegSD);
      // Whatever the MyPlacement shit does...
      PlacedVolume pv=chamberVol.placeVolume(scintillatorVol,Position(0,0,- fiber_gap / 2.));
      pv.addPhysVolId("layer",i);
      ChamberLog [i] = chamberVol;
    }
    else if (SensitiveModel == "RPC1")   {
      G4int layer_id = i;
      ChamberLog [layer_id] =
	BuildRPC1Box(ChamberSolid,
		     theBarrilRegSD,
		     layer_id,
		     pULimits);
    }
    else  {
      Control::Abort("Invalid sensitive model in the dababase!",MOKKA_ERROR_BAD_DATABASE_PARAMETERS);
    }

    ChamberLog[db->fetchInt("layer_id")]->SetVisAttributes(VisAtt);
  }   
  
  // Chamber Placements
  // module x and y offsets (needed for the SD)
  db->exec("select 0 AS module_x_offset, module_y_offset from barrel_module;");
  db->getTuple();
  
  double Xoff,Yoff;
  Xoff = db->fetchDouble("module_x_offset");
  Yoff = db->fetchDouble("module_y_offset");
  
  db->exec("select layer_id, 0. as chamber_x_offset,chamber_y_offset,0. as chamber_z_offset from barrel_regular_layer;");
   
  while(db->getTuple()!=NULL){    
    G4int layer_id = db->fetchInt("layer_id");
    new MyPlacement(0,
		    G4ThreeVector(db->fetchDouble("chamber_x_offset"),
				  db->fetchDouble("chamber_z_offset"),
				  db->fetchDouble("chamber_y_offset")+
				  chambers_y_off_correction),
		    //!!attention!! y<->z
		    ChamberLog [layer_id],
		    "ChamberBarrel",
		    MotherLog,false,layer_id);
    theBarrilRegSD->
      AddLayer(layer_id,
	       db->fetchDouble("chamber_x_offset") + Xoff - 
	       ((G4Box *)ChamberLog[layer_id]->GetSolid())->GetXHalfLength(),
	       db->fetchDouble("chamber_y_offset") + Yoff,
	       // - ((G4Box *)ChamberLog [layer_id]->GetSolid())->GetZHalfLength(),
	       db->fetchDouble("chamber_z_offset") - 
	       ((G4Box *)ChamberLog[layer_id]->GetSolid())->GetYHalfLength());    
  }
  helpBarrel.layerPos.push_back( db->fetchDouble("chamber_y_offset") + solidOfLog->GetZHalfLength() );
#endif

}
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~              Barrel End Modules                 ~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Hcal04::BarrelEndModules(G4LogicalVolume* MotherLog)
{
  // End modules
  // Get parameters
  db->exec("select y_dim1_for_z/2. AS YZ1H, y_dim2_for_z/2. AS YZ2H, y_dim3_for_z/2. AS YZ3H, top_end_dim_z/2.  AS TZ from barrel_end_module;");
  db->getTuple();
  double YZ1H = db->fetchDouble("YZ1H");
  double YZ2H = db->fetchDouble("YZ2H");
  double YZ3H = db->fetchDouble("YZ3H");
  double TZ = db->fetchDouble("TZ");
  double chambers_y_off_correction = YZ3H;

  db->exec("select bottom_dim_x/2 AS BHX,midle_dim_x/2. AS MHX, top_dim_x/2 AS THX, y_dim1_for_x/2. AS YX1H,y_dim2_for_x/2. AS YX2H,module_dim_z/2. AS DHZ from barrel_module,barrel_regular_module;");
  db->getTuple();
  double BHX = db->fetchDouble("BHX");
  double MHX = db->fetchDouble("MHX");
  double THX = db->fetchDouble("THX");
  double YX1H = db->fetchDouble("YX1H");
  double YX2H = db->fetchDouble("YX2H");
  double DHZ = db->fetchDouble("DHZ");

  // Attention: on b�tit le module dans la verticale
  // � cause des G4Trd et on le tourne avant de le positioner
  //
  // Base

  double MHXZ = BHX+(YZ1H+YZ2H)*(MHX-BHX)/YX1H;
  G4Trd * Base1 = new G4Trd("Base1_Barrel_End_Module",
			    BHX, // dx1 
			    MHXZ, // dx2
			    DHZ, // dy1
			    DHZ, // dy2
			    YZ1H+YZ2H); // dz
  
  G4Trd * Base2 = new G4Trd("Base2_Barrel_End_Module",
			    MHXZ,  
			    MHX,
			    TZ,
			    TZ,
			    YX1H-(YZ1H+YZ2H));

  G4UnionSolid* Base = 
    new G4UnionSolid("BasesEndSolid",
		     Base1,
		     Base2,
		     0,
		     G4ThreeVector(0,
				   TZ-DHZ,
				   YX1H));
  
  G4Trd * Top = new G4Trd("Top_Barrel_End_Module",
			  MHX, 
			  THX,
			  TZ,
			  TZ,
			  YX2H);
  
  G4UnionSolid* Ensemble1 = 
    new G4UnionSolid("EndSolid",
		     Base,
		     Top,
		     0,
		     G4ThreeVector(0,
				   TZ-DHZ,
				   2*YX1H-(YZ1H+YZ2H)+YX2H));

  double MHXZ1 = BHX+((YZ1H+YZ2H)-(TZ-DHZ))*(MHX-BHX)/YX1H;

  G4RotationMatrix *rot = new G4RotationMatrix();
  rot->rotateZ(pi/2.);


  double pDX = TZ-DHZ;
  double pDz = sqrt(4*YZ2H*YZ2H+pDX*pDX)/2.;  
  double pTheta = pi/2-atan(2*pDz/pDX);

  G4Trap* detail = new G4Trap("Trap",
			      pDz, //pDz
			      -pTheta,    //pTheta
			      0.,       //pPhi
			      MHXZ1, //pDy1
			      G4GeometryTolerance::GetInstance()->GetSurfaceTolerance(), //pDx1  
			      G4GeometryTolerance::GetInstance()->GetSurfaceTolerance(), //pDx2
			      0.,      //pAlp1
			      MHXZ, //pDy2,
			      pDX, //pDx3
			      pDX, //pDx4
			      0.);      //pAlp2
  
    
  G4UnionSolid* Ensemble2 = 
    new G4UnionSolid("EndSolid",
		     Ensemble1,
		     detail,
		     rot,
		     G4ThreeVector(0,
				   DHZ+pDX-pDz*tan(pTheta),
				   YZ1H+YZ2H-pDz));  //-pDX/2/tan(pTheta)
  
  EnvLogHcalModuleBarrel  = new G4LogicalVolume(Ensemble2,
 						RadiatorMaterial,
 						"barrelHcalModule", 
 						0, 0, 0);

  G4VisAttributes * VisAtt = new G4VisAttributes(G4Colour(.8,.8,.2));
  VisAtt->SetForceWireframe(true);
  VisAtt->SetDaughtersInvisible(true);
  //VisAtt->SetForceSolid(true);
  EnvLogHcalModuleBarrel->SetVisAttributes(VisAtt);


  //----------------------------------------------------
  // Chambers in the Hcal Barrel 
  //----------------------------------------------------
  BarrelEndChambers(EnvLogHcalModuleBarrel,chambers_y_off_correction);

  //   // Barrel End Module placements
  db->exec("select stave_id,module_id,module_type,stave_phi_offset,inner_radius,module_z_offset from barrel,barrel_stave, barrel_module, barrel_modules where module_type = 2;");  // un module:  AND module_id = 1 AND stave_id = 1


  // Take care of this return here: if is possible when building
  // the Hcal prototype single module, where there isn't end modules
  // at all !!!
  if(db->getTuple()==NULL) return;

  double Y;
  Y = db->fetchDouble("inner_radius")+YZ1H+YZ2H;
  G4int stave_inv [8] = {1,8,7,6,5,4,3,2};
  do {
    double phirot = db->fetchDouble("stave_phi_offset")*pi/180;
    G4RotationMatrix *rot=new G4RotationMatrix();
    double Z = db->fetchDouble("module_z_offset");
    double Xplace = -Y*sin(phirot);
    double Yplace = Y*cos(phirot);
    G4int stave_number = db->fetchInt("stave_id");
    rot->rotateX(pi*0.5); // on couche le module.

    if(Z>0) {
      rot->rotateZ(pi);
      Xplace = - Xplace;
      stave_number = stave_inv[stave_number-1];
    }
    
    rot->rotateY(phirot);
    new MyPlacement(rot,
		    G4ThreeVector(Xplace,
				  Yplace,
				  Z),
		    EnvLogHcalModuleBarrel,
		    "BarrelHcalModule",
		    MotherLog,
		    false,
		    HCALBARREL*100+stave_number*10+db->fetchInt("module_id"));
    theBarrilEndSD->SetStaveRotationMatrix(db->fetchInt("stave_id"),phirot);
    theBarrilEndSD->
      SetModuleZOffset(db->fetchInt("module_id"),
		       db->fetchDouble("module_z_offset"));
  } while(db->getTuple()!=NULL);
}

void Hcal04::BarrelEndChambers(G4LogicalVolume* MotherLog, 
			       double chambers_y_off_correction)
{
  G4LogicalVolume * ChamberLog[200];
  G4Box * ChamberSolid;

  G4VisAttributes *VisAtt = new G4VisAttributes(G4Colour(.2,.8,.2));
  VisAtt->SetForceWireframe(true);

  db->exec("select barrel_regular_layer.layer_id,chamber_dim_x/2. AS xdh,chamber_tickness/2. AS ydh,chamber_dim_z/2. AS zdh, fiber_gap from hcal,barrel_regular_layer,barrel_end_layer where barrel_regular_layer.layer_id = barrel_end_layer.layer_id ;");

  G4UserLimits* pULimits=
    new G4UserLimits(theMaxStepAllowed);
  
  while(db->getTuple()!=NULL)
    {
      ChamberSolid = 
	new G4Box("ChamberSolid", 
		  db->fetchDouble("xdh"),  //hx
		  db->fetchDouble("zdh"),  //hz attention!
		  db->fetchDouble("ydh")); //hy attention!
      
      if(SensitiveModel == "scintillator")
	{
	  //fg: introduce (empty) fiber gap - should be filled with fibres and cables
	  // - so far we fill it  with air ...
	  G4LogicalVolume *ChamberLogical =
	    new G4LogicalVolume(ChamberSolid,
				CGAGeometryManager::GetMaterial("air"), 
				"ChamberLogical", 
				0, 0, 0);
	   
	  double fiber_gap = db->fetchDouble("fiber_gap")  ;
	  double scintHalfWidth =  db->fetchDouble("ydh") - fiber_gap  / 2. ;

	  // fiber gap can't be larger than total chamber
	  assert( scintHalfWidth > 0. ) ;

	   
	  G4Box * ScintSolid = 
	    new G4Box("ScintSolid", 
		      db->fetchDouble("xdh"),  //hx
		      db->fetchDouble("zdh"),  //hz attention!
		      scintHalfWidth ); //hy attention!
	   
	  G4LogicalVolume* ScintLog =
	    new G4LogicalVolume(ScintSolid,
				CGAGeometryManager::GetMaterial("polystyrene"),
				"ScintLogical", 
				0, 0, pULimits);  
	   
	  // only scinitllator is sensitive
	  ScintLog->SetSensitiveDetector(theBarrilEndSD);
	   
	  int layer_id = db->fetchInt("layer_id") ;
	   
	  new MyPlacement(0, G4ThreeVector( 0,0,  - fiber_gap / 2. ), ScintLog,
			  "Scintillator", ChamberLogical, false, layer_id);   

	  ChamberLog [ layer_id ] = ChamberLogical ;
	   
	  // 	   ChamberLog [db->fetchInt("layer_id")] = 
	  // 	     new G4LogicalVolume(ChamberSolid,
	  // 				 CGAGeometryManager::GetMaterial("polystyrene"),
	  // 				 "ChamberLogical", 
	  // 				 0, 0, pULimits);  
	  // 	   ChamberLog[db->fetchInt("layer_id")]->SetSensitiveDetector(theBarrilEndSD);
	}
      else 
	if (SensitiveModel == "RPC1")
	  {
	    G4int layer_id = db->fetchInt("layer_id");
	    ChamberLog [layer_id] =
	      BuildRPC1Box(ChamberSolid,theBarrilEndSD,layer_id,pULimits);
	  }
	else Control::Abort("Invalid sensitive model in the dababase!",MOKKA_ERROR_BAD_DATABASE_PARAMETERS);
      ChamberLog[db->fetchInt("layer_id")]->SetVisAttributes(VisAtt);
    }
   
   
  // End Chamber Placements
  // module x and y offsets (needed for the SD)
  db->exec("select 0 AS module_x_offset, module_y_offset from barrel_module;");
  db->getTuple();
  double Xoff,Yoff;
  Xoff = db->fetchDouble("module_x_offset");
  Yoff = db->fetchDouble("module_y_offset");
   
  db->exec("select barrel_regular_layer.layer_id, 0. as chamber_x_offset,chamber_y_offset,chamber_z_offset as chamber_z_offset,chamber_dim_z/2 AS YHALF,chamber_dim_x/2. as XHALF from barrel_regular_layer,barrel_end_layer where barrel_regular_layer.layer_id = barrel_end_layer.layer_id;");
   
  while(db->getTuple()!=NULL){
    G4int layer_id = db->fetchInt("layer_id");
    new MyPlacement(0,
		    G4ThreeVector(db->fetchDouble("chamber_x_offset"),
				  db->fetchDouble("chamber_z_offset"),
				  db->fetchDouble("chamber_y_offset")+
				  chambers_y_off_correction),
		    //!!attention!! y<->z
		    ChamberLog [db->fetchInt("layer_id")],
		    "ChamberBarrel",
		    MotherLog,false,layer_id);
    theBarrilEndSD->
      AddLayer(layer_id,
	       db->fetchDouble("chamber_x_offset") + 
	       Xoff - db->fetchDouble("XHALF"),
	       db->fetchDouble("chamber_y_offset") + Yoff,
	       - (db->fetchDouble("chamber_z_offset")+
		  db->fetchDouble("YHALF")));
  }
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~              Endcaps                 ~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Hcal04::Endcaps(G4LogicalVolume* MotherLog)
{
  db->exec("select module_radius AS pRMax, module_dim_z/2. AS pDz, center_box_size/2. AS pRMin from endcap_standard_module;");
  db->getTuple();

  double zPlane[2];
  zPlane[0]=-db->fetchDouble("pDz");
  zPlane[1]=-zPlane[0];

  double rInner[2],rOuter[2];
  rInner[0]=rInner[1]=db->fetchDouble("pRMin");
  rOuter[0]=rOuter[1]=db->fetchDouble("pRMax");
  G4Polyhedra *EndCapSolid=
    new G4Polyhedra("HcalEndCapSolid",
		    0.,
		    360.,
		    32,
		    2,
		    zPlane,
		    rInner,
		    rOuter);
		    

  G4VisAttributes *VisAtt = new G4VisAttributes(G4Colour(.8,.8,.2));
  VisAtt->SetForceWireframe(true);
  VisAtt->SetDaughtersInvisible(true);
  //VisAtt->SetForceSolid(true);

  G4LogicalVolume* EndCapLogical =
    new G4LogicalVolume(EndCapSolid,
			RadiatorMaterial,
			"EndCapLogical",
			0, 0, 0);
  EndCapLogical->SetVisAttributes(VisAtt);

  // build and place the chambers in the Hcal Endcaps
  EndcapChambers(EndCapLogical);

  // Placements
   
  db->exec("select endcap_id,endcap_z_offset from endcap;");
   
  G4RotationMatrix *rotEffect=new G4RotationMatrix();
  rotEffect->rotateZ(pi/8.);
  G4int ModuleNumber = HCALENDCAPPLUS*100+16;
  double Z1=0;
  while(db->getTuple()!=NULL){    
    Z1=db->fetchDouble("endcap_z_offset");
    new MyPlacement(rotEffect,
		    G4ThreeVector(0.,
				  0.,
				  Z1),
		    EndCapLogical,
		    "EndCapPhys",
		    MotherLog,
		    false,
		    ModuleNumber);
    rotEffect=new G4RotationMatrix();
    rotEffect->rotateZ(pi/8.);
    rotEffect->rotateY(pi); // On inverse les endcaps 
    ModuleNumber -= (HCALENDCAPPLUS-HCALENDCAPMINUS)*100 + 6;
  }
  theENDCAPEndSD->
    SetModuleZOffset(0,
		     fabs(Z1));
  theENDCAPEndSD->
    SetModuleZOffset(6,
		     fabs(Z1));
}

void Hcal04::EndcapChambers(G4LogicalVolume* MotherLog)
{
  // Chambers in the Hcal04::Endcaps
  // standard endcap chamber solid:
  db->exec("select chamber_radius AS pRMax, chamber_tickness/2. AS pDz, fiber_gap, center_box_size/2. AS pRMin from endcap_standard_module,hcal;");
  db->getTuple();
  
  // G4Polyhedra Envelope parameters
  double phiStart = 0.;
  double phiTotal = 360.;
  G4int numSide = 32;
  G4int numZPlanes = 2;

  double zPlane[2];
  zPlane[0]=-db->fetchDouble("pDz");
  zPlane[1]=-zPlane[0];

  double rInner[2],rOuter[2];
  rInner[0]=rInner[1]=db->fetchDouble("pRMin");
  rOuter[0]=rOuter[1]=db->fetchDouble("pRMax");

  G4Polyhedra *EndCapChamberSolid=
    new G4Polyhedra("EndCapChamberSolid",
		    phiStart,
		    phiTotal,
		    numSide,
		    numZPlanes,
		    zPlane,
		    rInner,
		    rOuter);
		    
  G4UserLimits* pULimits=
    new G4UserLimits(theMaxStepAllowed);
  
  // standard endcap chamber logical
  G4LogicalVolume* EndCapChamberLogical=0;

  if(SensitiveModel == "scintillator")
    {
      //fg: introduce (empty) fiber gap - should be filled with fibres and cables
      // - so far we fill it  with air ...
      EndCapChamberLogical =
	new G4LogicalVolume(EndCapChamberSolid,
			    CGAGeometryManager::GetMaterial("air"), 
			    "EndCapChamberLogical", 
			    0, 0, 0);
	   
      double fiber_gap = db->fetchDouble("fiber_gap")  ;
      double scintHalfWidth =  db->fetchDouble("pDz") - fiber_gap  / 2. ;

      // fiber gap can't be larger than total chamber
      assert( scintHalfWidth > 0. ) ;

	   
      double zPlaneScint[2];
      zPlaneScint[0]=-scintHalfWidth ;
      zPlaneScint[1]=-zPlaneScint[0];

      G4Polyhedra *EndCapScintSolid=
	new G4Polyhedra("EndCapScintSolid",
			phiStart,
			phiTotal,
			numSide,
			numZPlanes,
			zPlaneScint,
			rInner,
			rOuter);

      G4LogicalVolume* ScintLog =
	new G4LogicalVolume(EndCapScintSolid,
			    CGAGeometryManager::GetMaterial("polystyrene"),
			    "EndCapScintLogical", 
			    0, 0, pULimits);  
	   
      // only scinitllator is sensitive
      ScintLog->SetSensitiveDetector(theENDCAPEndSD);
      new MyPlacement(0, G4ThreeVector( 0,0,  - fiber_gap / 2.  ), ScintLog,
		      "EndCapScintillator", EndCapChamberLogical, false, 0 );   
    }
  else 
    if (SensitiveModel == "RPC1")
      {
	EndCapChamberLogical =
	  BuildRPC1Polyhedra(EndCapChamberSolid,
			     theENDCAPEndSD,
			     phiStart,
			     phiTotal,
			     numSide,
			     numZPlanes,
			     zPlane,
			     rInner,
			     rOuter,
			     pULimits);
      }
    else Control::Abort("Invalid sensitive model in the dababase!",MOKKA_ERROR_BAD_DATABASE_PARAMETERS);
  
  G4VisAttributes *VisAtt = new G4VisAttributes(G4Colour(1.,1.,1.));
  EndCapChamberLogical->SetVisAttributes(VisAtt);


  // standard endcap chamber placements
  db->exec("select layer_id,chamber_z_offset AS Zoff from endcap_layer;");
  
  G4int layer_id;
  while(db->getTuple()!=NULL){
    layer_id=db->fetchInt("layer_id");
    new MyPlacement(0,
		    G4ThreeVector(0.,
				  0.,
				  db->fetchDouble("Zoff")),
		    EndCapChamberLogical,
		    "EndCapChamberPhys",
		    MotherLog,false,layer_id);
    theENDCAPEndSD->
      AddLayer(layer_id,
	       0,
	       0,
	       db->fetchDouble("Zoff"));
  }  
}

G4LogicalVolume * 
Hcal04::BuildRPC1Box(Box box, 
		     SD* theSD, 
		     G4int layer_id,
		     G4UserLimits* pULimits)
{
  // fill the Chamber Envelope with G10
  Volume vol(name+"_rpc",box,lcdd.material("G10"));
	     
  // build the RPC glass !!attention!! y<->z
  Box    glassBox(box.x(),box.y(),glass_thickness/2.0);
  Volume glassVol(name+"_rpcGlass",glassBox,lcdd.material("pyrex"));

      
  G4VisAttributes * VisAtt = new G4VisAttributes(G4Colour(.8,0,.2));
  VisAtt->SetForceWireframe(true);
  //VisAtt->SetForceSolid(true);
  GlassLogical->SetVisAttributes(VisAtt);
      
  //
  // build the standard spacer
  //!!attention!! y<->z
  G4Box * SpacerSolid =
    new G4Box("RPC1Spacer", 
	      ChamberSolid->GetXHalfLength(),
	      spacer_thickness/2.,
	      gas_thickness/2.);
      
  G4LogicalVolume *SpacerLogical =
    new G4LogicalVolume(SpacerSolid,
			CGAGeometryManager::GetMaterial("g10"),
			"RPC1Spacer", 
			0, 0, 0);

  VisAtt = new G4VisAttributes(G4Colour(1,1.1));
  VisAtt->SetForceWireframe(true);
  //VisAtt->SetForceSolid(true);
  SpacerLogical->SetVisAttributes(VisAtt);

  //
  // build the gas gap
  //
  G4Box * GasSolid =
    new G4Box("RPC1Gas", 
	      ChamberSolid->GetXHalfLength(),
	      ChamberSolid->GetYHalfLength(),
	      gas_thickness/2.);
      
  G4LogicalVolume *GasLogical =
    new G4LogicalVolume(GasSolid,
			CGAGeometryManager::GetMaterial("RPCGAS1"),
			"RPC1gas", 
			0, 
			0, 
			pULimits);
      
  VisAtt = new G4VisAttributes(G4Colour(.1,0,.8));
  VisAtt->SetForceWireframe(true);
  //VisAtt->SetForceSolid(true);
  GasLogical->SetVisAttributes(VisAtt);

  // PLugs the sensitive detector HERE!
  GasLogical->SetSensitiveDetector(theSD);
      
  // placing the spacers inside the gas gap
  double NextY = 
    - ChamberSolid->GetYHalfLength() + spacer_thickness/2.;
  while ( NextY < ChamberSolid->GetYHalfLength()-spacer_thickness/2.)
    {
      new MyPlacement(0,
		      G4ThreeVector(0,
				    NextY,
				    0),
		      SpacerLogical,
		      "RPCSpacer",
		      GasLogical,
		      false,
		      0);
      NextY += spacer_gap;
    }

  // placing the all. 
  // ZOffset starts pointing to the chamber border.
  double ZOffset = - ChamberSolid->GetZHalfLength();

  // first glass border after the g10_thickness
  ZOffset += g10_thickness + glass_thickness/2.;

  new MyPlacement(0,
		  G4ThreeVector(0,
				0,
				ZOffset),
		  GlassLogical,
		  "RPCGlass",
		  ChamberLog,
		  false,
		  0);
      
  // set ZOffset to the next first glass border
  ZOffset += glass_thickness/2.;
      
  // gas gap placing 
  ZOffset += gas_thickness/2.; // center !

  new MyPlacement(0,
		  G4ThreeVector(0,
				0,
				ZOffset),
		  GasLogical,
		  "RPCGas",
		  ChamberLog,
		  false,
		  layer_id);   // layer_id is set up here!
      
  // set ZOffset to the next gas gap border
  ZOffset += gas_thickness/2.;
      
  // second glass.
  ZOffset += glass_thickness/2.; // center !

  new MyPlacement(0,
		  G4ThreeVector(0,
				0,
				ZOffset),
		  GlassLogical,
		  "RPCGlass",
		  ChamberLog,
		  false,
		  0);
  return ChamberLog;
}

Control::Abort("Hcal04::BuildRPC1Box: invalid ChamberSolidEnvelope",MOKKA_OTHER_ERRORS);
return NULL;
}

G4LogicalVolume * 
Hcal04::BuildRPC1Polyhedra(G4Polyhedra* ChamberSolid, 
			   SD* theSD,
			   double phiStart,
			   double phiTotal,
			   G4int numSide,
			   G4int numZPlanes,
			   const double zPlane[],
			   const double rInner[],
			   const double rOuter[],
			   G4UserLimits* pULimits)
{
  if(ChamberSolid->GetEntityType()=="G4Polyhedra")
    {
      // fill the Chamber Envelope with G10
      G4LogicalVolume *ChamberLog =
	new G4LogicalVolume(ChamberSolid,
			    CGAGeometryManager::GetMaterial("g10"),
			    "RPC1", 
			    0, 0, 0);
      //
      // build the RPC glass 
      
      double NewZPlane[2];
      NewZPlane[0] = glass_thickness/2.;
      NewZPlane[1] = -NewZPlane[0];

      G4Polyhedra * GlassSolid =
	new G4Polyhedra("RPC1Glass", 
			phiStart,
			phiTotal,
			numSide,
			numZPlanes,
			NewZPlane,
			rInner,
			rOuter);	
      
      G4LogicalVolume *GlassLogical =
	new G4LogicalVolume(GlassSolid,
			    CGAGeometryManager::GetMaterial("pyrex"),
			    "RPC1glass", 
			    0, 0, 0);
      
      G4VisAttributes * VisAtt = new G4VisAttributes(G4Colour(.8,0,.2));
      VisAtt->SetForceWireframe(true);
      //VisAtt->SetForceSolid(true);
      GlassLogical->SetVisAttributes(VisAtt);

      //
      // build the gas gap
      //
      NewZPlane[0] = gas_thickness/2.;
      NewZPlane[1] = -NewZPlane[0];
      G4Polyhedra * GasSolid =
	new G4Polyhedra("RPC1Gas", 
			phiStart,
			phiTotal,
			numSide,
			numZPlanes,
			NewZPlane,
			rInner,
			rOuter);
      
      G4LogicalVolume *GasLogical =
	new G4LogicalVolume(GasSolid,
			    CGAGeometryManager::GetMaterial("RPCGAS1"),
			    "RPC1gas", 
			    0, 0, pULimits);
      
      VisAtt = new G4VisAttributes(G4Colour(.1,0,.8));
      VisAtt->SetForceWireframe(true);
      //VisAtt->SetForceSolid(true);
      GasLogical->SetVisAttributes(VisAtt);

      // PLugs the sensitive detector HERE!
      GasLogical->SetSensitiveDetector(theSD);

      //
#ifdef MOKKA_GEAR
      // get heighth of sensible part of layer for each layer
      helpEndcap.sensThickness.push_back( gas_thickness ) ;
      helpEndcap.gapThickness.push_back( spacer_thickness ) ;
#endif
      
      // placing the all. 
      // ZOffset starts pointing to the chamber border.
      double ZOffset = zPlane[0];
      
      // first glass after the g10_thickness
      ZOffset += g10_thickness + glass_thickness/2.;
      new MyPlacement(0,
		      G4ThreeVector(0,
				    0,
				    ZOffset),
		      GlassLogical,
		      "RPCGlass",
		      ChamberLog,
		      false,
		      0);
      
      // set ZOffset to the next first glass border
      ZOffset += glass_thickness/2.;
      
      // gas gap placing 
      ZOffset += gas_thickness/2.; // center !
      new MyPlacement(0,
		      G4ThreeVector(0,
				    0,
				    ZOffset),
		      GasLogical,
		      "RPCGas",
		      ChamberLog,
		      false,
		      0);

      // set ZOffset to the next gas gap border
      ZOffset += gas_thickness/2.;
      
      // second glass, after the g10_thickness, the first glass
      // and the gas gap.
      ZOffset += glass_thickness/2.; // center !
      new MyPlacement(0,
		      G4ThreeVector(0,
				    0,
				    ZOffset),
		      GlassLogical,
		      "RPCGlass",
		      ChamberLog,
		      false,
		      0);
      return ChamberLog;      
    }
  Control::Abort("Hcal04::BuildRPC1Polyhedra: invalid ChamberSolidEnvelope",MOKKA_OTHER_ERRORS);
  return NULL;  
#endif
  lcdd.pickMotherVolume(sdet).placeVolume(assembly);
  return sdet;
}

DECLARE_DETELEMENT(Tesla_hcal04,create_element);