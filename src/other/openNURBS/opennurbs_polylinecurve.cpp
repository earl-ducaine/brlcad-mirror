/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2001 Robert McNeel & Associates. All rights reserved.
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

#include "opennurbs.h"

ON_OBJECT_IMPLEMENT(ON_PolylineCurve,ON_Curve,"4ED7D4E6-E947-11d3-BFE5-0010830122F0");

ON_PolylineCurve::ON_PolylineCurve()
{
  m_dim = 3;
}

ON_PolylineCurve::ON_PolylineCurve( const ON_PolylineCurve& L )
{
  *this = L;
}

ON_PolylineCurve::ON_PolylineCurve( const ON_3dPointArray& L )
{
  *this = L;
}

ON_PolylineCurve::~ON_PolylineCurve()
{
}

unsigned int ON_PolylineCurve::SizeOf() const
{
  unsigned int sz = ON_Curve::SizeOf();
  sz += (sizeof(*this) - sizeof(ON_Curve));
  sz += m_pline.SizeOfArray();
  sz += m_t.SizeOfArray();
  return sz;
}

ON__UINT32 ON_PolylineCurve::DataCRC(ON__UINT32 current_remainder) const
{
  current_remainder = m_pline.DataCRC(current_remainder);
  current_remainder = m_t.DataCRC(current_remainder);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_dim),&m_dim);
  return current_remainder;
}

void ON_PolylineCurve::EmergencyDestroy()
{
  m_pline.EmergencyDestroy();
  m_t.EmergencyDestroy();
}

ON_PolylineCurve& ON_PolylineCurve::operator=( const ON_PolylineCurve& src )
{
  if ( this != &src ) {
    ON_Curve::operator=(src);
    m_pline = src.m_pline;
    m_t     = src.m_t;
    m_dim   = src.m_dim;
  }
  return *this;
}

ON_PolylineCurve& ON_PolylineCurve::operator=( const ON_3dPointArray& src )
{
  m_pline = src;
  m_dim   = 3;
  const int count = src.Count();
  m_t.Reserve(count);
  m_t.SetCount(count);
  int i;
  for (i = 0; i < count; i++) {
    m_t[i] = (double)i;
  }
  return *this;
}

int ON_PolylineCurve::Dimension() const
{
  return m_dim;
}

BOOL
ON_PolylineCurve::GetBBox( // returns TRUE if successful
         double* boxmin,    // minimum
         double* boxmax,    // maximum
         BOOL bGrowBox
         ) const
{
  return ON_GetPointListBoundingBox( m_dim, FALSE, PointCount(), 3, m_pline[0],
                        boxmin, boxmax, bGrowBox?true:false
                        );
}


BOOL
ON_PolylineCurve::Transform( const ON_Xform& xform )
{
  TransformUserData(xform);
	DestroyCurveTree();
  return m_pline.Transform( xform );
}



BOOL
ON_PolylineCurve::SwapCoordinates( int i, int j )
{
	DestroyCurveTree();
  return m_pline.SwapCoordinates(i,j);
}

BOOL ON_PolylineCurve::IsValid( ON_TextLog* text_log ) const
{
  BOOL rc = false;
  const int count = PointCount();
  if ( count >= 2 && count == m_t.Count() )
  {
    rc = m_pline.IsValid();
    int i;
    for ( i = 1; rc && i < count; i++ )
    {
      if ( m_t[i] <= m_t[i-1] )
      {
        rc = false;
        if ( 0 != text_log )
        {
          text_log->Print("PolylineCurve m_t[%d]=%g should be less than m_t[%d]=(%g).\n",
                           i-1,m_t[i-1],i,m_t[i]);
        }
      }
    }
    if ( rc )
    {
      if (m_dim < 2 || m_dim > 3 )
      {
        rc = false;
        if (0 != text_log )
          text_log->Print("PolylineCurve m_dim = %d (should be 2 or 3).\n",m_dim);
      }
    }
  }
  else if ( 0 != text_log )
  {
    if ( count < 2 )
      text_log->Print("PolylineCurve has %d points (should be >= 2)\n",count);
    else
      text_log->Print("PolylineCurve m_t.Count() = %d and PointCount() = %d (should be equal)\n",
                      m_t.Count(),count);
  }

  return rc;
}

void ON_PolylineCurve::Dump( ON_TextLog& dump ) const
{
  ON_Interval d = Domain();
  dump.Print( "ON_PolylineCurve:  domain = [%g,%g]\n",d[0],d[1]);
  for ( int i = 0; i < PointCount(); i++ ) {
    dump.Print( "  point[%2d] = ",i);
    dump.Print( m_pline[i] );
    dump.Print( ", %g\n",m_t[i]);
  }
}

BOOL ON_PolylineCurve::Write( ON_BinaryArchive& file ) const
{
  BOOL rc = file.Write3dmChunkVersion(1,0);
  if (rc) {
    if (rc) rc = file.WriteArray( m_pline );
    if (rc) rc = file.WriteArray( m_t );
    if (rc) rc = file.WriteInt(m_dim);
  }
  return rc;
}

BOOL ON_PolylineCurve::Read( ON_BinaryArchive& file )
{
  int major_version = 0;
  int minor_version = 0;
  BOOL rc = file.Read3dmChunkVersion(&major_version,&minor_version);
  if (rc && major_version==1) {
    // common to all 1.x versions
    if (rc) rc = file.ReadArray( m_pline );
    if (rc) rc = file.ReadArray( m_t );
    if (rc) rc = file.ReadInt(&m_dim);
  }
  return rc;
}

ON_Interval ON_PolylineCurve::Domain() const
{
  ON_Interval d;
  //BOOL rc = FALSE;
  const int count = PointCount();
  if ( count >= 2 && m_t[0] < m_t[count-1] ) {
    d.Set(m_t[0],m_t[count-1]);
  }
  return d;
}

BOOL ON_PolylineCurve::SetDomain( double t0, double t1 )
{
  BOOL rc = FALSE;
  const int count = m_t.Count()-1;
  if ( count >= 1 )
  {
    if ( t0 == m_t[0] && t1 == m_t[count] )
      rc = TRUE;
    else if ( t0 < t1 )
    {
      const ON_Interval old_domain = Domain();
      const ON_Interval new_domain(t0,t1);
      m_t[0] = t0;
      m_t[count] = t1;
      for ( int i = 1; i < count; i++ )
      {
        m_t[i] = new_domain.ParameterAt( old_domain.NormalizedParameterAt(m_t[i]) );
      }
			rc=TRUE;
    }
  }
	DestroyCurveTree();
  return rc;
}

bool ON_PolylineCurve::ChangeDimension( int desired_dimension )
{
  bool rc = (desired_dimension>=2 && desired_dimension<=3);

  if ( rc && m_dim != desired_dimension )
  {
  	DestroyCurveTree();
    int i, count = m_pline.Count();
    if ( 2 == desired_dimension )
    {
      if ( count > 0 )
      {
        // 7 April 2003 Dale Lear:
        //   If x coord of first point is set, then
        //   zero all z coords.
        if ( ON_UNSET_VALUE != m_pline[0].x )
        {
          for ( i = 0; i < count; i++ )
            m_pline[i].z = 0.0;
        }
      }
      m_dim = 2;
    }
    else
    {
      if ( count > 0 )
      {
        // 7 April 2003 Dale Lear:
        //   If first point x coord is set and z is unset, then
        //   zero all z coords.
        if ( ON_UNSET_VALUE != m_pline[0].x && ON_UNSET_VALUE == m_pline[0].z )
        {
          for ( i = 0; i < count; i++ )
            m_pline[i].z = 0.0;
        }
      }
      m_dim = 3;
    }
  }

  return rc;
}


BOOL ON_PolylineCurve::ChangeClosedCurveSeam( double t )
{
  const ON_Interval old_dom = Domain();
  BOOL rc = IsClosed();
  if ( rc )
  {
    double k = t;
    if ( !old_dom.Includes(t) )
    {
      double s = old_dom.NormalizedParameterAt(t);
      s = fmod(s,1.0);
      if ( s < 0.0 )
        s += 1.0;
      k = old_dom.ParameterAt(s);
    }
    if ( old_dom.Includes(k,true) )
    {
      int old_count = PointCount();
      int i = ON_NurbsSpanIndex(2,old_count,m_t.Array(),k,0,0);
      if ( k < m_t[i] )
        return FALSE;
      if ( k >= m_t[i+1] )
        return FALSE;
      int new_count = (k==m_t[i]) ? old_count : old_count+1;
      ON_SimpleArray<ON_3dPoint> new_pt(new_count);
      ON_SimpleArray<double> new_t(new_count);
      ON_3dPoint new_start = (k==m_t[i]) ? m_pline[i] : PointAt(k);
      new_pt.Append( new_start );
      new_t.Append(k);
      int n = old_count-i-1;
      new_pt.Append( n, m_pline.Array() + i+1 );
      new_t.Append( n, m_t.Array() + i+1 );

      int j = new_t.Count();

      n = new_count-old_count+i-1;
      new_pt.Append( n, m_pline.Array() + 1 );
      new_t.Append(  n, m_t.Array() + 1 );

      new_pt.Append( new_start );
      new_t.Append(k);

      double d = old_dom.Length();
      while ( j  < new_t.Count() )
      {
        new_t[j] += d;
        j++;
      }

      m_pline = new_pt;
      m_t = new_t;
    }
    else
    {
      // k already at start end of this curve
      rc = TRUE;
    }
    if ( rc )
      SetDomain( t, t + old_dom.Length() );
  }
  return rc;
}

int ON_PolylineCurve::SpanCount() const
{
  return m_pline.SegmentCount();
}

BOOL ON_PolylineCurve::GetSpanVector( // span "knots"
       double* s // array of length SpanCount() + 1
       ) const
{
  BOOL rc = FALSE;
  const int count = PointCount();
  if ( count >= 1 )
  {
    memcpy( s, m_t.Array(), count*sizeof(*s) );
    rc = TRUE;
  }
  return rc;
}

int ON_PolylineCurve::Degree() const
{
  return 1;
}


BOOL
ON_PolylineCurve::IsLinear( // TRUE if curve locus is a line segment
      double tolerance // tolerance to use when checking linearity
      ) const
{
  BOOL rc = FALSE;
  ON_NurbsCurve nurbs_curve;
  nurbs_curve.m_dim = m_dim;
  nurbs_curve.m_is_rat = 0;
  nurbs_curve.m_order = 2;
  nurbs_curve.m_cv_count = m_pline.Count();
  if ( nurbs_curve.m_cv_count >= 2 )
  {
    nurbs_curve.m_cv = const_cast<double*>(&m_pline[0].x);
    nurbs_curve.m_cv_stride = (int)(&m_pline[1].x - nurbs_curve.m_cv); // the int converts 64 bit size_t
    nurbs_curve.m_knot = const_cast<double*>(m_t.Array());
    // using ptr to make sure we go through vtable
    const ON_Curve* ptr = &nurbs_curve;
    rc = ptr->IsLinear(tolerance);
    nurbs_curve.m_cv = 0;
    nurbs_curve.m_knot = 0;
  }
  return rc;
}

int ON_PolylineCurve::IsPolyline(
      ON_SimpleArray<ON_3dPoint>* pline_points,
      ON_SimpleArray<double>* pline_t
      ) const
{
  if ( pline_points )
    pline_points->SetCount(0);
  if ( pline_t )
    pline_t->SetCount(0);
  int rc = this->PointCount();
  if ( rc >= 2 )
  {
    if ( pline_points )
      pline_points->operator=(m_pline);
    if ( pline_t )
      pline_t->operator=(m_t);
  }
  else
    rc = 0;
  return rc;
}

BOOL
ON_PolylineCurve::IsArc( // TRUE if curve locus in an arc or circle
      const ON_Plane* plane, // if not NULL, test is performed in this plane
      ON_Arc* arc,         // if not NULL and TRUE is returned, then arc
                              // arc parameters are filled in
      double tolerance // tolerance to use when checking linearity
      ) const
{
  return FALSE;
}


BOOL
ON_PolylineCurve::IsPlanar(
      ON_Plane* plane, // if not NULL and TRUE is returned, then plane parameters
                         // are filled in
      double tolerance // tolerance to use when checking linearity
      ) const
{
  BOOL rc = FALSE;
  ON_NurbsCurve nurbs_curve;
  nurbs_curve.m_dim = m_dim;
  nurbs_curve.m_is_rat = 0;
  nurbs_curve.m_order = 2;
  nurbs_curve.m_cv_count = m_pline.Count();
  if ( nurbs_curve.m_cv_count >= 2 )
  {
    if (m_dim == 2 )
    {
      rc = ON_Curve::IsPlanar(plane,tolerance);
    }
    else
    {
      nurbs_curve.m_cv = const_cast<double*>(&m_pline[0].x);
      nurbs_curve.m_cv_stride = (int)(&m_pline[1].x - nurbs_curve.m_cv); // the (int) converts 64 bit size_t
      nurbs_curve.m_knot = const_cast<double*>(m_t.Array());
      // using ptr to make sure we go through vtable
      const ON_Curve* ptr = &nurbs_curve;
      rc = ptr->IsPlanar(plane,tolerance);
      nurbs_curve.m_cv = 0;
      nurbs_curve.m_knot = 0;
    }
  }
  return rc;
}

BOOL
ON_PolylineCurve::IsInPlane(
      const ON_Plane& plane, // plane to test
      double tolerance // tolerance to use when checking linearity
      ) const
{
  BOOL rc = FALSE;
  ON_NurbsCurve nurbs_curve;
  nurbs_curve.m_dim = m_dim;
  nurbs_curve.m_is_rat = 0;
  nurbs_curve.m_order = 2;
  nurbs_curve.m_cv_count = m_pline.Count();
  if ( nurbs_curve.m_cv_count >= 2 )
  {
    nurbs_curve.m_cv = const_cast<double*>(&m_pline[0].x);
    nurbs_curve.m_cv_stride = (int)(&m_pline[1].x - nurbs_curve.m_cv);
    nurbs_curve.m_knot = const_cast<double*>(m_t.Array());
    rc = nurbs_curve.IsInPlane(plane,tolerance);
    nurbs_curve.m_cv = 0;
    nurbs_curve.m_knot = 0;
  }
  return rc;
}

BOOL
ON_PolylineCurve::IsClosed() const
{
  return m_pline.IsClosed(0.0);
}

BOOL
ON_PolylineCurve::IsPeriodic() const
{
  return FALSE;
}

bool ON_PolylineCurve::GetNextDiscontinuity(
                ON::continuity c,
                double t0,
                double t1,
                double* t,
                int* hint,
                int* dtype,
                double cos_angle_tolerance,
                double curvature_tolerance
                ) const
{
  bool rc = false;

  const int segment_count = m_pline.SegmentCount();

  if ( segment_count > 0 && t0 != t1 )
  {
    ON_Interval domain = Domain();

    if ( t0 < t1 )
    {
      if ( t0 < domain[0] )
        t0 = domain[0];
      if ( t1 > domain[1] )
        t1 = domain[1];
      if ( t0 >= t1 )
        return false;
    }
    else if ( t0 > t1 )
    {
      if ( t1 < domain[0] )
        t1 = domain[0];
      if ( t0 > domain[1] )
        t0 = domain[1];
      if ( t1 >= t0 )
        return false;
    }

    if ( t0 != t1 )
    {
      ON_3dPoint Pm, Pp;
      ON_3dVector D1m, D1p, Tm, Tp;

      if ( dtype )
        *dtype = 0;
      ON::continuity parametric_c = ON::ParametricContinuity(c);
      if ( segment_count >= 2 && parametric_c != ON::C0_continuous )
      {
        int i = 0;
        int delta_i = 1;
        double s0 = t0;
        double s1 = t1;
        i = ON_NurbsSpanIndex(2,PointCount(),m_t,t0,0,(hint)?*hint:0);
        double segtol = (fabs(m_t[i]) + fabs(m_t[i+1]) + fabs(m_t[i+1]-m_t[i]))*ON_SQRT_EPSILON;
        if ( t0 < t1 )
        {
          if ( t0 < m_t[i+1] && t1 > m_t[i+1] && (m_t[i+1]-t0) <= segtol && i+1 < PointCount() )
          {
            t0 = m_t[i+1];
            i = ON_NurbsSpanIndex(2,PointCount(),m_t,t0,0,(hint)?*hint:0);
          }
          if ( hint )
            *hint = i;
          i++; // start checking at first m_t[i] > t0
        }
        else if ( t0 > t1 )
        {
          // Check backwards (have to handle this case so
          // ON_CurveProxy::GetNextDiscontinuity() works on
          // reversed proxy curves.
          if ( t0 > m_t[i] && t1 < m_t[i] && (t0-m_t[i]) <= segtol && i > 0 )
          {
            t0 = m_t[i];
            i = ON_NurbsSpanIndex(2,PointCount(),m_t,t0,0,(hint)?*hint:0);
          }
          if ( hint )
            *hint = i;
          if ( t0 == m_t[i] )
            i--;
          delta_i = -1;
          s0 = t1;
          s1 = t0;
        }
        for ( /*empty*/; !rc && 0 < i && i < segment_count && s0 < m_t[i] && m_t[i] < s1; i += delta_i )
        {
          Ev1Der(m_t[i], Pm, D1m, -1, hint );
          Ev1Der(m_t[i], Pp, D1p, +1, hint );
          if ( parametric_c == ON::C1_continuous || parametric_c == ON::C2_continuous )
          {
            if ( !(D1m-D1p).IsTiny(D1m.MaximumCoordinate()*ON_SQRT_EPSILON) )
              rc = TRUE;
          }
          else if ( parametric_c == ON::G1_continuous || parametric_c == ON::G2_continuous )
          {
            Tm = D1m;
            Tp = D1p;
            Tm.Unitize();
            Tp.Unitize();
            if ( Tm*Tp < cos_angle_tolerance )
              rc = TRUE;
          }
          if ( rc )
          {
            if ( dtype )
              *dtype = 1;
            if ( t )
              *t = m_t[i];
            break;
          }
        }
      }

      if ( !rc && segment_count > 0 && parametric_c != c )
      {
        // 20 March 2003 Dale Lear:
        //   Let base class test for locus continuities at start/end.
        rc = ON_Curve::GetNextDiscontinuity( c, t0, t1, t, hint, dtype, cos_angle_tolerance, curvature_tolerance );
      }
    }
  }

  return rc;
}

bool ON_PolylineCurve::IsContinuous(
    ON::continuity desired_continuity,
    double t,
    int* hint, // default = NULL,
    double point_tolerance, // default=ON_ZERO_TOLERANCE
    double d1_tolerance, // default==ON_ZERO_TOLERANCE
    double d2_tolerance, // default==ON_ZERO_TOLERANCE
    double cos_angle_tolerance, // default==0.99984769515639123915701155881391
    double curvature_tolerance  // default==ON_SQRT_EPSILON
    ) const
{
  bool rc = TRUE;
  const int segment_count = m_pline.SegmentCount();

  if ( segment_count >= 1 )
  {
    bool bPerformTest = false;

    switch(desired_continuity)
    {
    case ON::C2_continuous:
      // on a polyline C1 impiles C2
      desired_continuity = ON::C1_continuous;
      break;
    case ON::G2_continuous:
      // on a polyline G1 impiles G2
      desired_continuity = ON::G1_continuous;
      break;
    case ON::C2_locus_continuous:
      // on a polyline C1 impiles C2
      desired_continuity = ON::C1_locus_continuous;
      break;
    case ON::G2_locus_continuous:
      // on a polyline G1 impiles G2
      desired_continuity = ON::G1_locus_continuous;
      break;
    default:
      // intentionally ignoring other ON::continuity enum values
      break;
    }

    if ( t <= m_t[0] || t >= m_t[segment_count] )
    {
      // 20 March 2003 Dale Lear
      //     Consistently handles locus case and out of domain case.
      switch(desired_continuity)
      {
      case ON::C0_locus_continuous:
      case ON::C1_locus_continuous:
      case ON::G1_locus_continuous:
        bPerformTest = true;
        break;
      default:
        // intentionally ignoring other ON::continuity enum values
        break;
      }
    }
    else
    {
      if ( segment_count >= 2 && desired_continuity != ON::C0_continuous )
      {
        int i = ON_NurbsSpanIndex(2,PointCount(),m_t,t,0,(hint)?*hint:0);

        {
          // 20 March 2003 Dale Lear:
          //     If t is very near interior m_t[] value, see if it
          //     should be set to that value.  A bit or two of
          //     precision sometimes gets lost in proxy
          //     domain to real curve domain conversions on the interior
          //     of a curve domain.
          double segtol = (fabs(m_t[i]) + fabs(m_t[i+1]) + fabs(m_t[i+1]-m_t[i]))*ON_SQRT_EPSILON;
          if ( m_t[i]+segtol < m_t[i+1]-segtol )
          {
            if ( fabs(t-m_t[i]) <= segtol && i > 0 )
            {
              t = m_t[i];
            }
            else if ( fabs(t-m_t[i+1]) <= segtol && i+1 < PointCount() )
            {
              t = m_t[i+1];
              i = ON_NurbsSpanIndex(2,PointCount(),m_t,t,0,(hint)?*hint:0);
            }
          }
        }

        if ( hint )
          *hint = i;
        if ( i > 0 && i < segment_count && t == m_t[i] )
        {
          // "locus" and "parametric" tests are the same at this point.
          desired_continuity = ON::ParametricContinuity(desired_continuity);
          bPerformTest = true;
        }
      }
    }

    if ( bPerformTest )
    {
      // need to evaluate and test
      rc = ON_Curve::IsContinuous( desired_continuity, t, hint,
                                   point_tolerance, d1_tolerance, d2_tolerance,
                                   cos_angle_tolerance, curvature_tolerance );
    }
  }

  return rc;
}

BOOL
ON_PolylineCurve::Reverse()
{
  BOOL rc = FALSE;
  const int count = PointCount();
  if ( count >= 2 ) {
    m_pline.Reverse();
    m_t.Reverse();
    double* t = m_t.Array();
    for ( int i = 0; i < count; i++ ) {
      t[i] = -t[i];
    }
    rc = TRUE;
  }
	DestroyCurveTree();
  return rc;
}

BOOL ON_PolylineCurve::SetStartPoint(
        ON_3dPoint start_point
        )
{
  BOOL rc = FALSE;
  if ( !IsClosed() && m_pline.Count() >= 2 )
  {
    m_pline[0] = start_point;
    rc = TRUE;
  }
	DestroyCurveTree();
  return rc;
}

BOOL ON_PolylineCurve::SetEndPoint(
        ON_3dPoint end_point
        )
{
  BOOL rc = FALSE;
  if ( !IsClosed() && m_pline.Count() >= 2 )
  {
    m_pline[m_pline.Count()-1] = end_point;
    rc = TRUE;
  }
	DestroyCurveTree();
  return rc;
}

BOOL
ON_PolylineCurve::Evaluate( // returns FALSE if unable to evaluate
       double t,       // evaluation parameter
       int der_count,  // number of derivatives (>=0)
       int v_stride,   // v[] array stride (>=Dimension())
       double* v,      // v[] array of length stride*(ndir+1)
       int side,       // optional - determines which side to evaluate from
                       //         0 = default
                       //      <  0 to evaluate from below,
                       //      >  0 to evaluate from above
       int* hint       // optional - evaluation hint (int) used to speed
                       //            repeated evaluations
       ) const
{
  BOOL rc = FALSE;
  const int count = PointCount();
  if ( count >= 2 ) {
    const int segment_index = ON_NurbsSpanIndex(2,count,m_t,t,side,(hint)?*hint:0);
    const double t0 = m_t[segment_index];
    const double t1 = m_t[segment_index+1];
    double s = (t == t1) ? 1.0 : (t-t0)/(t1-t0);
    const ON_3dPoint p = (1.0-s)*m_pline[segment_index] + s*m_pline[segment_index+1];
    v[0] = p.x;
    v[1] = p.y;
    if ( m_dim == 3 )
      v[2] = p.z;
    if ( der_count >= 1 ) {
      v += v_stride;
      ON_3dVector d = 1.0/(t1-t0)*(m_pline[segment_index+1] - m_pline[segment_index]);
      v[0] = d.x;
      v[1] = d.y;
      if ( m_dim == 3 )
        v[2] = d.z;
    }
    for ( int di = 2; di <= der_count; di++ ) {
      v += v_stride;
      v[0] = 0.0;
      v[1] = 0.0;
      if ( m_dim == 3 )
        v[2] = 0.0;
    }
    if ( hint )
      *hint = segment_index;
    rc = TRUE;
  }
  return rc;
}

BOOL
ON_PolylineCurve::PointCount() const
{
  return m_pline.PointCount();
}

bool ON_PolylineCurve::GetClosestPoint( const ON_3dPoint& test_point,
        double* t,       // parameter of local closest point returned here
        double maximum_distance,
        const ON_Interval* sub_domain
        ) const
{
  double s, d;
  int segment_index0 = 0;
  int segment_index1 = m_pline.SegmentCount();
  if ( sub_domain ) {
    segment_index0 = ON_NurbsSpanIndex(2,m_pline.PointCount(),m_t,sub_domain->Min(),1,0);
    segment_index1 = ON_NurbsSpanIndex(2,m_pline.PointCount(),m_t,sub_domain->Max(),-1,0)+1;
  }
  bool rc = m_pline.ClosestPointTo( test_point, &s, segment_index0, segment_index1 );
  if ( rc ) {
    int i = (int)floor(s);
    if ( i < 0 )
      i = 0;
    else if ( i >= m_pline.PointCount()-1 )
      i = m_pline.PointCount()-2;
    ON_Interval in(m_t[i],m_t[i+1]);
    s = in.ParameterAt(s-i);
    if ( sub_domain ) {
      if ( s < sub_domain->Min() )
        s = sub_domain->Min();
      else if ( s > sub_domain->Max() )
        s = sub_domain->Max();
    }
    if ( maximum_distance > 0.0 ) {
      d = test_point.DistanceTo(PointAt(s));
      if ( d > maximum_distance )
        rc = FALSE;
    }
    if (rc && t)
      *t = s;
  }
  return rc;
}

BOOL ON_PolylineCurve::GetLocalClosestPoint( const ON_3dPoint& test_point,
        double seed_parameter,
        double* t,
        const ON_Interval* sub_domain
        ) const
{
  BOOL rc;
  if ( m_pline.Count() <= 2 )
  {
    // no need for complicated test in common and simple case of a line.
    rc = GetClosestPoint(test_point,t,0.0,sub_domain);
    if ( rc
         && t
         && test_point.DistanceTo(PointAt(seed_parameter)) <= test_point.DistanceTo(PointAt(*t))
         )
    {
      *t = seed_parameter;
    }
  }
  else
  {
    // Use the local closest point finder for nurbs curves
    ON_NurbsCurve nurbs_curve;
    nurbs_curve.m_dim = m_dim;
    nurbs_curve.m_is_rat = 0;
    nurbs_curve.m_order = 2;
    nurbs_curve.m_cv_count = m_pline.Count();
    nurbs_curve.m_cv_stride = 3;
    nurbs_curve.m_cv_capacity = 0;
    nurbs_curve.m_knot_capacity = 0;

    // share the point and parameter memory
    nurbs_curve.m_cv = const_cast<double*>(&m_pline[0].x);
    nurbs_curve.m_knot = const_cast<double*>(&m_t[0]);

    rc = nurbs_curve.GetLocalClosestPoint(test_point,seed_parameter,t,sub_domain);

    // Prevent ~ON_NurbsCurve from deleting of this polyline's
    // points and knots. Setting the nurbs_curve.m_*_capacity = 0
    // should be enough, but this is fast and bulletproof.
    nurbs_curve.m_cv = 0;
    nurbs_curve.m_knot = 0;
  }

  return rc;
}

BOOL ON_PolylineCurve::GetLength(
        double* length,               // length returned here
        double fractional_tolerance,  // default = 1.0e-8
        const ON_Interval* sub_domain // default = NULL
        ) const
{
	if (length==NULL)
		return FALSE;

  if ( sub_domain ) {
    *length = 0.0;
		if(sub_domain->IsDecreasing())
			return FALSE;

    int i, count = m_t.Count();
		if( count<1)
			return TRUE;

		ON_Interval scratch_domain= ON_Interval(*m_t.Last(), m_t[0]);
		if(scratch_domain.Intersection(*sub_domain))
			sub_domain = &scratch_domain;
		else
			return FALSE;

    double s0 = sub_domain->Min();
    double s1 = sub_domain->Max();
    ON_3dPoint p0, p1;
    if ( s0 < m_t[0] )
      s0 = m_t[0];
    if ( s1 > m_t[count-1] )
      s1 = m_t[count-1];
    p1 = m_pline[0];
    for ( i = 1; i < count; i++ ) {
      if ( s0 < m_t[i] ) {
        p1 = PointAt(s0);
        break;
      }
    }
    for (/*empty*/; i < count; i++ ) {
      p0 = p1;
      if ( s1 < m_t[i] ) {
        p1 = PointAt(s1);
        *length += p0.DistanceTo(p1);
        break;
      }
      else {
        p1 = m_pline[i];
        *length += p0.DistanceTo(m_pline[i]);
      }
    }
  }
  else {
    *length = m_pline.Length();
  }
  return TRUE;
}


bool ON_PolylineCurve::Append( const ON_PolylineCurve& c )
{

  if ( PointCount() == 0 ) {
    *this = c;
    return IsValid() ? true : false;
  }

  if (!IsValid() || !c.IsValid())
    return false;

  if ( c.Dimension() == 3 &&  Dimension() == 2)
    m_dim = 3;

  m_pline.Remove();
  m_pline.Append(c.m_pline.Count(), c.m_pline.Array());
  m_t.Reserve(m_t.Count()+c.m_t.Count()-1);
  double del = *m_t.Last() - c.m_t[0];
  int i;
  for (i=1; i<c.m_t.Count(); i++)
    m_t.Append(c.m_t[i] + del);

  return true;
}

BOOL ON_PolylineCurve::GetNormalizedArcLengthPoint(
        double s,
        double* t,
        double fractional_tolerance,
        const ON_Interval* sub_domain
        ) const
{
  BOOL rc = TRUE;
  ON_Interval domain = (sub_domain) ? *sub_domain : Domain();
  if ( s == 0.0 )
    *t = domain.Min();
  else if ( s == 1.0 )
    *t = domain.Max();
  else if ( 0.0 < s && s < 1.0 )
  {
    int segment_count = m_pline.SegmentCount();
    double length;
    rc = GetLength(&length,fractional_tolerance,sub_domain);
    if ( rc ) {
      rc = FALSE;
      double seg_length, s_length = s*length;
      int i = ON_SearchMonotoneArray( m_t, m_t.Count(), domain[0] );
      if ( i < 0 )
        i = 0;
      else if (i >= m_t.Count())
        i = m_t.Count()-1;
      for( /*empty*/; i < segment_count; i++ )
      {
        if ( m_t[i] > domain[1] )
          break;
        seg_length = m_pline[i].DistanceTo(m_pline[i+1]);
        if ( seg_length < s_length )
          s_length -= seg_length;
        else if ( s_length >= seg_length )
        {
          *t = m_t[i+1];
          rc = (*t <= domain[1]);
          break;
        }
        else {
          ON_Interval seg_domain(m_t[i],m_t[i+1]);
          *t = seg_domain.ParameterAt( s_length/seg_length );
          rc = (*t <= domain[1]);
          break;
        }
      }
    }
  }
  else
    rc = FALSE;
  return rc;
}

BOOL ON_PolylineCurve::GetNormalizedArcLengthPoints(
        int count,
        const double* s,
        double* t,
        double absolute_tolerance,
        double fractional_tolerance,
        const ON_Interval* sub_domain
        ) const
{
  // TODO: move actual calculations into this function and
  // have GetNormalizedArcLengthPoint() call here.
  BOOL rc = TRUE;
  if ( count > 0 || s != NULL && t != NULL )
  {
    int i;
    for ( i = 0; i < count && rc ; i++ )
    {
      rc = GetNormalizedArcLengthPoint( s[i], &t[i], fractional_tolerance, sub_domain );
    }
  }
  return rc;
}



// returns true if t is sufficiently close to m_t[index]
bool ON_PolylineCurve::ParameterSearch(double t, int& index, bool bEnableSnap) const{
	return ON_Curve::ParameterSearch( t, index,bEnableSnap, m_t, ON_SQRT_EPSILON);
}


BOOL ON_PolylineCurve::Trim( const ON_Interval& domain )
{
  int segment_count = m_t.Count()-1;

	if ( segment_count < 1 || m_t.Count() != m_pline.Count() || !domain.IsIncreasing() )
    return false;

  const ON_Interval original_polyline_domain = Domain();
  if ( !original_polyline_domain.IsIncreasing() )
    return false;

  ON_Interval output_domain = domain;
  if ( !output_domain.Intersection(original_polyline_domain) )
    return false;
	if(!output_domain.IsIncreasing())
		return false;

  ON_Interval actual_trim_domain = output_domain;

  int i, j;
  int s0 = -2; // s0 gets set to index of first segment we keep
  int s1 = -3; // s1 gets set to index of last segment we keep

	if ( ParameterSearch(output_domain[0], s0, true ) )
  {
    // ParameterSearch says domain[0] is within "microtol" of
    // m_t[s0].  So we will actually trim at m_t[s0].
    if (s0 >= 0 && s0 <= segment_count)
    {
      actual_trim_domain[0]=m_t[s0];
    }
  }

	if ( ParameterSearch(output_domain[1], s1, true ) )
  {
    if (s1 >= 0 && s1 <= segment_count )
    {
      // ParameterSearch says domain[1] is within "microtol" of
      // m_t[s1].  So we will actually trim at m_t[s1].
      actual_trim_domain[1]=m_t[s1];
      s1--;
    }
  }

  if ( !actual_trim_domain.IsIncreasing() )
  {
    // After microtol snapping, there is not enough curve left to trim.
    return false;
  }

  if ( s0 < 0 || s0 > s1 || s1 >= segment_count )
  {
    // Because output_domain is a subinterval of original_polyline_domain,
    // the only way that (s0 < 0 || s0 > s1 || s1 >= segment_count) can be true
    // is if something is seriously wrong with the m_t[] values.
    return false;
  }

  // we will begin modifying the polyline
  DestroyCurveTree();

  if ( actual_trim_domain == original_polyline_domain )
  {
    // ParameterSearch says that the ends of output_domain
    // were microtol away from being the entire curve.
    // Set the domain and return.
    m_t[0] = output_domain[0];
    m_t[segment_count] = output_domain[1];
    return true;
  }

  if ( s1 < segment_count-1 )
  {
    m_t.SetCount(s1+2);
    m_pline.SetCount(s1+2);
    segment_count = s1+1;
  }

  if ( s0 > 0 )
  {
    double* tmp_t = m_t.Array();
    ON_3dPoint* tmp_P = m_pline.Array();
    for ( i = 0, j = s0; j <= segment_count; i++, j++ )
    {
      tmp_t[i] = tmp_t[j];
      tmp_P[i] = tmp_P[j];
    }
    s1 -= s0;
    s0 = 0;
    m_t.SetCount(s1+2);
    m_pline.SetCount(s1+2);
    segment_count = s1+1;
  }

  bool bTrimFirstSegment = ( m_t[0] < actual_trim_domain[0] || (0 == s1 && actual_trim_domain[1] < m_t[1]) );
  bool bTrimLastSegment = (s1>s0 && m_t[s1] < actual_trim_domain[1] && actual_trim_domain[1] < m_t[s1+1]);

  if ( bTrimFirstSegment )
  {
    ON_Interval seg_domain(m_t[0],m_t[1]);
    ON_3dPoint Q0 = m_pline[0];
    ON_3dPoint Q1 = m_pline[1];
    ON_Line seg_chord(Q0,Q1);
    double np0 = 0.0;
    double np1 = 1.0;
    bool bSet0 = false;
    bool bSet1 = false;
    if ( m_t[0] < actual_trim_domain[0] && actual_trim_domain[0] < m_t[1] )
    {
      np0 = seg_domain.NormalizedParameterAt(actual_trim_domain[0]);
      Q0 = seg_chord.PointAt( np0 );
      bSet0 = true;
    }
    if ( 0 == s1 && m_t[0] < actual_trim_domain[1] && actual_trim_domain[1] < m_t[1] )
    {
      np1 = seg_domain.NormalizedParameterAt(actual_trim_domain[1]);
      Q1 = seg_chord.PointAt( np1 );
      bSet1 = true;
    }

    if ( np0 >= np1 )
      return false; // trim is not viable

    if ( bSet0 )
    {
      if ( np0 >= 1.0-ON_SQRT_EPSILON && Q0.DistanceTo(Q1) <= ON_ZERO_TOLERANCE && s1>0 && m_t[1] < actual_trim_domain[1] )
      {
        // trim will leave a micro segment at the start - just remove the first segment
        m_t.Remove(0);
        m_pline.Remove(0);
        s1--;
        segment_count--;
        actual_trim_domain[0] = m_t[0];
      }
      m_t[0] = actual_trim_domain[0];
      m_pline[0] = Q0;
    }
    if ( bSet1 )
    {
      m_t[1] = actual_trim_domain[1];
      m_pline[1] = Q1;
    }
  }

  if ( bTrimLastSegment )
  {
    ON_Interval seg_domain(m_t[s1],m_t[s1+1]);
    ON_3dPoint Q0 = m_pline[s1];
    ON_3dPoint Q1 = m_pline[s1+1];
    ON_Line seg_chord(Q0,Q1);
    double np = seg_domain.NormalizedParameterAt(actual_trim_domain[1]);
    Q1 = seg_chord.PointAt(np);
    if ( np <= ON_SQRT_EPSILON && Q1.DistanceTo(Q0) <= ON_ZERO_TOLERANCE && s1 > 0 )
    {
        // trim will leave a micro segment at the end - just remove the last segment
      m_pline.SetCount(s1+1);
      m_t.SetCount(s1+1);
      s1--;
      segment_count--;
      actual_trim_domain[1] = m_t[s1+1];
    }
    m_t[s1+1] = actual_trim_domain[1];
    m_pline[s1+1] = Q1;
  }

  // If we get this far, trims were is successful.
  // The following makes potential tiny adjustments
  // that need to happen when trims get snapped to
  // input m_t[] values that are within fuzz of the
  // output_domain[] values.
	m_t[0] = output_domain[0];
  m_t[m_t.Count()-1] = output_domain[1];

  return true;
}

bool ON_PolylineCurve::Extend(
  const ON_Interval& domain
  )

{
  if (IsClosed())
    return false;
  if (PointCount() < 2)
    return false;
  if ( !domain.IsIncreasing() )
    return false;
  bool changed = false;
  if ( domain == Domain() )
    return true;

  if (domain[0] < m_t[0]){
    changed = true;
    double len = m_t[1] - m_t[0];
    if ( len <= 0.0 )
      return false;
    ON_3dVector V = m_pline[1] - m_pline[0];
    ON_3dPoint Q0 = m_pline[0];
    Q0 += (domain[0]-m_t[0])/len*V;
    m_t[0] = domain[0];
    m_pline[0] = Q0;
  }

  int last = PointCount()-1;
  if (domain[1] > m_t[last]){
    changed = true;
    double len = m_t[last] - m_t[last-1];
    if ( len <= 0.0 )
      return false;
    ON_3dVector V = m_pline[last] - m_pline[last-1];
    ON_3dPoint Q1 = m_pline[last];
    Q1 += (domain[1]-m_t[last])/len*V;
    m_t[last] = domain[1];
    m_pline[last] = Q1;
  }

  if (changed){
    DestroyCurveTree();
  }
  return changed;
}



BOOL ON_PolylineCurve::Split(
    double t,
    ON_Curve*& left_side,
    ON_Curve*& right_side
  ) const
{
  BOOL rc = FALSE;
  ON_PolylineCurve* left_pl=0;
  ON_PolylineCurve* right_pl=0;
  if ( left_side ) {
    left_pl = ON_PolylineCurve::Cast(left_side);
    if (!left_pl)
      return FALSE;
  }
  if ( right_side ) {
    right_pl = ON_PolylineCurve::Cast(right_side);
    if (!right_pl)
      return FALSE;
  }
  const int count = m_t.Count()-1;

  if ( count >= 1 && m_t[0] < t && t < m_t[count] )
	{
/* March 26 2003 Greg Arden	 Use new function ParameterSearch() to snap parameter value
															when close to break point.

*/
		int segment_index;
		bool split_at_break = ParameterSearch(t, segment_index, true);


    if ( m_t[0] < t && t < m_t[count] )
    {
      int left_point_count = (split_at_break) ? segment_index+1 : segment_index+2;
      int right_point_count = m_t.Count() - segment_index;

      if ( left_pl != this )
      {
        if ( !left_pl )
          left_pl = new ON_PolylineCurve();
        left_pl->m_t.Reserve(left_point_count);
        left_pl->m_t.SetCount(left_point_count);
        left_pl->m_pline.Reserve(left_point_count);
        left_pl->m_pline.SetCount(left_point_count);
        memcpy( left_pl->m_t.Array(), m_t.Array(), left_point_count*sizeof(double) );
        memcpy( left_pl->m_pline.Array(), m_pline.Array(), left_point_count*sizeof(ON_3dPoint) );
				if(split_at_break){
					// reparameterize the last segment
					*left_pl->m_t.Last()= t;
				}
        left_pl->m_dim = m_dim;
      }
      if ( right_pl != this )
      {
        if ( !right_pl )
          right_pl = new ON_PolylineCurve();
        right_pl->m_t.Reserve(right_point_count);
        right_pl->m_t.SetCount(right_point_count);
        right_pl->m_pline.Reserve(right_point_count);
        right_pl->m_pline.SetCount(right_point_count);
        memcpy( right_pl->m_t.Array(),
							  m_t.Array() + m_t.Count() - right_point_count,
							  right_point_count*sizeof(double) );
        memcpy( right_pl->m_pline.Array(),
							  m_pline.Array() + m_pline.Count() - right_point_count,
							  right_point_count*sizeof(ON_3dPoint) );
				if( split_at_break)
					// Reparameterize the first segment
					right_pl->m_t[0] = t;
        right_pl->m_dim = m_dim;
      }
      left_pl->Trim( ON_Interval( left_pl->m_t[0], t ) );
      right_pl->Trim( ON_Interval( t, *right_pl->m_t.Last() ) );
		  rc = TRUE;
    }

	}


	left_side = left_pl;
	right_side = right_pl;
  return rc;
}


int ON_PolylineCurve::GetNurbForm(
                                  ON_NurbsCurve& nurb,
                                  double tol,
                                  const ON_Interval* subdomain  // OPTIONAL subdomain of ON::ProxyCurve::Domain()
                                  ) const
{
  int rc = 0;
  const int count = PointCount();
  if ( count < 2 )
    nurb.Destroy();
  else  if ( nurb.Create( Dimension(), FALSE, 2, count) ) {
    int i;
    for ( i = 0; i < count; i++ ) {
      nurb.SetKnot( i, m_t[i] );
      nurb.SetCV( i, m_pline[i] );
    }
    if ( subdomain && *subdomain != Domain() )
      nurb.Trim(*subdomain);
    if ( nurb.IsValid() )
      rc = 1;
  }
  return rc;
}

int ON_PolylineCurve::HasNurbForm() const
{
  if (PointCount() < 2)
    return 0;
  if (!IsValid())
    return 0;
  return 1;
}

BOOL ON_PolylineCurve::GetCurveParameterFromNurbFormParameter(
      double nurbs_t,
      double* curve_t
      ) const
{
  *curve_t = nurbs_t;
  return TRUE;
}

BOOL ON_PolylineCurve::GetNurbFormParameterFromCurveParameter(
      double curve_t,
      double* nurbs_t
      ) const
{
  *nurbs_t = curve_t;
  return TRUE;
}
