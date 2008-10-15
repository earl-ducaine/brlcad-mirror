/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2007 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#if !defined(OPENNURBS_CURVE_ON_SURFACE_INC_)
#define OPENNURBS_CURVE_ON_SURFACE_INC_

class ON_CurveOnSurface;
class ON_CLASS ON_CurveOnSurface : public ON_Curve
{
  ON_OBJECT_DECLARE(ON_CurveOnSurface);

public:
  ON_CurveOnSurface();

  /*
  Parameters:
    p2dCurve - [in] ~ON_CurveOnSurface() will delete this curve.
       Use an ON_CurveProxy if you don't want the original deleted.
    p3dCurve - [in] ~ON_CurveOnSurface() will delete this curve.
       Use an ON_CurveProxy if you don't want the original deleted.
    pSurface - [in] ~ON_CurveOnSurface() will delete this surface.
       Use an ON_SurfaceProxy if you don't want the original deleted.
  */
  ON_CurveOnSurface( ON_Curve* p2dCurve,  // required 2d curve
		     ON_Curve* p3dCurve,  // optional 3d curve
		     ON_Surface* pSurface // required surface
		     );
  ON_CurveOnSurface(const ON_CurveOnSurface&); // no implementation
  ON_CurveOnSurface& operator=(const ON_CurveOnSurface&); // no implementation

  /*
  Remarks:
    Deletes m_c2, m_c3, and m_s.  Use ON_CurveProxy or ON_SurfaceProxy
    if you need to use curves or a surface that you do not want deleted.
  */
  virtual ~ON_CurveOnSurface();

  // virtual ON_Object::SizeOf override
  unsigned int SizeOf() const;


  /////////////////////////////////////////////////////////////////
  // ON_Object overrides

  /*
  Description:
    Tests an object to see if its data members are correctly
    initialized.
  Parameters:
    text_log - [in] if the object is not valid and text_log
	is not NULL, then a brief englis description of the
	reason the object is not valid is appened to the log.
	The information appended to text_log is suitable for 
	low-level debugging purposes by programmers and is 
	not intended to be useful as a high level user 
	interface tool.
  Returns:
    @untitled table
    TRUE     object is valid
    FALSE    object is invalid, uninitialized, etc.
  Remarks:
    Overrides virtual ON_Object::IsValid
  */
  BOOL IsValid( ON_TextLog* text_log = NULL ) const;

  void Dump( ON_TextLog& ) const; // for debugging

  BOOL Write(
	 ON_BinaryArchive&  // open binary file
       ) const;

  BOOL Read(
	 ON_BinaryArchive&  // open binary file
       );

  /////////////////////////////////////////////////////////////////
  // ON_Geometry overrides

  int Dimension() const;

  BOOL GetBBox( // returns TRUE if successful
	 double*,    // minimum
	 double*,    // maximum
	 BOOL = FALSE  // TRUE means grow box
	 ) const;

  BOOL Transform( 
	 const ON_Xform&
	 );

  // (optional - default uses Transform for 2d and 3d objects)
  BOOL SwapCoordinates(
	int, int        // indices of coords to swap
	);

  /////////////////////////////////////////////////////////////////
  // ON_Curve overrides

  ON_Interval Domain() const;

  int SpanCount() const; // number of smooth spans in curve

  BOOL GetSpanVector( // span "knots" 
	 double* // array of length SpanCount() + 1 
	 ) const; // 

  int Degree( // returns maximum algebraic degree of any span 
		  // ( or a good estimate if curve spans are not algebraic )
    ) const; 


  // (optional - override if curve is piecewise smooth)
  BOOL GetParameterTolerance( // returns tminus < tplus: parameters tminus <= s <= tplus
	 double,  // t = parameter in domain
	 double*, // tminus
	 double*  // tplus
	 ) const;

  BOOL IsLinear( // TRUE if curve locus is a line segment between
		 // between specified points
	double = ON_ZERO_TOLERANCE // tolerance to use when checking linearity
	) const;

  BOOL IsArc( // ON_Arc.m_angle > 0 if curve locus is an arc between
	      // specified points
	const ON_Plane* = NULL, // if not NULL, test is performed in this plane
	ON_Arc* = NULL, // if not NULL and TRUE is returned, then arc parameters
			 // are filled in
	double = ON_ZERO_TOLERANCE    // tolerance to use when checking
	) const;

  BOOL IsPlanar(
	ON_Plane* = NULL, // if not NULL and TRUE is returned, then plane parameters
			   // are filled in
	double = ON_ZERO_TOLERANCE    // tolerance to use when checking
	) const;

  BOOL IsInPlane(
	const ON_Plane&, // plane to test
	double = ON_ZERO_TOLERANCE    // tolerance to use when checking
	) const;

  BOOL IsClosed(  // TRUE if curve is closed (either curve has
	void      // clamped end knots and euclidean location of start
	) const;  // CV = euclidean location of end CV, or curve is
		  // periodic.)

  BOOL IsPeriodic(  // TRUE if curve is a single periodic segment
	void 
	) const;
  
  BOOL Reverse();       // reverse parameterizatrion
			// Domain changes from [a,b] to [-b,-a]

  BOOL Evaluate( // returns FALSE if unable to evaluate
	 double,         // evaluation parameter
	 int,            // number of derivatives (>=0)
	 int,            // array stride (>=Dimension())
	 double*,        // array of length stride*(ndir+1)
	 int = 0,        // optional - determines which side to evaluate from
			 //         0 = default
			 //      <  0 to evaluate from below, 
			 //      >  0 to evaluate from above
	 int* = 0        // optional - evaluation hint (int) used to speed
			 //            repeated evaluations
	 ) const;

  int GetNurbForm( // returns 0: unable to create NURBS representation
		   //            with desired accuracy.
		   //         1: success - returned NURBS parameterization
		   //            matches the curve's to wthe desired accuracy
		   //         2: success - returned NURBS point locus matches
		   //            the curve's to the desired accuracy but, on
		   //            the interior of the curve's domain, the 
		   //            curve's parameterization and the NURBS
		   //            parameterization may not match to the 
		   //            desired accuracy.
	ON_NurbsCurve&,
	double = 0.0,
	const ON_Interval* = NULL     // OPTIONAL subdomain of 2d curve
	) const;

  /////////////////////////////////////////////////////////////////
  // Interface

  // ~ON_CurveOnSurface() deletes these classes.  Use a
  // ON_CurveProxy and/or ON_SurfaceProxy wrapper if you don't want
  // the destructor to destroy the curves
  ON_Curve* m_c2;  // REQUIRED parameter space (2d) curve
  ON_Curve* m_c3;  // OPTIONAL 3d curve (approximation) to srf(crv2(t))
  ON_Surface* m_s; 
};


#endif
