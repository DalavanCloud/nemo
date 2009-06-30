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
#include <sstream>
#include "snapshotgadget.h"

namespace glnemo {

SnapshotGadget::SnapshotGadget():SnapshotInterface()
{
  valid=false;
  part_data = new ParticlesData();
  gadget_io = NULL;
  interface_type="Gadget";
}

// ============================================================================
// Destructor                                                                  
SnapshotGadget::~SnapshotGadget()
{
  if (obj)       delete obj;
  if (part_data) delete part_data;
  if (valid) close();
  if (gadget_io) delete gadget_io;
}
// ============================================================================
// newObject                                                                   
// instantiate a new object and return a pointer on it                         
SnapshotInterface * SnapshotGadget::newObject(const std::string _filename)
{
  filename = _filename;
  obj      = new SnapshotGadget();
  obj->setFileName(_filename);

  return obj;
}
// ============================================================================
// isValidData()                                                               
// return true if it's a Gadget snapshot.                                      
bool SnapshotGadget::isValidData()
{
  valid=false;
  gadget_io = new gadget::GadgetIO(filename);
  int fail = gadget_io->open(filename);
  if (!fail) {
    valid=true;
    std::ostringstream stm;
    stm << gadget_io->getVersion();
    interface_type += " " + stm.str();
  }

  return valid; 
}
// ============================================================================
// getSnapshotRange                                                            
ComponentRangeVector * SnapshotGadget::getSnapshotRange()
{
  crv.clear();
  if (valid) {
    crv = gadget_io->getCRV();
    ComponentRange::list(&crv);
    if (first) {
      first       = false;
      crv_first   = crv;
      nbody_first = gadget_io->getNtotal();
      time_first  = gadget_io->getTime();
    }
  }
  return &crv;
}
// ============================================================================
// initLoading()                                                               
int SnapshotGadget::initLoading(const bool _load_vel, const std::string _select_time)
{
  load_vel = _load_vel;
  select_time = _select_time;
  return 1;
}
// ============================================================================
// nextFrame()                                                                 
int SnapshotGadget::nextFrame(const int * index_tab, const int nsel)
{
  if (valid) {
    if (nsel > *part_data->nbody) {
      //pos
      if (part_data->pos) delete [] part_data->pos;
      part_data->pos = new float[nsel*3];
      //vel
      if (load_vel) {
        if (part_data->vel) delete [] part_data->vel;
        part_data->vel = new float[nsel*3];
      }
      //rho
      if (part_data->rho) delete [] part_data->rho;
      part_data->rho = new float[nsel];
      for (int i=0; i<nsel; i++) part_data->rho[i]=-1.;
      //rneib
      if (part_data->rneib) delete [] part_data->rneib;
      part_data->rneib = new float[nsel];
      for (int i=0; i<nsel; i++) part_data->rneib[i]=-1.;
      //temp
      if (part_data->temp) delete [] part_data->temp;
      part_data->temp = new float[nsel];
      for (int i=0; i<nsel; i++) part_data->temp[i]=-1.;
    }
    *part_data->nbody = nsel;
#if 1
    if (gadget_io->read2(part_data->pos,part_data->vel,part_data->rho, part_data->rneib,part_data->temp,
	index_tab,nsel,load_vel)) {
#else
    if (gadget_io->read(part_data->pos,part_data->vel,part_data->rho, part_data->rneib,
        index_tab,nsel,load_vel)) {

#endif
      part_data->computeVelNorm();
      part_data->computeMinMaxRho();
      part_data->computeMinMaxTemp();
      if ( ! part_data->isRhoValid() ) {
        if (part_data->rho) delete [] part_data->rho;
        part_data->rho=NULL;
        if (part_data->rneib) delete [] part_data->rneib;
        part_data->rneib=NULL;
      }
      if (! part_data->timu ) part_data->timu = new float;
      *part_data->timu = gadget_io->getTime();
      end_of_data=true; // only one frame from an gadget snapshot
    }
    else {
      end_of_data=true;
    }
  }
  return 1;
}
// ============================================================================
// close()                                                                     
int SnapshotGadget::close()
{
  gadget_io->close();
  end_of_data=false;
  return 1;
}
// ============================================================================
// endendOfDataMessage()                                                       
QString SnapshotGadget::endOfDataMessage()
{
  QString message=tr("Gadget Snapshot [")+QString(filename.c_str())+tr("] end of snapshot reached!");
  return message;
}
}
// You have to export outside of the namespace "glnemo"
// BUT you have to specify the namespace in the export:
// ==> glnemo::SnapshotGadget                          

Q_EXPORT_PLUGIN2(gadgetplugin, glnemo::SnapshotGadget);
