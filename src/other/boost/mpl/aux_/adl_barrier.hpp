
#ifndef BOOST_MPL_AUX_ADL_BARRIER_HPP_INCLUDED
#define BOOST_MPL_AUX_ADL_BARRIER_HPP_INCLUDED

// Copyright Aleksey Gurtovoy 2002-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id: adl_barrier.hpp 49239 2008-10-10 09:10:26Z agurtovoy $
// $Date: 2008-10-10 14:40:26 +0530 (Fri, 10 Oct 2008) $
// $Revision: 49239 $

#include <boost/mpl/aux_/config/adl.hpp>
#include <boost/mpl/aux_/config/gcc.hpp>
#include <boost/mpl/aux_/config/workaround.hpp>

#if !defined(BOOST_MPL_CFG_NO_ADL_BARRIER_NAMESPACE)

#   define BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE mpl_
#   define BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE_OPEN namespace mpl_ {
#   define BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE_CLOSE }
#   define BOOST_MPL_AUX_ADL_BARRIER_DECL(type) \
    namespace boost { namespace mpl { \
    using ::BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE::type; \
    } } \
/**/

#if !defined(BOOST_MPL_PREPROCESSING_MODE)
namespace BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE { namespace aux {} }
namespace boost { namespace mpl { using namespace BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE; 
namespace aux { using namespace BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE::aux; }
}}
#endif

#else // BOOST_MPL_CFG_NO_ADL_BARRIER_NAMESPACE

#   define BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE boost::mpl
#   define BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE_OPEN namespace boost { namespace mpl {
#   define BOOST_MPL_AUX_ADL_BARRIER_NAMESPACE_CLOSE }}
#   define BOOST_MPL_AUX_ADL_BARRIER_DECL(type) /**/

#endif

#endif // BOOST_MPL_AUX_ADL_BARRIER_HPP_INCLUDED
