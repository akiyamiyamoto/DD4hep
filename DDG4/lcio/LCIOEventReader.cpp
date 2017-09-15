//==========================================================================
//  AIDA Detector description implementation 
//--------------------------------------------------------------------------
// Copyright (C) Organisation europeenne pour la Recherche nucleaire (CERN)
// All rights reserved.
//
// For the licensing terms see $DD4hepINSTALL/LICENSE.
// For the list of contributors see $DD4hepINSTALL/doc/CREDITS.
//
// @author  P.Kostka (main author)
// @author  M.Frank  (code reshuffeling into new DDG4 scheme)
//
//====================================================================

// Framework include files
#include "LCIOEventReader.h"
#include "DD4hep/Printout.h"
#include "DDG4/Geant4Primary.h"
#include "DDG4/Geant4Context.h"
#include "DDG4/Factories.h"

#include "G4ParticleTable.hh"
#include "EVENT/MCParticle.h"
#include "EVENT/LCCollection.h"

#include "G4Event.hh"

#include <fstream>
#include <sstream>
#include <string>

using namespace std;
using namespace dd4hep;
using namespace dd4hep::sim;
typedef dd4hep::detail::ReferenceBitMask<int> PropertyMask;

// Neede for backwards compatibility:
namespace dd4hep{namespace sim{typedef Geant4InputAction LCIOInputAction;}}
DECLARE_GEANT4ACTION(LCIOInputAction)


namespace {
  inline int GET_ENTRY(const map<EVENT::MCParticle*,int>& mcparts, EVENT::MCParticle* part)  {
    map<EVENT::MCParticle*,int>::const_iterator ip=mcparts.find(part);
    if ( ip == mcparts.end() )  {
      throw runtime_error("Unknown particle identifier look-up!");
    }
    return (*ip).second;
  }
}


/// Initializing constructor
LCIOEventReader::LCIOEventReader(const string& nam)
  : Geant4EventReader(nam)
{
}

/// Default destructor
LCIOEventReader::~LCIOEventReader()   {
}

/// A file containing IP smearing parameters
LCIOEventReader::EventReaderStatus
LCIOEventReader::setParameters(  std::map< std::string, std::string > & parameters ) {

  _getParameterValue( parameters, "ipSmearFile", m_ip_smear_file, std::string(""));

  if ( m_ip_smear_file.size() <= 0 )
    return EVENT_READER_OK;
 
  printout(INFO, "LCIOReader", "--- will get IP smear parameters from %s ", m_ip_smear_file.c_str());
  
  std::ifstream ipsmearFile( m_ip_smear_file.c_str(), std::ifstream::in );
  if ( !ipsmearFile.is_open() ){
    except("Could not open IP smear file: %s", m_ip_smear_file.c_str() );
    return EVENT_READER_ERROR;
  }
  printout(INFO, "LCIOReader", "--- opened IP smear file: %s", m_ip_smear_file.c_str());
  while ( !ipsmearFile.eof() ) {
    // read line
    std::string linebuf;
    getline( ipsmearFile, linebuf );

    // ignore comments
    if (linebuf.substr(0,1) == "#") continue;

    // ignore empty lines
    if (linebuf.empty()) continue;
   
    // parse line
    std::string nam;
    std::vector<double> dxsx(6,0);
    std::istringstream istr(linebuf);
    istr >> nam >> dxsx[0] >> dxsx[1] >> dxsx[2] >> dxsx[3] >> dxsx[4] >> dxsx[5] ;

    m_ip_smear_data[nam] = dxsx ;

    printout(INFO, "LCIOReader", "--- IP smear parameter %s : Offset(%g,%g,%g), Sigma(%g,%g,%g) in mm unit.",nam.c_str(),
                   dxsx[0], dxsx[1], dxsx[2],dxsx[3],dxsx[4] ,dxsx[5] ) ;
  }

  return EVENT_READER_OK;
}


/// Read an event and fill a vector of MCParticles.
LCIOEventReader::EventReaderStatus
LCIOEventReader::readParticles(int event_number, 
                               Vertices& vertices,
                               vector<Particle*>& particles)
{
  EVENT::LCCollection*        primaries = 0;
  map<EVENT::MCParticle*,int> mcparts;
  vector<EVENT::MCParticle*>  mcpcoll;
  EventReaderStatus ret = readParticleCollection(event_number,&primaries);

  if ( ret != EVENT_READER_OK ) return ret;

  int NHEP = primaries->getNumberOfElements();
  // check if there is at least one particle
  if ( NHEP == 0 ) return EVENT_READER_NO_PRIMARIES;

  //fg: for now we create exactly one event vertex here ( as before )
  Geant4Vertex* vtx = new Geant4Vertex ;
  vertices.push_back( vtx );
  vtx->x = 0;
  vtx->y = 0;
  vtx->z = 0;
  vtx->time = 0;
  //  bool haveVertex = false ;


  mcpcoll.resize(NHEP,0);
  for(int i=0; i<NHEP; ++i ) {
    EVENT::MCParticle* p = dynamic_cast<EVENT::MCParticle*>(primaries->getElementAt(i));
    mcparts[p] = i;
    mcpcoll[i] = p;
  }

  // build collection of MCParticles
  for(int i=0; i<NHEP; ++i )   {
    EVENT::MCParticle* mcp = mcpcoll[i];
    Geant4ParticleHandle p(new Particle(i));
    const double *mom   = mcp->getMomentum();
    const double *vsx   = mcp->getVertex();
    const double *vex   = mcp->getEndpoint();
    const float  *spin  = mcp->getSpin();
    const int    *color = mcp->getColorFlow();
    const int     pdg   = mcp->getPDG();
    PropertyMask status(p->status);
    p->pdgID        = pdg;
    p->charge       = int(mcp->getCharge()*3.0);
    p->psx          = mom[0]*CLHEP::GeV;
    p->psy          = mom[1]*CLHEP::GeV;
    p->psz          = mom[2]*CLHEP::GeV;
    p->time         = mcp->getTime()*CLHEP::ns;
    p->properTime   = mcp->getTime()*CLHEP::ns;
    p->vsx          = vsx[0]*CLHEP::mm;
    p->vsy          = vsx[1]*CLHEP::mm;
    p->vsz          = vsx[2]*CLHEP::mm;
    p->vex          = vex[0]*CLHEP::mm;
    p->vey          = vex[1]*CLHEP::mm;
    p->vez          = vex[2]*CLHEP::mm;
    p->process      = 0;
    p->spin[0]      = spin[0];
    p->spin[1]      = spin[1];
    p->spin[2]      = spin[2];
    p->colorFlow[0] = color[0];
    p->colorFlow[0] = color[1];
    p->mass         = mcp->getMass()*CLHEP::GeV;
    const EVENT::MCParticleVec &par = mcp->getParents(), &dau=mcp->getDaughters();
    for(int num=dau.size(),k=0; k<num; ++k)
      p->daughters.insert(GET_ENTRY(mcparts,dau[k]));
    for(int num=par.size(),k=0; k<num; ++k)
      p->parents.insert(GET_ENTRY(mcparts,par[k]));

    int genStatus = mcp->getGeneratorStatus();
    if ( genStatus == 0 ) status.set(G4PARTICLE_GEN_EMPTY);
    else if ( genStatus == 1 ) status.set(G4PARTICLE_GEN_STABLE);
    else if ( genStatus == 2 ) status.set(G4PARTICLE_GEN_DECAYED);
    else if ( genStatus == 3 ) status.set(G4PARTICLE_GEN_DOCUMENTATION);
    else if ( genStatus == 4 ) status.set(G4PARTICLE_GEN_BEAM);
    else if ( genStatus == 5 ) status.set(G4PARTICLE_GEN_HARDPROCESS);
    else {
      cout << " #### WARNING - LCIOInputAction : unknown generator status : "
           << genStatus << " -> ignored ! " << endl;
    }

    //fixme: need to define the correct logic for selecting the particle to use
    //       for the _one_ event vertex 
    // fill vertex information from first stable particle
    // if( !haveVertex &&  genStatus == 1 ){
    //   vtx->x = p->vsx ;
    //   vtx->y = p->vsy ;
    //   vtx->z = p->vsz ;
    //   vtx->time = p->time ;
    //   haveVertex = true ;
    // }

    if ( p->parents.size() == 0 )  {
      if ( status.isSet(G4PARTICLE_GEN_EMPTY) || status.isSet(G4PARTICLE_GEN_DOCUMENTATION) )
	      vtx->in.insert(p->id);  // Beam particles and primary quarks etc
      else
        vtx->out.insert(p->id); // Stuff, to be given to Geant4 together with daughters
    }
    else if ( status.isSet(G4PARTICLE_GEN_BEAM) || status.isSet(G4PARTICLE_GEN_HARDPROCESS) ) { // primary quarks etc of Whizard2
      vtx->in.insert(p->id);
    }
    
    if ( mcp->isCreatedInSimulation() )       status.set(G4PARTICLE_SIM_CREATED);
    if ( mcp->isBackscatter() )               status.set(G4PARTICLE_SIM_BACKSCATTER);
    if ( mcp->vertexIsNotEndpointOfParent() ) status.set(G4PARTICLE_SIM_PARENT_RADIATED);
    if ( mcp->isDecayedInTracker() )          status.set(G4PARTICLE_SIM_DECAY_TRACKER);
    if ( mcp->isDecayedInCalorimeter() )      status.set(G4PARTICLE_SIM_DECAY_CALO);
    if ( mcp->hasLeftDetector() )             status.set(G4PARTICLE_SIM_LEFT_DETECTOR);
    if ( mcp->isStopped() )                   status.set(G4PARTICLE_SIM_STOPPED);
    if ( mcp->isOverlay() )                   status.set(G4PARTICLE_SIM_OVERLAY);
    particles.push_back(p);
  }
  
  // Smear IP vertex point here, if smear file is given
  if ( m_ip_smear_file.size() <= 0 )
    return EVENT_READER_OK;

  EVENT::LCParameters* evt_param = getEventParameters();
  float energy=evt_param.getFloatVal("Energy");
  std::string beamspectrum= evt_param.getStringVal("BeamSpectrum");
  int beamPDG0=evt_param.getIntVal("beamPDG0");
  int beamPDG1=evt_param.getIntVal("beamPDG1");
  std::sstringstream beamkey_stream;
  beamkey_stream << int(energy) << "/" << beamspectrum << "/" << beamPDG0 << "/" << beamPDG1 ;
  std::string beamkey = beamkey_stream.str();  
  
  printout(INFO, "LCIOReader", "--- LCIO Event header : Energy(%g), BeamSpectrum(%s), beamPDG0(%d), beamPDG1(%d)",
               energy, beamspectrum, beamPDG0, beamPDG1);
  printout(INFO, "LCIOReader", "--- IPSmearpara search key(%s)",beamkey.c_str());

  IP_Smear_Data::iterator iterkey = m_ip_smear_data.find(beamkey);
  if ( iterkey == m_ip_smear_data.end() ) {
    except("Could not find beamkey for IP smearing. beamkey was %s", beamkey.c_str() );
    return EVENT_READER_ERROR;
  }
  std::vector<double> dxsx = iterkey.second ;

  std::cout << dxsx[0] << "," << dxsx[1] << "," << dxsx[2] ;
  std::cout << dxsx[3] << "," << dxsx[4] << "," << dxsx[5] << std::endl;


  
    
  


  return EVENT_READER_OK;
}

