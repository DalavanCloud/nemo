// ============================================================================
// Copyright Jean-Charles LAMBERT - 2004                                       
// e-mail:   Jean-Charles.Lambert@oamp.fr                                      
// address:  Dynamique des galaxies                                            
//           Laboratoire d'Astrophysique de Marseille                          
//           2, place Le Verrier                                               
//           13248 Marseille Cedex 4, France                                   
//           CNRS U.M.R 6110                                                   
// ============================================================================
// See the complete license in LICENSE and/or "http://www.cecill.info".        
// ============================================================================
//                                                                             
// VirtualData class implementation                                            
//                                                                             
// Virtual class to implement all the different kind of data which can be      
// rendered with the OpenGL viewer                                             
// ============================================================================
#ifndef VIRTUAL_DATA_H
#define VIRTUAL_DATA_H
#include <iostream>
#include <qobject.h>
#include "particles_range.h"

class VirtualData : public QObject
{ 
  Q_OBJECT
  public:
  VirtualData() { is_end_of_data=FALSE;};
  ~VirtualData() { cerr << "inside ~VirtualData\n";};
  virtual int loadPos(ParticlesRangeVector *); 
  virtual int getNbody();
  virtual float * getPos();
  virtual float  getTime();
  virtual bool isValidData();
  virtual float * getCooMax();
  virtual int * getCooIndexMax();
  virtual void uploadGlData(ParticlesRangeVector *);
  virtual QString endOfDataMessage();
  virtual const char * getDataName();
  virtual const char * getDataType();
  virtual void setSelectedRange(const QString) { };
  virtual int fillParticleRange(ParticlesRangeVector * prv,const int nbody, const char * sel2);
  virtual bool isConnected() { return FALSE; };
  virtual int reload(ParticlesRangeVector *) { cerr << "reload not implemented\n"; return 0;};
  bool is_loading_thread;
  bool is_end_of_data;
  
  // 
  enum Status {
    EndOfNemo
  };
  
  signals:
  virtual void messageLoad(QString * );
  
  protected:
  int full_nbody;
  float * pos, * vel, * timu, coo_max[3];
  int   * nbody,i_max[3];
  virtual void computeCooMax();
  private:

};


#endif // VIRTUAL_DATA_H