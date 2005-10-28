//-----------------------------------------------------------------------------+
//                                                                             |
// nbody.cc                                                                    |
//                                                                             |
// Copyright (C) 2000-2005  Walter Dehnen                                      |
//                                                                             |
// This program is free software; you can redistribute it and/or modify        |
// it under the terms of the GNU General Public License as published by        |
// the Free Software Foundation; either version 2 of the License, or (at       |
// your option) any later version.                                             |
//                                                                             |
// This program is distributed in the hope that it will be useful, but         |
// WITHOUT ANY WARRANTY; without even the implied warranty of                  |
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU           |
// General Public License for more details.                                    |
//                                                                             |
// You should have received a copy of the GNU General Public License           |
// along with this program; if not, write to the Free Software                 |
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                   |
//                                                                             |
//-----------------------------------------------------------------------------+
#include <public/nbody.h>
#include <public/inline.h>
#include <public/io.h>
#include <iomanip>
using namespace falcON;
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// class falcON::Integrator                                                   //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
Integrator::Integrator(const ForceAndDiagnose*S,
		       fieldset p, fieldset k, fieldset r,
		       fieldset P, fieldset K, fieldset R) falcON_THROWING :
  // all quantities remembered must also be predicted
  predALL     ((S->requires()&fieldset::w ? p | fieldset::w : p) | r),
  kickALL     ( S->requires()&fieldset::w ? k | fieldset::v : k ),
  rembALL     ( S->requires()&fieldset::w ? r | fieldset::w : r ),
  // don't do to SPH what has already been done for all
  predSPH     ((P|R) &~ predALL ),
  kickSPH     ( K    &~ kickALL ),
  rembSPH     ( R    &~ rembALL ),
  SOLVER      ( S ),
  C_OLD       ( clock() ),
  CPU_TOTAL   ( 0. )
{
  // sanity check prediction, kick, and rembembrance settings
  char     comp[32];
  fieldset test;
  if( (test=predALL & ~fieldset(fieldset::x|fieldset::w)) ) 
    warning("Integration: will not predict '%s'", test.make_word(comp));
  if( (test=kickALL & ~fieldset(fieldset::v)) )
    warning("Integration: will not kick '%s'", test.make_word(comp));
  if( (test=rembALL & ~fieldset(fieldset::w)) )
    warning("Integration: will not remember '%s'", test.make_word(comp));
  if( predALL & fieldset::w && !(kickALL & fieldset::v) )
    falcON_THROW("Integration: cannot predict w without kicking v");
  if( predALL & fieldset::x && !(kickALL & fieldset::v) )
    falcON_THROW("Integration: request to predict x without kicking v");
  requALL = 
    kickALL & fieldset::v ? fieldset::a : fieldset::o |
    predALL & fieldset::w ? fieldset::a : fieldset::o;
#ifdef falcON_SPH
  if( (test=predSPH & ~fieldset(fieldset::x|fieldset::H|fieldset::R|
				fieldset::Y|fieldset::V)) ) 
    warning("Integration: will not predict '%s'", test.make_word(comp));
  if( (test=kickSPH & ~fieldset(fieldset::U|fieldset::v)) )
    warning("Integration: will not kick '%s'", test.make_word(comp));
  if( (test=rembSPH & ~fieldset(fieldset::V|fieldset::Y)) )
    warning("Integration: will not remember '%s'", test.make_word(comp));
  if( predSPH & fieldset::V && !((kickSPH|kickALL) & fieldset::v) )
    falcON_THROW("Integration: cannot predict V without kicking v");
  if( predSPH & fieldset::Y && !(kickSPH & fieldset::U) )
    falcON_THROW("Integration: cannot predict Y without kicking U");
  if( predSPH & fieldset::x && !((kickSPH|kickALL) & fieldset::v) )
    falcON_THROW("Integration: request to predict x without kicking v");
  requSPH =
    predSPH & fieldset::H ? fieldset::J : fieldset::o |
    predSPH & fieldset::V ? fieldset::a : fieldset::o |
    predSPH & fieldset::R ? fieldset::D : fieldset::o |
    kickSPH & fieldset::U ? fieldset::I : fieldset::o;
#endif
  // sanity check requirements and delivieries
  reset_CPU();
  if(! SOLVER->computes().contain(required()))
    falcON_THROW("Integrator requires '%s', but ForceSolver computes '%s'",
		 word(required()),word(S->computes()));
  fieldset full = SOLVER->computes() | predicted() | fieldset::m |
                  rembALL | kickALL;
  if(! full.contain(SOLVER->requires()) )
    falcON_THROW("ForceAndDiagnose requires '%s', but code delivers only '%s'",
		 word(S->requires()), word(full));
  if(! SOLVER->computesSPH().contain(requiredSPH()))
    falcON_THROW("SPH: Integrator requires '%s', but ForceSolver computes '%s'",
		 word(requiredSPH()), word(S->computesSPH()));
  full |= SOLVER->computesSPH() | predictedSPH() | rembSPH | kickSPH;
  if(! full.contain(SOLVER->requiresSPH()) )
    falcON_THROW("SPH: ForceAndDiagnose requires '%s', "
		 "but code delivers only '%s'",
		 word(S->requiresSPH()), word(full));
  // make sure snapshot supports all data required
  fieldset need = p | k | r | P | K | R | fieldset::f;
  need |= SOLVER->computes() | SOLVER->computesSPH();
  snap_shot()->add_fields(need);
}
////////////////////////////////////////////////////////////////////////////////
namespace {
  //----------------------------------------------------------------------------
  template<int TYPE, int DERIV>
  inline void move_all(const bodies*B, double dt, bool all) {
    if(all)
      LoopAllBodies(B,b) {
	b. template datum<TYPE>() += dt * const_datum<DERIV>(b);
      }
    else
      LoopAllBodies(B,b) if(is_active(b)) {
	b. template datum<TYPE>() += dt * const_datum<DERIV>(b);
      }
  }
  //----------------------------------------------------------------------------
  template<int TYPE, int DERIV>
  inline void move_all_i(const bodies*B, const double*dt, bool all) {
    if(all)
      LoopAllBodies(B,b) {
	b. template datum<TYPE>() += dt[level(b)] * const_datum<DERIV>(b);
      }
    else
      LoopAllBodies(B,b) if(is_active(b)) {
	b. template datum<TYPE>() += dt[level(b)] * const_datum<DERIV>(b);
      }
  }
  //----------------------------------------------------------------------------
  template<int TYPE, int DERIV>
  inline void move_sph(const bodies*B, double dt, bool all) {
    if(all)
      LoopSPHBodies(B,b)
	b. template datum<TYPE>() += dt * const_datum<DERIV>(b);
    else
      LoopSPHBodies(B,b) if(is_active(b))
	b. template datum<TYPE>() += dt * const_datum<DERIV>(b);
  }
  //----------------------------------------------------------------------------
  template<int TYPE, int DERIV>
  inline void move_sph_i(const bodies*B, const double*dt, bool all) {
    if(all)
      LoopSPHBodies(B,b)
	b. template datum<TYPE>() += dt[level(b)] * const_datum<DERIV>(b);
    else
      LoopSPHBodies(B,b) if(is_active(b))
	b. template datum<TYPE>() += dt[level(b)] * const_datum<DERIV>(b);
  }
  //----------------------------------------------------------------------------
  template<int TYPE, int COPY>
  inline void copy_all(const bodies*B, bool all) {
    if(all)
      LoopAllBodies(B,b)
	b. template datum<TYPE>() = const_datum<COPY>(b);
    else
      LoopAllBodies(B,b) if(is_active(b))
	b. template datum<TYPE>() = const_datum<COPY>(b);
  }
  //----------------------------------------------------------------------------
  template<int TYPE, int COPY>
  inline void copy_sph(const bodies*B, bool all) {
    if(all)
      LoopSPHBodies(B,b)
	b. template datum<TYPE>() = const_datum<COPY>(b);
    else
      LoopSPHBodies(B,b) if(is_active(b))
	b. template datum<TYPE>() = const_datum<COPY>(b);
  }
}
//------------------------------------------------------------------------------
void Integrator::drift(double dt, bool all) const
{
  const snapshot*const&B(SOLVER->snap_shot());
  B->advance_time_by(dt);
  if(predALL & fieldset::x) move_all<fieldbit::x,fieldbit::v>(B,dt,all);
  if(predALL & fieldset::w) move_all<fieldbit::w,fieldbit::a>(B,dt,all);
#ifdef falcON_SPH
  if(predSPH & fieldset::x) move_sph<fieldbit::x,fieldbit::v>(B,dt,all);
  if(predSPH & fieldset::V) move_sph<fieldbit::V,fieldbit::a>(B,dt,all);
  if(predSPH & fieldset::Y) move_sph<fieldbit::Y,fieldbit::I>(B,dt,all);
#endif
}
//------------------------------------------------------------------------------
void Integrator::kick(double dt, bool all) const
{
  const snapshot*const&B(SOLVER->snap_shot());
  if(kickALL & fieldset::v) move_all<fieldbit::v,fieldbit::a>(B,dt,all);
#ifdef falcON_SPH
  if(kickSPH & fieldset::v) move_sph<fieldbit::v,fieldbit::a>(B,dt,all);
  if(kickSPH & fieldset::U) move_sph<fieldbit::U,fieldbit::I>(B,dt,all);
#endif
}
//------------------------------------------------------------------------------
void Integrator::kick_i(const double*dt, bool all) const
{
  const snapshot*const&B(SOLVER->snap_shot());
  if(kickALL & fieldset::v) move_all_i<fieldbit::v,fieldbit::a>(B,dt,all);
#ifdef falcON_SPH
  if(kickSPH & fieldset::v) move_sph_i<fieldbit::v,fieldbit::a>(B,dt,all);
  if(kickSPH & fieldset::U) move_sph_i<fieldbit::U,fieldbit::I>(B,dt,all);
#endif
}
//------------------------------------------------------------------------------
void Integrator::remember(bool all) const
{
  const snapshot*const&B(SOLVER->snap_shot());
  if(rembALL & fieldset::w) copy_all<fieldbit::w,fieldbit::v>(B,all);
#ifdef falcON_SPH
  if(rembSPH & fieldset::V) copy_sph<fieldbit::V,fieldbit::v>(B,all);
  if(rembSPH & fieldset::Y) copy_sph<fieldbit::Y,fieldbit::U>(B,all);
#endif
}
//------------------------------------------------------------------------------
void Integrator::cpu_stats_body(std::ostream&to) const
{
  SOLVER->cpu_stats_body(to);
  print_cpu(CPU_STEP,to); to<<' ';
  int    h,m,s,c;
  double t = CPU_TOTAL;
  h = int(t/3600); t-= 3600*h;
  m = int(t/60);   t-= 60*m;
  s = int(t);      t-= s;
  c = int(100*t);
  to<<std::setw(3)<<std::setfill(' ')<<h<<':'
    <<std::setw(2)<<std::setfill('0')<<m<<':'
    <<std::setw(2)<<s<<'.'<<std::setw(2)<<c
    <<std::setfill(' ');
} 
//------------------------------------------------------------------------------
#ifdef falcON_NEMO
//------------------------------------------------------------------------------
void Integrator::describe(std::ostream&out,        // I: output stream          
			  const char  *com)        // I: command line           
const {
  out<<"#"; stats_line(out);
  out<<"# \""<<com<<"\"\n#\n";
  out<<"# run at  "  <<run_info::time()<<"\n";
  if(run_info::user_known()) out<<"#     by  \""<<run_info::user()<<"\"\n";
  if(run_info::host_known()) out<<"#     on  \""<<run_info::host()<<"\"\n";
  if(run_info::pid_known())  out<<"#     pid  " <<run_info::pid() <<"\n";
  out<<"#\n";
  out.flush();
}
//------------------------------------------------------------------------------
void Integrator::write(nemo_out const&o,           // I: nemo output            
		       fieldset       w) const     //[I: what to write]         
{
  if( o.is_sink()) return;
  if(!o.is_open()) 
    falcON_THROW("Integrator::write_nemo(): nemo device not open\n");
  snap_shot()->write_nemo(o,w);
} 
#endif
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// class falcON::LeapFrogCode                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
LeapFrogCode::LeapFrogCode(int h, const ForceAndDiagnose *F,
			   fieldset p, fieldset k, fieldset r,
			   fieldset P, fieldset K, fieldset R) falcON_THROWING :
  Integrator(F,p,k,r,P,K,R),
  TAU       (pow(0.5,h)),
  TAUH      (0.5*TAU)
{
  remember();                                      // eg: w = v                 
  set_time_derivs(1,1,0.);                         // eg: a = F(x,w)            
  finish_diagnose();                               // finish diagnosis          
  add_to_cpu_step();                               // record CPU time           
}
////////////////////////////////////////////////////////////////////////////////
void LeapFrogCode::fullstep() const {
  reset_CPU();                                     // reset cpu timers          
  kick(TAUH);                                      // eg: v+= a*tau/2           
  drift(TAU);                                      // eg: x+= v*tau;  w+= a*tau 
  set_time_derivs(1, 1, TAU);                      // eg: a = F(x,w)            
  kick(TAUH);                                      // eg: v+= a*tau/2           
  remember();                                      // eg: w = v                 
  finish_diagnose();                               // finish diagnosis          
  add_to_cpu_step();                               // record CPU time           
}    
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// class falcON::BlockStepCode                                                //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void BlockStepCode::elementary_step(int t) const { // I: number of step         
  static int m = 0;                                // # of tiny moves omitted   
  ++m;                                             // cound shortest steps      
  ++t;                                             // add one to t              
  int l=HIGHEST;                                   // find lowest level moving  
  for(; !(t&1) && l; t>>=1, --l);                  // l: lowest level moving    
  bool move=false;                                 // need to do anything?      
  for(int i=l; i!=NSTEPS; ++i)                     // LOOP levels up to highest 
    if(N[i]) move = true;                          //   IF any non-empty: move  
  if(!move) return;                                // IF not moving: DONE       
  bool all=true;                                   // are all active?           
  for(int i=0; i!=l; ++i)                          // LOOP lower levels         
    if(N[i]) all  = false;                         //   IF all empty: all active
  double dt=tau_min() * m;                         // dt = m*tau_min            
  drift(dt);                                       // predict @ new time        
  m = 0;                                           // reset m = 0               
  LoopAllBodies(snap_shot(),b)                     // LOOP bodies               
    if(level(b) >= l) b.flag_as_active();          //   IF(level>=l): active    
    else              b.unflag_active ();          //   ELSE        : inactive  
  set_time_derivs(all,l==0,dt);                    // set accelerations etc     
  kick_i(TAUH,all);                                // kick velocity etc         
  if(l != highest_level() ||                       // IF(levels may change OR   
     ST->always_adjust() )                         //    always adjusting       
    adjust_levels(l, l==0);                        //   h -> h'                 
  if(l) {                                          // IF not last step          
    remember(all);                                 //   remember to be predicted
    kick_i(TAUH,all);                              //   kick by half a step     
  }                                                // ENDIF                     
}
//------------------------------------------------------------------------------
void BlockStepCode::fullstep() const {
  reset_CPU();                                     // reset cpu timers          
  remember(true);                                  // remember to be predicted  
  kick_i(TAUH,true);                               // kick by half a step       
  for(int t=0; t!=1<<HIGHEST; ++t)                 // LOOP elementary steps     
    elementary_step(t);                            //   elementary step         
  finish_diagnose();                               // finish diagnosis          
  add_to_cpu_step();                               // record CPU time           
}
//------------------------------------------------------------------------------
BlockStepCode::BlockStepCode(int h0,               // I: h0, tau_max = 2^-h0    
			     int Ns,               // I: #steps                 
			     const ForceAndDiagnose *F,
			     const StepLevels       *S,
			     fieldset p, fieldset k, fieldset r,
			     fieldset P, fieldset K, fieldset R, int w)
  falcON_THROWING :
  Integrator( F,p,k,r,P,K,R ),
  H0        ( h0 ),
  NSTEPS    ( abs(Ns) ),
  HIGHEST   ( NSTEPS-1 ),
  TAUH      ( falcON_NEW(double,NSTEPS) ),
  N         ( falcON_NEW(int   ,NSTEPS) ),
  W         ( (H0+HIGHEST)>9? max(5,w) : max(4,w) ),
  ST        ( S )
{
  T[0]     = falcON_NEW(double,NSTEPS);
  T[1]     = falcON_NEW(double,NSTEPS);
  N[0]     = 0;
  T[0][0]  = pow(0.5,h0);
  TAUH[0]  = 0.5*T[0][0];
  T[1][0]  = square(T[0][0]);
  for(int i=1; i!=NSTEPS; ++i) {
    N[i]    = 0;
    T[0][i] = TAUH[i-1];
    TAUH[i] = 0.5*T[0][i];
    T[1][i] = square(T[0][i]);
  }
  remember();                                      // to be predicted quantities
  set_time_derivs(1,1,0.);                         // set initial forces        
  snap_shot()->add_fields(fieldset::l);
  assign_levels();                                 // get bodies into levels    
  finish_diagnose();                               // finish diagnosis          
  add_to_cpu_step();                               // record CPU time           
}
////////////////////////////////////////////////////////////////////////////////
namespace falcON {
  inline std::ostream& put_char(std::ostream&o, const char c, const int s) {
    if(s>0) for(register int i=0; i<s; ++i) o<<c;
    return o;
  }
}
//------------------------------------------------------------------------------
void BlockStepCode::stats_head(std::ostream&to) const {
  SOLVER -> dia_stats_head(to);
  if(HIGHEST)
    for(int i=0, h=-h0(); i!=NSTEPS; i++, h--)
      if     (h>13)  put_char(to,' ',W-4)<<"2^" <<     h  <<' ';
      else if(h> 9)  put_char(to,' ',W-4)       << (1<<h) <<' ';
      else if(h> 6)  put_char(to,' ',W-4)<<' '  << (1<<h) <<' ';
      else if(h> 3)  put_char(to,' ',W-4)<<"  " << (1<<h) <<' ';
      else if(h>=0)  put_char(to,' ',W-4)<<"   "<< (1<<h) <<' ';
      else if(h==-1) put_char(to,' ',W-4)<<" 1/2 ";
      else if(h==-2) put_char(to,' ',W-4)<<" 1/4 ";
      else if(h==-3) put_char(to,' ',W-4)<<" 1/8 ";
      else if(h==-4) put_char(to,' ',W-4)<<"1/16 ";
      else if(h==-5) put_char(to,' ',W-4)<<"1/32 ";
      else if(h==-6) put_char(to,' ',W-4)<<"1/64 ";
      else if(h>-10) put_char(to,' ',W-4)<<"2^" <<     h  <<' ';
      else           put_char(to,' ',W-5)<<"2^" <<     h  <<' ';
  cpu_stats_head(to);
  to<<std::endl;
}
#ifdef falcON_NEMO
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// class falcON::NBodyCode                                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
NBodyCode::NBodyCode(// data input                                              
		     const char*file,              // I: input file             
		     bool       resume,            // I: resume old (if nemo)   
		     fieldset   read_more,         // I: further data to read   
		     const char*time)              // I: time for initial data  
  falcON_THROWING :
  FILE ( file ),
  SHOT ( fieldset::gravity | read_more ),
  CODE ( 0 )
{
  const fieldset read(fieldset::basic | read_more);
  nemo_in  In(file);                               // open nemo input           
  fieldset got;                                    // what did we get?          
  if(resume) {                                     // IF resuming: last snapshot
    do   SHOT.read_nemo(In,got,read,0,0);          //   DO:  read bodies        
    while(In.has_snapshot());                      //   WHILE more to be read   
  } else {                                         // ELIF:                     
    bool gotit=false;                              //   read snapshot?          
    do   gotit=SHOT.read_nemo(In,got,read,time,0); //   DO:  try to read them   
    while(!gotit && In.has_snapshot());            //   WHILE snapshots present 
    if(!gotit)                                     //   didn't read any -> ERROR
      falcON_THROW("NBodyCode: no snapshot matching \"time=%s\""
		   "found in file \"%s\"",time? time:"  ", file);
  }                                                // ENDIF                     
  if(!got.contain(fieldset::f))                    // UNLESS flags just read    
    SHOT.reset_flags();                            //   reset them              
  if(!got.contain(read))                           // IF some data missing      
    falcON_THROW("NBodyCode: couldn't read body data: %s",
		 word(got.missing(read)));
}
//------------------------------------------------------------------------------
void NBodyCode::init(const ForceAndDiagnose         *FS,
		     int                             hmin,
		     int                             Nlev,
		     const BlockStepCode::StepLevels*St,
		     fieldset p, fieldset k, fieldset r,
		     fieldset P, fieldset K, fieldset R) falcON_THROWING
{
  try {
    if(FS->acc_ext()) SHOT.add_fields(fieldset::q);
    if(Nlev <= 1 || St == 0)
      CODE = static_cast<const Integrator*>
	( new LeapFrogCode(hmin,FS,p,k,r,P,K,R) );
    else
      CODE = static_cast<const Integrator*>
	( new BlockStepCode(hmin+1-Nlev,Nlev,FS,St,p,k,r,P,K,R,
			    int(1+std::log10(double(SHOT.N_bodies())))) );
   
  } catch(falcON::exception E) { falcON_RETHROW(E); }
}
#endif // falcON_NEMO
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// class falcON::ForceDiagGrav                                                //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void ForceDiagGrav::diagnose_grav() const
{
  double m(0.), vin(0.), vex(0.), w[Ndim][Ndim]={0.};
  vect_d x(0.);
  if(snap_shot()->have(fieldbit::q)) {             // IF have external pot      
    LoopAllBodies(snap_shot(),b) {                 //   LOOP bodies             
      register double mi = mass(b);                //     m                     
      m  += mi;                                    //     add: total mass       
      vin+= mi * pot(b);                           //     add: int pot energy   
      vex+= mi * pex(b);                           //     add: ext pot energy   
      register vect_d mx = mi * pos(b);            //     m * x                 
      AddTensor(w,mx,acc(b));                      //     add to W_ij           
      x += mx;                                     //     add: dipole           
    }                                              //   END LOOP                
  } else {                                         // ELSE: no external pot     
    LoopAllBodies(snap_shot(),b) {                 //   LOOP bodies             
      register double mi = mass(b);                //     m                     
      m  += mi;                                    //     add: total mass       
      vin+= mi * pot(b);                           //     add: int pot energy   
      register vect_d mx = mi * pos(b);            //     m * x                 
      AddTensor(w,mx,acc(b));                      //     add to W_ij           
      x += mx;                                     //     add: dipole           
    }                                              //   END LOOP                
  }                                                // ENDIF                     
  M    = m;                                        // total mass                
  Vin  = half*vin;                                 // total int pot energy      
  Vex  = vex;                                      // total ext pot energy      
  W    = tr(w);                                    // total pot energy from acc 
  CMX  = x/m;                                      // center of mass            
  for(register int i=0; i!=Ndim; ++i)
    for(register int j=0; j!=Ndim; ++j) 
      WT[i][j] = half *(w[i][j]+w[j][i]);          // pot energy tensor         
  TIME  = snap_shot()->time();
}
////////////////////////////////////////////////////////////////////////////////
void ForceDiagGrav::diagnose_vels() const falcON_THROWING
{
  if(snap_shot()->time() != TIME)
    falcON_THROW("ForceDiagGrav::diagnose_vels(): time mismatch");
  double m(0.), k[Ndim][Ndim]={0.};
  vect_d v(0.), l(0.);
  LoopAllBodies(snap_shot(),b) {                   // LOOP bodies               
    register double mi = mass(b);                  //   m                       
    m  += mi;                                      //   add: total mass         
    register vect_d mv = mi * vel(b);              //   m * v                   
    AddTensor(k,mv,vel(b));                        //   add to K_ij             
    v += mv;                                       //   add: total momentum     
    l += vect_d(pos(b)) ^ mv;                      //   add: total ang mom      
  }                                                // END LOOP                  
  T    = half*tr(k);                               // total kin energy          
  TW   =-T/W;                                      // virial ratio              
  L    = l;                                        // total angular momentum    
  CMV  = v/m;                                      // center of mass velocity   
  for(register int i=0; i!=Ndim; ++i)
    for(register int j=0; j!=Ndim; ++j) 
      KT[i][j] = half * k[i][j];                   // kin energy tensor         
}
////////////////////////////////////////////////////////////////////////////////
void ForceDiagGrav::diagnose_full() const
{
  double m(0.), vin(0.), vex(0.), w[Ndim][Ndim]={0.}, k[Ndim][Ndim]={0.};
  vect_d x(0.), v(0.), l(0.);
  if(snap_shot()->have(fieldbit::q)) {             // IF have external pot      
    LoopAllBodies(snap_shot(),b) {                 //   LOOP bodies             
      register double mi = mass(b);                //     m                     
      m  += mi;                                    //     add: total mass       
      vin+= mi * pot(b);                           //     add: int pot energy   
      vex+= mi * pex(b);                           //     add: ext pot energy   
      register vect_d mx = mi * pos(b);            //     m * x                 
      register vect_d mv = mi * vel(b);            //     m * v                 
      AddTensor(w,mx,acc(b));                      //     add to W_ij           
      AddTensor(k,mv,vel(b));                      //     add to K_ij           
      x += mx;                                     //     add: dipole           
      v += mv;                                     //     add: total momentum   
      l += mx ^ vect_d(vel(b));                    //     add: total ang mom    
    }                                              //   END LOOP                
  } else {                                         // ELSE: no external pot     
    LoopAllBodies(snap_shot(),b) {                 //   LOOP bodies             
      register double mi = mass(b);                //     m                     
      m  += mi;                                    //     add: total mass       
      vin+= mi * pot(b);                           //     add: int pot energy   
      register vect_d mx = mi * pos(b);            //     m * x                 
      register vect_d mv = mi * vel(b);            //     m * v                 
      AddTensor(w,mx,acc(b));                      //     add to W_ij           
      AddTensor(k,mv,vel(b));                      //     add to K_ij           
      x += mx;                                     //     add: dipole           
      v += mv;                                     //     add: total momentum   
      l += mx ^ vect_d(vel(b));                    //     add: total ang mom    
    }                                              //   END LOOP                
  }                                                // ENDIF                     
  M    = m;                                        // total mass                
  Vin  = half*vin;                                 // total int pot energy      
  Vex  = vex;                                      // total ext pot energy      
  W    = tr(w);                                    // total pot energy from acc 
  T    = half*tr(k);                               // total kin energy          
  TW   =-T/W;                                      // virial ratio              
  L    = l;                                        // total angular momentum    
  CMX  = x/m;                                      // center of mass            
  CMV  = v/m;                                      // center of mass velocity   
  for(register int i=0; i!=Ndim; ++i)
    for(register int j=0; j!=Ndim; ++j) {
      WT[i][j] = half *(w[i][j]+w[j][i]);          // pot energy tensor         
      KT[i][j] = half * k[i][j];                   // kin energy tensor         
    }
  TIME  = snap_shot()->time();
}
////////////////////////////////////////////////////////////////////////////////
#if 0
void ForceDiagGrav::write_diag_nemo(nemo_out const&out,
				   double         cpu) const
{
  out.open_set(nemo_io::diags);                    // OPEN diagnostics set      
  out.single_vec(0) = Ekin() + Epot();             //     copy total energy     
  out.single_vec(1) = Ekin();                      //     copy kin energy       
  out.single_vec(2) = Epot();                      //     copy pot energy       
  out.write(nemo_io::energy);                      //     write energies        
  for(int i=0;i!=Ndim;++i) for(int j=0;j!=Ndim;++j)//   LOOP dims twice         
    out.single_mat(i,j) = KT[i][j];                //     copy K_ij             
  out.write(nemo_io::KinT);                        //   write K_ij              
  for(int i=0;i!=Ndim;++i) for(int j=0;j!=Ndim;++j)//   LOOP dims twice         
    out.single_mat(i,j) = WT[i][j];                //     copy W_ij             
  out.write(nemo_io::PotT);                        //   write W_ij              
  for(int i=0;i!=Ndim;++i) for(int j=0;j!=Ndim;++j)//   LOOP dimensions         
    out.single_mat(i,j) = as_angmom(L,i,j);        //     copy A_ij             
  out.write(nemo_io::AmT);                         //   write A_ij              
  for(int i=0;i!=Ndim;++i) {                       //   LOOP dims               
    out.single_phs(0,i) = CMX[i];                  //     copy c-of-m pos       
    out.single_phs(1,i) = CMV[i];                  //     copy c-of-m vel       
  }                                                //   END LOOP                
  out.write(nemo_io::cofm);                        //   write c-of-m (x,v)      
  out.write(nemo_io::cputime, (cpu)/60.);          //   write accum CPU [min]   
  out.close_set(nemo_io::diags);                   // CLOSE diagnostics set     
}
#endif
////////////////////////////////////////////////////////////////////////////////
void ForceDiagGrav::dia_stats_body(std::ostream&to) const
{
  int ACC = 1+sizeof(real);
  std::ios::fmtflags old = to.flags();
  to.setf(std::ios::left | std::ios::showpoint);
  to  <<std::setprecision(ACC)<<std::setw(ACC+5)<<TIME       <<' '
      <<std::setprecision(ACC+2)<<std::setw(ACC+8)<<T+Vin+Vex  <<' '
      <<std::setprecision(ACC)<<std::setw(ACC+5)<<T          <<' ';
  if(SELF_GRAV) 
    to<<std::setprecision(ACC)<<std::setw(ACC+6)<<Vin        <<' ';
  if(acc_ext())
    to<<std::setprecision(ACC)<<std::setw(ACC+6)<<Vex        <<' ';
  to  <<std::setprecision(ACC)<<std::setw(ACC+6)<<W          <<' '
      <<std::setprecision(min(ACC,max(1,ACC-int(-log10(twice(TW))))))
      <<std::setw(ACC+2)<<twice(TW) <<' '
      <<std::setprecision(ACC)<<std::setw(ACC+5)
      <<std::sqrt(norm(L))
      <<' '
      <<std::setprecision(ACC-3)<<std::setw(ACC+3)<<std::sqrt(norm(CMV));
  to.flags(old);
}
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// class falcON::ForceALCON                                                   //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void ForceALCON::reset_softening(
				  kern_type ker,
				  real      e
#ifdef falcON_ADAP
				 ,real      ns,
				  unsigned  nr,
				  real      em,
				  real      ef
#endif
				  )
{
#ifdef falcON_ADAP
  NSOFT = ns;
  NREF  = nr;
  EMIN  = abs(em);
  EFAC  = abs(ef);
  if(SOFTENING == individual_adaptive && EFAC  == zero)
    falcON_THROW("ForceALCON: using individual adaptive softening, "
		 " but eps_fac=0\n");
  if(SOFTENING == individual_adaptive && NSOFT == zero)
    falcON_THROW("ForceALCON: using individual adaptive softening, "
		 "but Nsoft=0\n");
#endif
  FALCON.reset_softening(abs(e),ker);
}
////////////////////////////////////////////////////////////////////////////////
ForceALCON::ForceALCON(snapshot          *s,       // I: snapshot: time & bodies
		       real               e,       // I: global softening length
		       real               th,      // I: tolerance parameter    
		       int                nc,      // I: N_crit                 
		       const vect        *x0,      // I: pre-set root centre    
		       kern_type          ke,      // I: softening kernel       
		       real               g,       // I: Newton's G             
		       int                ru,      // I: # reused of tree       
		       const acceleration*ae,      // I: external acceleration  
		       const int          gd[4]    // I: direct sum: gravity    
#ifdef falcON_INDI
		       ,soft_type         sf       // I: softening type         
#ifdef falcON_ADAP
		       ,real              ns       // I: N_soft                 
		       ,unsigned          nr       // I: N_ref                  
		       ,real              em       // I: eps_min                
		       ,real              ef       // I: eps_fac                
#endif
#endif
#ifdef falcON_SPH
		       ,const int         sd[3]    //[I: direct sum: SPH]       
#endif
		       ) falcON_THROWING :
  ForceDiagGrav ( s, ae, g!=zero ),
#ifdef falcON_INDI
  SOFTENING     ( sf ),
#endif
  ROOTCENTRE    ( x0 ),
  NCRIT         ( max(1,nc) ),
  REUSE         ( ru ),
  FALCON        ( s, abs(e), abs(th), ke,
#ifdef falcON_INDI
		  SOFTENING != global_fixed,
#endif
		  g, th < zero? const_theta : theta_of_M, gd
#ifdef falcON_SPH
		  , sd
#endif
		  ),
  REUSED        ( ru ),
  CPU_TREE      ( 0. ),
  CPU_GRAV      ( 0. ),
  CPU_AEX       ( 0. )
{
#ifdef falcON_INDI
  if(SOFTENING==individual_fixed && !snap_shot()->have(fieldbit::e)) 
    falcON_THROW("ForceALCON: individual fixed softening, but no eps_i given");
#endif
#ifdef falcON_ADAP
  reset_softening(ke,e,ns,nr,em,ef);
#else
  reset_softening(ke,e);
#endif
}
//------------------------------------------------------------------------------
void ForceALCON::set_tree_and_forces(bool all, bool build_tree) const
{
  clock_t cpu = clock();
  // 1. build tree if required
  if(SELF_GRAV || build_tree) {
    if(REUSED < REUSE) {                           // IF may re-use old tree    
      REUSED++;                                    //   increment # re-using    
      FALCON.reuse();                              //   re-use old tree         
    } else {                                       // ELSE                      
      FALCON.grow(NCRIT,ROOTCENTRE);               //   grow new tree           
      REUSED=0;                                    //   reset # re-using        
    }                                              // ENDIF                     
    Integrator::record_cpu(cpu,CPU_TREE);          // record CPU consumption    
  }
  // 2.  deal with self-gravity
  if(SELF_GRAV) {
    // 2.1 compute self-gravity
#ifdef falcON_ADAP
    if(SOFTENING==individual_adaptive)             // IF adaptive softening     
      FALCON.approximate_gravity(true,all,NSOFT,NREF,EMIN,EFAC);
    else                                           // ELIF: fixed softening     
#endif
      FALCON.approximate_gravity(true,all);        // ELIF: fixed softening     
    Integrator::record_cpu(cpu,CPU_GRAV);          // record CPU consumption    
  } else {
    // 2.2 no self-gravity: //   reset pot and acc
    if(acc_ext()) {
      LoopAllBodies(snap_shot(),b)
	if(all || is_active(b))
	  b.pot() = zero;
    } else {
      LoopAllBodies(snap_shot(),b)
	if(all || is_active(b)) {
	  b.pot() = zero;
	  b.acc() = zero;
	}
    }
  }
  // 3 compute external gravity
  if(acc_ext()) {
    acc_ext() -> set(snap_shot(), all, SELF_GRAV? 2 : 0);
    Integrator::record_cpu(cpu,CPU_AEX);           // record CPU consumption    
  }
}
//------------------------------------------------------------------------------
void ForceALCON::cpu_stats_body(std::ostream&to) const {
  if(SELF_GRAV) {
    Integrator::print_cpu(CPU_TREE, to);
    to<<' ';
    Integrator::print_cpu(CPU_GRAV, to);
    to<<' ';
  }
  if(acc_ext()) {
    Integrator::print_cpu(CPU_AEX, to);
    to<<' ';
  }
  CPU_TREE = 0.;
  CPU_GRAV = 0.;
  CPU_AEX  = 0.;
}
////////////////////////////////////////////////////////////////////////////////