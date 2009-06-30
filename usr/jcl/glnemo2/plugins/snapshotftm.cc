// ============================================================================
// Copyright Jean-Charles LAMBERT - 2007-2008                                  
// e-mail:   Jean-Charles.Lambert@oamp.fr                                      
// address:  Dynamique des galaxies                                            
//           Laboratoire d'Astrophysique de Marseille                          
//           P�le de l'Etoile, site de Ch�teau-Gombert                         
//           38, rue Fr�d�ric Joliot-Curie                                     
//           13388 Marseille cedex 13 France                                   
//           CNRS U.M.R 6110                                                   
// ============================================================================
// See the complete license in LICENSE and/or "http://www.cecill.info".        
// ============================================================================
#include <QtGui>
#include "snapshotftm.h"
#include "particlesdata.h"

namespace glnemo {

using namespace ftm;

// ============================================================================
// Constructor                                                                 
SnapshotFtm::SnapshotFtm():SnapshotInterface()
{
  valid=false;
  part_data = new ParticlesData();
  ftm_io = NULL;
  interface_type="Ftm";
}
// ============================================================================
// Destructor                                                                  
SnapshotFtm::~SnapshotFtm()
{
  if (obj)       delete obj;
  if (part_data) delete part_data;
  if (valid) close();
  if (ftm_io)    delete ftm_io;
}
// ============================================================================
// newObject                                                                   
// instantiate a new object and return a pointer on it                         
SnapshotInterface * SnapshotFtm::newObject(const std::string _filename)
{
  filename = _filename;
  obj      = new SnapshotFtm();
  obj->setFileName(_filename);

  return obj;
}
// ============================================================================
// isValidData()                                                               
// return true if it's a Ftm snapshot.                                         
bool SnapshotFtm::isValidData()
{
  valid=false;
  if (isFileExist()) {
    ftm_io = new FtmIO(filename);
    int fail = ftm_io->open();
    if (!fail) valid=true;
  }
  return valid; 
}
// ============================================================================
// getSnapshotRange                                                            
ComponentRangeVector * SnapshotFtm::getSnapshotRange()
{
  crv.clear();
  if (valid) {
    ComponentRange * cr = new ComponentRange();
    cr->setData(0,ftm_io->getNtotal()-1);
    cr->setType("all");
    crv.push_back(*cr);
    const FtmComponent * comp = ftm_io->getComp();
    //std::cerr << "Halo.type = " << comp->halo.type << "\n";
    if ( comp->halo.type == "halo") crv.push_back(comp->halo);
    if ( comp->disk.type == "disk") crv.push_back(comp->disk);
    if ( comp->gas.type  == "gas" ) crv.push_back(comp->gas);
    //ComponentRange::list(&crv);
    if (first) {
      first       = false;
      crv_first   = crv;
      nbody_first = ftm_io->getNtotal();
      time_first  = ftm_io->getTime();
    }
    delete cr;
  }
  return &crv;
}
// ============================================================================
// initLoading()                                                               
int SnapshotFtm::initLoading(const bool _load_vel, const std::string _select_time)
{
  load_vel = _load_vel;
  select_time = _select_time;
  return 1;
}
// ============================================================================
// nextFrame()                                                                 
int SnapshotFtm::nextFrame(const int * index_tab, const int nsel)
{
  if (valid) {
  if (ftm_io->read(part_data,index_tab,nsel,load_vel)) {
    part_data->computeVelNorm();
    part_data->computeMinMaxRho();
    end_of_data=true; // only one frame from an ftm snapshot
  }
    else {
      end_of_data=true;
    }
  }
  return 1;
}
// ============================================================================
// close()                                                                     
int SnapshotFtm::close()
{
  ftm_io->close();
  end_of_data=false;
  return 1;
}
// ============================================================================
// endendOfDataMessage()                                                       
QString SnapshotFtm::endOfDataMessage()
{
  QString message=tr("Ftm Snapshot [")+QString(filename.c_str())+tr("] end of snapshot reached!");
  return message;
}
}
// You have to export outside of the namespace "glnemo"
// BUT you have to specify the namespace in the export:
// ==> glnemo::SnapshotFtm                             

Q_EXPORT_PLUGIN2(ftmplugin, glnemo::SnapshotFtm);
