// -*- C++ -*-                                                                 |
//-----------------------------------------------------------------------------+
//                                                                             |
// tset.h                                                                      |
//                                                                             |
// C++ code                                                                    |
//                                                                             |
// Copyright Walter Dehnen, 2003-2004                                          |
// e-mail:   walter.dehnen@astro.le.ac.uk                                      |
// address:  Department of Physics and Astronomy, University of Leicester      |
//           University Road, Leicester LE1 7RH, United Kingdom                |
//                                                                             |
//-----------------------------------------------------------------------------+
//                                                                             |
// classes                                                                     |
//                                                                             |
// class nbdy::symset3D<N,X>   set of 3D symmetric tensors of order 0,...,N    |
// class nbdy::poles3D<N,X>    multipoles of order 2,...,N                     |
//                                                                             |
//-----------------------------------------------------------------------------+
#ifndef falcON_included_tset_h
#define falcON_included_tset_h

#ifndef falcON_included_tn3D_h
#  include <public/tn3D.h>
#endif
#if defined(falcON_SSE_CODE) && !defined(falcON_included_simd_h)
#  include <proper/simd.h>
#endif

namespace nbdy {
  //////////////////////////////////////////////////////////////////////////////
  //                                                                          //
  // class nbdy::poles3D<N,X>                                                 //
  //                                                                          //
  // set of symmetric 3D tensors of orders 2 to N and with elements of type X //
  //                                                                          //
  //////////////////////////////////////////////////////////////////////////////
  template<int N, typename X=real> class poles3D {
    poles3D(poles3D const&);                       // no copy constructor       
    // public static members and type                                           
  public:
    typedef X element_type;
    static const int NDAT = ((N+1)*(N+2)*(N+3))/6 - 4;
    // private types                                                            
  private:
    typedef poles3D<N,X>          M;               // set of poles              
    typedef meta::taux<X,NDAT-1>  L;               // meta looping (k,l,m)      
    typedef tupel<3,X>            V;               // vector                    
    // static methods (type conversions to X* and const X*)                     
    template<class A>
    static X*      pX(A      &x) { return static_cast<      X*>(x); }
    template<class A>
    static const X*pX(A const&x) { return static_cast<const X*>(x); }
    // datum                                                                    
    X a[NDAT];
  public:
    // construction                                                             
    poles3D()          {}
    poles3D(X const&x) { L::s_as(a,x); }
    // element access                                                           
    template<int K>
    symt3D<K,X>      &pole()       { return meta3D::ONE3D<K,X>::pole(a); }
    template<int K>
    symt3D<K,X> const&pole() const { return meta3D::ONE3D<K,X>::pole(a); }
    // unitary operators                                                        
    M&negate         ()                    { L::v_neg(a); return*this; }
    M&set_zero       ()                    { L::s_ze (a); return*this; }
    operator       X*()                    { return a; }
    operator const X*() const              { return a; }
    // binary operators with poles3D<N,X>                                       
    M&operator=      (M const&m)           { L::v_as (a,m.a);   return*this; }
    M&operator+=     (M const&m)           { L::v_ad (a,m.a);   return*this; }
    M&operator-=     (M const&m)           { L::v_su (a,m.a);   return*this; }
    M&ass_times      (M const&m,X const&x) { L::v_ast(a,m.a,x); return*this; }
    M&add_times      (M const&m,X const&x) { L::v_adt(a,m.a,x); return*this; }
    M&sub_times      (M const&m,X const&x) { L::v_sut(a,m.a,x); return*this; }
    // binary operators with scalar                                             
    M& operator*=    (X const&x)           { L::s_ml(a,x); return*this; }
    M& operator/=    (X const&x)           { return operator*=(X(1)/x); }
    bool operator==  (X const&x) const     { return L::s_eq(a,x); }
    bool operator!=  (X const&x) const     { return L::s_neq(a,x); }
    // generating outer products of vector                                      
    M&ass_out_prd    (V const&);
    // add poles from single point mass                                         
    M&add_body       (V const&, X const&);
    // add poles from cell, represented by its mass and poles                   
    M&add_cell       (V const&, X const&, M const&);
    // normalize: multiply pole n by   x / n!                                   
    M&normalize      (X const&);
  };
  //////////////////////////////////////////////////////////////////////////////
  template<typename X> class poles3D<1,X> {
  public: void normalize(X const&) {} };
  template<typename X> class poles3D<0,X> {
  public: void normalize(X const&) {} };
  //////////////////////////////////////////////////////////////////////////////
  //                                                                          //
  // class nbdy::symset3D<N,X>                                                //
  //                                                                          //
  // set of symmetric 3D tensor of orders 0 to N and with elements of type X  //
  //                                                                          //
  //////////////////////////////////////////////////////////////////////////////
  template<int N, typename X=real> class symset3D {
    // public static members and type                                           
  public:
    typedef X element_type;
    static const int NDAT = ((N+1)*(N+2)*(N+3))/6;
    // private types                                                            
  private:
    typedef symset3D<N,X>         C;               // set of sym tensors        
    typedef poles3D<N-1,X>        M;               // associated set of poles   
    typedef meta::taux<X,NDAT-1>  L;               // meta looping (k,l,m)      
    typedef tupel<3,X>            V;               // vector                    
    // static methods (type conversions to X* and const X*)                     
    template<class A>
    static       X*pX(A      &x) { return static_cast<      X*>(x); }
    template<class A>
    static const X*pX(A const&x) { return static_cast<const X*>(x); }
    // datum                                                                    
    X a[NDAT];
  public:
    // construction                                                             
    symset3D()          {}
    symset3D(X const&x) { L::s_as(a,x); }
//     symset3D(C const&c) { L::v_as(a,c.a); }
    // element access                                                           
    template<int K> typename meta3D::ONE3D<K,X>::Tensor
    const&tensor () const { return meta3D::ONE3D<K,X>::tens(a); }
    template<int K> typename meta3D::ONE3D<K,X>::Tensor
         &tensor ()       { return meta3D::ONE3D<K,X>::tens(a); }
    // sub-set of first K tensors                                               
    template<int K> symset3D<K,X>
    const&subset () const { return *static_cast<const symset3D<K,X>*>
			           (static_cast<const void*>(this)); }
    template<int K> symset3D<K,X>
         &subset ()       { return *static_cast<symset3D<K,X>*>
			           (static_cast<void*>(this)); }
    // unitary operators                                                        
    C&negate         ()                    { L::v_neg(a); return*this; }
    C&set_zero       ()                    { L::s_ze (a); return*this; }
    operator       X*()                    { return a; }
    operator const X*() const              { return a; }
    // binary operators with scalar                                             
    C&operator=      (X const&x)           { L::s_as(a,x); return*this; }
    C&operator*=     (X const&x)           { L::s_ml(a,x); return*this; }
    C&operator/=     (X const&x)           { return operator*=(X(1)/x); }
    bool operator==  (X const&x) const     { return L::s_eq(a,x); }
    bool operator!=  (X const&x) const     { return L::s_neq(a,x); }
    // binary operators with symset3D<N,X>                                      
    C&operator=      (C const&c)           { L::v_as (a,c.a);   return*this; }
    C&operator+=     (C const&c)           { L::v_ad (a,c.a);   return*this; }
    C&operator-=     (C const&c)           { L::v_su (a,c.a);   return*this; }
    C&ass_times      (C const&c,X const&x) { L::v_ast(a,c.a,x); return*this; }
    C&add_times      (C const&c,X const&x) { L::v_adt(a,c.a,x); return*this; }
    C&sub_times      (C const&c,X const&x) { L::v_sut(a,c.a,x); return*this; }
    // generating outer products of vector                                      
    C&ass_out_prd    (V const&);
    // flip sign for odd- or even-ordered tensors                               
    C&flip_sign_odd  ();
    C&flip_sign_even ();
    // complete inner products with vector                                      
    void ass_inn_prd(V const&, symset3D<N-1,X>&) const;
  };
  //////////////////////////////////////////////////////////////////////////////
  template<typename X> class symset3D<0,X> {};     // use X instead             
  //////////////////////////////////////////////////////////////////////////////
  //                                                                          //
  // support for gravity at general expansion order                           //
  //                                                                          //
  //////////////////////////////////////////////////////////////////////////////
  //----------------------------------------------------------------------------
  // set F^k := m*m * nabla^(k) * Phi, given d_k:= m*m * (1/r d/dr)^k Phi       
  //----------------------------------------------------------------------------
#ifdef falcON_SSE_CODE
  template<int N, typename X> inline void
  set_dPhi(symset3D<N,X>      &,                   // I: F to assign to         
	   tupel   <3,X> const&,                   // I: vector R               
	   const fvec4[N+1],                       // I: set of D_n {n=0...N}   
	   int           const&);                  // I: which elem of fvec4s?  
  template<int N, typename X> inline void
  add_dPhi(symset3D<N,X>      &,                   // I: F to add to            
	   tupel   <3,X> const&,                   // I: vector R               
	   const fvec4[N+1],                       // I: set of D_n {n=0...N}   
	   int           const&);                  // I: which elem of fvec4s?  
#else
  template<int N, typename X> inline void
  set_dPhi(symset3D<N,X>      &,                   // I: F to assign to         
	   tupel   <3,X> const&,                   // I: vector R               
	   const      X[N+1]);                     // I: set of D_n {n=0...N}   
  template<int N, typename X> inline void
  add_dPhi(symset3D<N,X>      &,                   // I: F to add to            
	   tupel   <3,X> const&,                   // I: vector R               
	   const      X[N+1]);                     // I: set of D_n {n=0...N}   
#endif
  //----------------------------------------------------------------------------
  // C^k += Sum_n^{N-k} (-1)^n/n! F^{n+k} . M^n                                 
  //----------------------------------------------------------------------------
  template<int N, typename X> inline void
  add_C_C2C(symset3D<N,X>       &,                 // I: C to add to            
	    symset3D<N,X>  const&,                 // I: F : mm nabla^(k) Phi   
	    poles3D<N-1,X> const&);                // I: M~: source's multipoles
  //----------------------------------------------------------------------------
  template<int N, typename X> inline void
  add_C_C2B(symset3D<1,X>       &,                 // I: C to add to            
	    symset3D<N,X>  const&,                 // I: F : mm nabla^(k) Phi   
	    poles3D<N-1,X> const&);                // I: M~: source's multipoles
  //----------------------------------------------------------------------------
  template<int N, typename X> inline void
  add_C_B2C(symset3D<N,X>      &c,                 // I: C to add to            
	    symset3D<N,X> const&f) {               // I: F : mm nabla^(k) Phi   
    c += f;
  }
  //----------------------------------------------------------------------------
  // shift expansion by dX                                                      
  //----------------------------------------------------------------------------
  template<int N, typename X> inline void
  shift_by(symset3D<N,X>&, tupel<3,X>&);
  //----------------------------------------------------------------------------
  // evaluate expansion at X                                                    
  //----------------------------------------------------------------------------
  template<int N, typename X> inline void
  eval_expn(symset3D<1,X>&, symset3D<N,X> const&, tupel<3,X> const&);
  //////////////////////////////////////////////////////////////////////////////
}                                                  // END: namespace nbdy       
#include <public/tset.cc>
////////////////////////////////////////////////////////////////////////////////
#endif	                                           // falcON_included_tset_h    